#ifndef _MACHINE_H_
#define _MACHINE_H_

#include "stdint.h"

extern "C" void resetEIP(void);

extern "C" int inb(int port);
extern "C" int inl(int port);
extern "C" void outb(int port, int val);
extern "C" void outl(int port, int val);

extern "C" uint64_t rdmsr(uint32_t id);
extern "C" void wrmsr(uint32_t id, uint64_t value);

extern "C" void vmm_on(uint32_t pd);
extern "C" void invlpg(uint32_t va);

extern "C" void apitHandler_(void);
extern "C" void spuriousHandler_(void);
extern "C" void pageFaultHandler_(void);

extern "C" void* memcpy(void *dest, const void* src, size_t n);
extern "C" void* bzero(void *dest, size_t n);

extern "C" void sti();
extern "C" void cli();
extern "C" uint32_t getCR3();
extern "C" uint32_t getFlags();
extern "C" void monitor(uintptr_t);
extern "C" void mwait();

struct cpuid_out {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
};

extern "C" void cpuid(uint32_t eax, cpuid_out* out);

//extern bool disable();
//extern void enable(bool wasDisabled);

extern void pause();

extern "C" void switchToUser(uint32_t pc, uint32_t esp, uint32_t eax);

extern "C" void ltr(uint32_t);

extern uint32_t tssDescriptorBase;
extern uint32_t kernelSS;

extern "C" void sysHandler_(void);

/* Extras */

extern "C" void setWP();
extern "C" void setCR3(uint32_t pd);
extern "C" void setPGE();

#endif
