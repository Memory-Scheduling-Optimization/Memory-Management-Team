#ifndef _VMM_H_
#define _VMM_H_

#include "stdint.h"

namespace VMM {

    extern uint32_t* kpd;
    
    // Called (on the initial core) to initialize data structures, etc
    extern void global_init();

    // Called on each core to do per-core initialization
    extern void per_core_init();

    extern void destroy_pd(uint32_t);
    extern void clear_pd(uint32_t*);
    extern uint32_t copy_kpd();
    extern uint32_t copy_pd(uint32_t*);    
    extern void walk_pd(uint32_t*);

    extern void write_protect(uint32_t*);    
}

#endif
