#ifndef _IDT_H_
#define _IDT_H_

#include "stdint.h"

class IDT {
public:
    static void init(void);
    static void interrupt(int index, uint32_t handler);
    static void trap(int index, uint32_t handler, uint32_t dpl);
};

#endif
