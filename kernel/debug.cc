#include "debug.h"
#include "libk.h"
#include "machine.h"
#include "smp.h"
#include "config.h"
#include "kernel.h"
#include "atomic.h"

OutputStream<char> *Debug::sink = 0;
bool Debug::debugAll = false;
bool Debug::shutdown_called = false;
Atomic<uint32_t> Debug::checks{0};

void Debug::init(OutputStream<char> *sink) {
    Debug::sink = sink;
}

static InterruptSafeLock lock{};

void Debug::vprintf(const char* fmt, va_list ap) {
    if (sink) {
        lock.lock();
        K::vsnprintf(*sink,1000,fmt,ap);
        lock.unlock();
    }
}

void Debug::printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt,ap);
    va_end(ap);
}

void Debug::shutdown() {
    if (checks.get() > 0) {
        printf("*** passed %d checkes\n",checks.get());
    }
    printf("core %d requested shutdown\n",SMP::me());
    shutdown_called = true;
    while (true) {
        outb(0xf4,0x00);
	    asm volatile("pause");
    }
    //printf("| system shutdown\n");
    //while (true) asm volatile ("hlt");
}

void Debug::vpanic(const char* fmt, va_list ap) {
    lock.unlock(); // things are going bad, force unlock
    vprintf(fmt,ap);
    printf("| processor %d halting\n",SMP::me());
    shutdown_called = true;
    while (true) {
        outb(0xf4,0x00);
	asm volatile("pause");
    }
    //printf("| system shutdown\n");
    //while (true) asm volatile ("hlt");
}


void Debug::panic(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vpanic(fmt,ap);
    va_end(ap);
}

void Debug::vdebug(const char* fmt, va_list ap) {
    if (debugAll || flag) {
       printf("[%s] ",what);
       vprintf(fmt,ap);
       printf("\n");
    }
}

void Debug::debug(const char* fmt, ...) {
    if (debugAll || flag) {
        va_list ap;
        va_start(ap, fmt);
        vdebug(fmt,ap);
        va_end(ap);
    }
}

void Debug::missing(const char* file, int line) {
    panic("*** Missing code at %s:%d\n",file,line);
}


