#include "sys.h"
#include "stdint.h"
#include "idt.h"
#include "debug.h"
#include "process.h"
#include "threads.h"
#include "semaphore.h"
#include "future.h"
#include "descriptor.h"
#include "elf.h"
#include "libk.h"

using namespace Descriptor;

class FileDescriptor : public FD {
    Shared<Node> node;
    uint32_t offset = 0;
public:
    FileDescriptor(Shared<Node> node) : node{node} {}
    
    int len() override {
	return node->size_in_bytes();
    }

    int read(char* buffer, int len) override {
	if (offset > node->size_in_bytes())
	    return -1;
	auto cnt = node->read_all(offset, len, buffer);
	offset += cnt;
	return cnt;	
    }

    int seek(int offset) override {
	return this->offset = offset;
    }
};

class ProcessDescriptor : public PD {
    Shared<Future<int>> exit_status;
public:
    ProcessDescriptor(Shared<Future<int>> exit_status) :
	exit_status{exit_status} {}
    
    int wait(uint32_t* status) override {
	*status = exit_status->get();
	return 0;
    }
};

class SemaphoreDescriptor : public SD {
    Semaphore sem;
public:
    SemaphoreDescriptor(uint32_t count) : sem{count} {}

    int up() override {
	sem.up();
	return 0;
    }

    int down() override {
	sem.down();
	return 0;
    }
};

namespace SYS {

    inline bool check_range(uint32_t start, uint32_t size) {
	uint32_t lower = start;
	uint32_t upper = start+size;
	return (lower <= upper)							&&
	    (lower >= 0x80000000)						&&
	    (upper <= kConfig.localAPIC || lower >= kConfig.localAPIC+4096)	&&
	    (upper <= kConfig.ioAPIC    || lower >= kConfig.ioAPIC+4096);
    }
    
#define GEN(fun) int fun(Shared<PCB>& pcb, uint32_t* stack)

    inline uint32_t* getargs(uint32_t* stack) {
	return (uint32_t*) stack[3] + 1;
    }   

    GEN(exit) {
	auto args = getargs(stack);
	pcb->process()->exit_status->set(args[0]);
	pcb.~Shared<PCB>();
	stop();
	return -1;
    }

    GEN(write) {
	auto args = getargs(stack);
	if (!check_range(args[1], args[2])) return -1;
	return pcb->process()->get_fd(args[0])->write((char*) args[1], (int) args[2]);
    }

    GEN(fork) {
	return pcb->process()
	    ->set_pd([=] {
			 auto p = pcb->process()->clone();
			 uint32_t pc = stack[0];
			 uint32_t esp = stack[3];
			 thread(p, [=] { switchToUser(pc, esp, 0); });
			 return Shared<PD>{
			     new ProcessDescriptor{p->process()->exit_status}};
		     });
    }

    GEN(sem) {
	auto args = getargs(stack);
	return pcb->process()
	    ->set_sd([=] { return Shared<SD>{new SemaphoreDescriptor{args[0]}}; });
    }

    GEN(up) {
	auto args = getargs(stack);
	return pcb->process()->get_sd(args[0])->up();
    }

    GEN(down) {
	auto args = getargs(stack);
	return pcb->process()->get_sd(args[0])->down();
    }

    GEN(close) {
	auto args = getargs(stack);
	return pcb->process()->close(args[0]);
    }

    GEN(shutdown) {
	pcb.~Shared<PCB>();
	Debug::shutdown();	
	return -1;
    }

    GEN(wait) {
	auto args = getargs(stack);
	return pcb->process()->wait(args[0], (uint32_t*) args[1]);
    }

    GEN(execl) {
	auto args = getargs(stack);
	auto proc = pcb->process();
	auto fs = proc->fs;
	Shared<Node> file = fs->open(proc->cd, (const char*) args[0]);
	if (file == nullptr) return -1;
	return exec(pcb, file, (const char**) &args[1]);
    }

    GEN(open) {
	auto args = getargs(stack);
	auto proc = pcb->process();
	auto fs = proc->fs;
	Shared<Node> file = fs->open(proc->cd, (const char*) args[0]);
	if (file == nullptr) return -1;
	return proc->set_fd([=] { return Shared<FD>{new FileDescriptor{file}}; });
    }

