#ifndef _INIT_H_
#define _INIT_H_

#include "stdint.h"

extern "C" void kernelInit(void);
extern "C" uint32_t kernelPickStack(void);

extern bool onHypervisor;

#endif
