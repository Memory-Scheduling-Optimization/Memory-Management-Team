#ifndef _pcb_h_
#define _pcb_h_

#include "shared.h"

struct Process;

struct PCB : public Sharable<PCB> {
    static Atomic<uint32_t> next_id;

    const uint32_t id;
    const uint32_t cr3;
    uint32_t esp0;

    PCB(uint32_t cr3) : id{next_id}, cr3{cr3} {}

    virtual ~PCB() {}
    
    virtual Process* process() {
	return nullptr;
    }
};

extern Shared<PCB> kProc;

#endif
