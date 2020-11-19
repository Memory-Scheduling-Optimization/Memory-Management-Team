#include "idt.h"
#include "stdint.h"
#include "debug.h"
#include "machine.h"
#include "smp.h"

extern uint32_t idt[];
extern uint32_t kernelCS;

void IDT::init(void) {
}

void IDT::interrupt(int index, uint32_t handler) {
    int i0 = 2 * index;
    int i1 = i0 + 1;

    idt[i0] = (kernelCS << 16) + (handler & 0xffff);
    idt[i1] = (handler & 0xffff0000) | 0x8e00;
}

void IDT::trap(int index, uint32_t handler, uint32_t dpl) {
    int i0 = 2 * index;
    int i1 = i0 + 1;

    idt[i0] = (kernelCS << 16) + (handler & 0xffff);
    idt[i1] = (handler & 0xffff0000) | 0x8f00 | (dpl << 13);
}