    GEN(len) {
	auto args = getargs(stack);
	return pcb->process()->get_fd(args[0])->len();	
    }
    
    GEN(read) {
	auto args = getargs(stack);
	if (!check_range(args[1], args[2])) return -1;
	return pcb->process()->get_fd(args[0])->read((char*) args[1], (int) args[2]);
    }
    
    GEN(seek) {
	auto args = getargs(stack);
	return pcb->process()->get_fd(args[0])->seek((int) args[1]);
    }

#undef GEN
}

typedef int (*syscall)(Shared<PCB>&, uint32_t*);
syscall* syscall_table;

static constexpr uint32_t NUM_SYSCALLS = 14;


extern "C" int sysHandler(uint32_t num, uint32_t stack) {
    if (num >= NUM_SYSCALLS) return -1;
    auto pcb = gheith::current()->pcb;
    return syscall_table[num](pcb, (uint32_t*) stack);
}   

static uint32_t user_stack;

void SYS::init(void) {

    // Initalize syscall table
    syscall_table = new syscall[NUM_SYSCALLS];
    syscall_table[0] = exit;
    syscall_table[1] = write;
    syscall_table[2] = fork;
    syscall_table[3] = sem;
    syscall_table[4] = up;    
    syscall_table[5] = down;
    syscall_table[6] = close;
    syscall_table[7] = shutdown;    
    syscall_table[8] = wait;
    syscall_table[9] = execl;
    syscall_table[10] = open;
    syscall_table[11] = len;
    syscall_table[12] = read;
    syscall_table[13] = seek;
    
    user_stack = (kConfig.localAPIC < kConfig.ioAPIC) ?
	kConfig.localAPIC :
	kConfig.ioAPIC;				     
    
    // Register syscall handler
    IDT::trap(48,(uint32_t)sysHandler_,3);
}

int exec(Shared<PCB>& pcb,
	 Shared<Node>& file,
	 uint32_t argc,
	 const char** argv,
	 uint32_t sz) {
    
    ELF::Header eh;
    if (!ELF::read_header(file, eh))
	return -1;
    
    const uint32_t offset = (argc+1) * sizeof(uint32_t);
    sz += offset;
    char* buffer = new char[sz];
    char* string = buffer + offset;
    for (uint32_t i = 0; i < argc; i++) {
	uint32_t len = K::strlen(argv[i])+1;
	memcpy(string, argv[i], len);
	((uint32_t*) buffer)[i] = (uint32_t) string - (uint32_t) buffer;
	string += len;
    }
    ((uint32_t*) buffer)[argc] = 0;
    
    ASSERT(pcb->cr3 == getCR3());
    pcb->process()->reset();

    auto entry = ELF::load(file, eh);

    uint32_t esp = user_stack;
    esp = ((esp-sz-8) & 0xfffffff0)+8;
    memcpy((void*) esp, buffer, sz);
    for (uint32_t i = 0; i < argc; i++)
	((uint32_t*) esp)[i] += esp;

    ((uint32_t*) esp)[-2] = argc;
    ((uint32_t*) esp)[-1] = esp;

    delete[] buffer;
    pcb.~Shared<PCB>();
    file.~Shared<Node>();
    switchToUser(entry, esp-8, 0);
    return -1;    
}

int SYS::exec(Shared<PCB>& pcb, Shared<Node>& file, const char** argv) {
    uint32_t argc = 0;
    uint32_t sz = 0;
    for (;; argc++) {
	const char* str = argv[argc];
	if (str) sz += K::strlen(str)+1;
	else break;
    }
    return exec(pcb, file, argc, argv, sz);
}

int SYS::exec(Shared<Node>& file, const char* arg, ...) {
    auto pcb = gheith::current()->pcb;
    return exec(pcb, file, &arg);
}

int SYS::exit(int status) {
    auto pcb = [] { return gheith::current()->pcb; }();
    pcb->process()->exit_status->set(status);
    pcb.~Shared<PCB>();
    stop();
    return -1;
}
