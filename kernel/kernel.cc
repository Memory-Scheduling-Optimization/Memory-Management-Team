#include "stdint.h"
#include "debug.h"

#include "shared.h"
#include "threads.h"
#include "ext2.h"
#include "elf.h"
#include "vmm.h"
#include "process.h"
#include "barrier.h"
#include "sys.h"

const char* initName = "/sbin/init";

void printFile(Shared<Node> file) {
    if (file == nullptr) {
	Debug::printf("file does not exist\n");
	return;
    }
    auto buffer = new char[file->size_in_bytes()];
    file->read_all(0, file->size_in_bytes(), buffer);
    for (uint32_t i = 0; i < file->size_in_bytes(); i++)
	Debug::printf("%c", buffer[i]);
    delete[] buffer;
}

void kernelMain(void) {
    {
	auto ide = Shared<Ide>::make(1);
	auto fs = Shared<Ext2>::make(ide);	
	auto init = fs->open(fs->root, initName);
	auto pcb = Shared<PCB>{new Process(fs)};
	thread(pcb, [=]() mutable { SYS::exec(init, "init", 0); });
    }
    // Debug::printf("init exited with status %d\n",
    // 		  pcb->process()->exit_status->get());
    stop();
}

