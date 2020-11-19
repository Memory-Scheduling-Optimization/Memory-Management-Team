#ifndef _physmem_h_
#define _physmem_h_

#include "stdint.h"
#include "atomic.h"
#include "debug.h"

namespace PhysMem {
    constexpr uint32_t FRAME_SIZE = 1 << 12;

    void init(uint32_t start, uint32_t size);

    inline uint32_t offset(uint32_t pa) {
        return pa & 0xFFF;
    }

    inline uint32_t ppn(uint32_t pa) {
        return pa >> 12;
    }

    inline uint32_t framedown(uint32_t pa) {
        return (pa / FRAME_SIZE) * FRAME_SIZE;
    }

    inline uint32_t frameup(uint32_t pa) {
        return framedown(pa + FRAME_SIZE - 1);
    }

    uint32_t unsafe_alloc_frame();
    
    uint32_t alloc_frame();

    void dealloc_frame(uint32_t);

    extern InterruptSafeLock reflock;
    extern uint32_t* refs;
    extern uint32_t start;
    
    inline void incref(uint32_t p) {	
	ASSERT(offset(p) == 0);
	uint32_t index = (p - start) >> 12;
	uint32_t ridx = index >> 11;
	uint32_t rofs = index & 0x7ff;
	reflock.lock();
	if (!(refs[ridx] & 0xfff))
	    refs[ridx] = unsafe_alloc_frame();
	reflock.unlock();
	uint16_t* rtb = (uint16_t*) (refs[ridx] & 0xfffff000);
	if (!__atomic_fetch_add(&rtb[rofs], 1, __ATOMIC_ACQ_REL))
	    __atomic_add_fetch(&refs[ridx], 1, __ATOMIC_ACQ_REL);
    }

    template<typename F>
    inline void decref(uint32_t p, const F& f) {
	ASSERT(offset(p) == 0);
	uint32_t index = (p - start) >> 12;
	uint32_t ridx = index >> 11;
	uint32_t rofs = index & 0x7ff;
	uint32_t rent = refs[ridx];
	ASSERT(rent & 0xfff);
	uint16_t* rtb = (uint16_t*) (rent & 0xfffff000);
	uint16_t val = __atomic_sub_fetch(&rtb[rofs], 1, __ATOMIC_ACQ_REL);
	if (!val) {
	    f(p);
	    dealloc_frame(p);
	    rent = __atomic_sub_fetch(&refs[ridx], 1, __ATOMIC_ACQ_REL);
	    if (!(rent & 0xfff))
		dealloc_frame(rent & 0xfffff000);
	}
    }
}

#endif
