#include "atomic.h"
#include "machine.h"

#include "debug.h"

void pause() {
    if (Debug::shutdown_called) {
        Debug::panic("shutdown called\n");
    }
    asm volatile("pause");
}



