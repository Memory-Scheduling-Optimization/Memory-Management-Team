#ifndef _process_h_
#define _process_h_

#include "shared.h"
#include "pcb.h"
#include "future.h"
#include "ext2.h"
#include "descriptor.h"
#include "vmm.h"
#include "io.h"
#include "u8250.h"

struct Process : public PCB {
    static constexpr uint32_t FDT_SIZE = 10;
    static constexpr uint32_t PDT_SIZE = 10;
    static constexpr uint32_t SDT_SIZE = 10;
    
    Shared<Future<int>> exit_status;
    Shared<Ext2> fs;
    Shared<Node> cd;
    Shared<OutputStream<char>> io;
    Shared<Descriptor::FD> fdt[FDT_SIZE];
    Shared<Descriptor::PD> pdt[PDT_SIZE];
    Shared<Descriptor::SD> sdt[SDT_SIZE];

    Process(Shared<Ext2> fs) :
	PCB(VMM::copy_kpd()),
	exit_status(new Future<int>()),
	fs(fs),
	cd(fs->root),
	io(new U8250()) {
	using namespace Descriptor;
	fdt[0] = Shared<FD>{new StdIn{}};
	fdt[1] = Shared<FD>{new StdOut{io}};
	fdt[2] = Shared<FD>{new StdErr{io}};	
	for (uint32_t i = 3; i < FDT_SIZE; i++)
	    fdt[i] = FD::empty;	
	for (uint32_t i = 0; i < PDT_SIZE; i++)
	    pdt[i] = PD::empty;
	for (uint32_t i = 0; i < SDT_SIZE; i++)
	    sdt[i] = SD::empty;
    }

    ~Process() {
	ASSERT(cr3 != (uint32_t) VMM::kpd);
	VMM::destroy_pd(cr3);
    }

#define GEN(n1, n2)					\
    Shared<Descriptor::n2> get_##n1(uint32_t des) {	\
	using namespace Descriptor;			\
	if ((des & TYPE_MASK) != TYPE_##n2)		\
	    return n2::empty;				\
	uint32_t index = des & NUM_MASK;		\
	if (index >= n2##T_SIZE)			\
	    return n2::empty;				\
	return n1##t[index];				\
    }							\
							\
    template<typename F>				\
    int set_##n1(F fun) {				\
	using namespace Descriptor;			\
	for (uint32_t i = 0; i < n2##T_SIZE; i++)	\
	    if (n1##t[i] == n2::empty) {		\
		n1##t[i] = fun();			\
		return i | TYPE_##n2;			\
	    }						\
	return -1;					\
    }

    GEN(fd, FD);
    GEN(pd, PD);
    GEN(sd, SD);
#undef GEN    
    
    int close(uint32_t); 
    int wait(uint32_t, uint32_t*);    
    
    Process* process() override {
	return this;
    }    

    void reset();
    Shared<PCB> clone();
    
private:
    Process(const Process& p) :
	PCB(VMM::copy_pd((uint32_t*) p.cr3)),
	exit_status(new Future<int>),
	fs(p.fs),
	cd(p.cd),
	io(p.io),
	fdt(p.fdt),
	sdt(p.sdt) {
	for (uint32_t i = 0; i < PDT_SIZE; i++)
	    pdt[i] = Descriptor::PD::empty;
    }
};

#endif
