#include "vmm.h"
#include "debug.h"

#include "physmem.h"
#include "machine.h"

#include "smp.h"
#include "sys.h"

namespace VMM {

    // saving for later
    // https://tldp.org/LDP/tlk/ipc/ipc.html
    
    constexpr uint32_t PAGES_PER_TABLE = 1024;
    constexpr uint32_t PAGE_BYTES = 4096;   

    namespace Flag {
	constexpr uint32_t P	= 0x1;   // present
	constexpr uint32_t RW	= 0x2;   // writable
	constexpr uint32_t US	= 0x4;   // user-accessible
	constexpr uint32_t G	= 0x200; // global
	constexpr uint32_t PR	= 0x400; // private
	constexpr uint32_t NG   = 0x400; // non global
	constexpr uint32_t MASK = 0xfffff000;
    }    
    
    uint32_t* kpd; // kernel page directory

    void destroy_pg(uint32_t) {}
    
    void destroy_pt(uint32_t p) {
	using namespace PhysMem;
	uint32_t* pt = (uint32_t*) p;
	for (uint32_t i = 0; i < PAGES_PER_TABLE; i++)
	    if ((pt[i] & (Flag::NG | Flag::P)) == (Flag::NG | Flag::P))
		decref(pt[i] & Flag::MASK, destroy_pg);
    }

    void destroy_pd(uint32_t p) {
	using namespace PhysMem;
	ASSERT(p != getCR3());
	uint32_t* pd = (uint32_t*) p;
	for (uint32_t i = 0; i < PAGES_PER_TABLE; i++)
	    if ((pd[i] & (Flag::NG | Flag::P)) == (Flag::NG | Flag::P))
		decref(pd[i] & Flag::MASK, destroy_pt);
	decref(p, destroy_pg);
    }

    void clear_pt(uint32_t* pt) {
	using namespace PhysMem;
	bool discard;
	for (uint32_t i = 0; i < PAGES_PER_TABLE; i++) {
	    uint32_t pte = pt[i];

	    if (!(pte & Flag::P)) {
		continue;
	    }

	    if (!(pte & Flag::NG)) {
		discard = false;
		continue;
	    }
	    
	    decref(pte & Flag::MASK, destroy_pg);
	    pt[i] = 0;
	}
	ASSERT(!discard);
    }
    
    void clear_pd(uint32_t* pd) {
	using namespace PhysMem;
	for (uint32_t i = 0; i < PAGES_PER_TABLE; i++) {
	    uint32_t pde = pd[i];

	    if (!(pde & Flag::P)) {
		continue;
	    }

	    if (!(pde & Flag::NG)) {
		continue;
	    }

	    if (!(pde & Flag::G)) {
		decref(pde & Flag::MASK, destroy_pt);
		pd[i] = 0;
		continue;
	    }
	    
	    clear_pt((uint32_t*) (pde & Flag::MASK));
	}
	setCR3(getCR3());
    }
    
    uint32_t copy_kpd() {
	using namespace PhysMem;
	uint32_t* pd = (uint32_t*) alloc_frame();
	memcpy(pd, kpd, PAGE_BYTES);
	return (uint32_t) pd;
    }

    uint32_t copy_pg(uint32_t* pg) {
	using namespace PhysMem;
	uint32_t* new_pg = (uint32_t*) alloc_frame();
	memcpy(new_pg, pg, PAGE_BYTES);
	return (uint32_t) new_pg;
    }
    
    uint32_t copy_pt(uint32_t* pt) {
	using namespace PhysMem;
	uint32_t* new_pt = (uint32_t*) alloc_frame();
	for (uint32_t i = 0; i < PAGES_PER_TABLE; i++) {
	    uint32_t pte = pt[i];

	    // no entry
	    if (!(pte & Flag::P)) {
		continue;
	    }

	    // global entry
	    if (pte & Flag::G) {
		ASSERT(!(pte & Flag::NG));
		new_pt[i] = pte;
		continue;
	    }

	    // read-only entry
	    if (!(pte & Flag::RW)) {
		new_pt[i] = pte;
		incref(pte & Flag::MASK);
		continue;		
	    }

	    // non-global, read-write page
	    new_pt[i] = copy_pg((uint32_t*) (pte & Flag::MASK)) | (pte & 0xfff);
	}
	return (uint32_t) new_pt;
    }
    
    uint32_t copy_pd(uint32_t* pd) {
	using namespace PhysMem;
	uint32_t* new_pd = (uint32_t*) alloc_frame();
	for (uint32_t i = 0; i < PAGES_PER_TABLE; i++) {
	    uint32_t pde = pd[i];

	    // no entry
	    if (!(pde & Flag::P)) {
		continue;
	    }

	    // global page table
	    if (!(pde & Flag::NG)) {
		new_pd[i] = pde;
		continue;
	    }

	    // read-only page table
	    if (!(pde & Flag::RW)) {
		new_pd[i] = pde;
		incref(pde & Flag::MASK);
		continue;
	    }

	    // non-global, read-write page table
	    new_pd[i] = copy_pt((uint32_t*) (pde & Flag::MASK)) | (pde & 0xfff);
	}
	return (uint32_t) new_pd;
    }
    
    void walk_pd(uint32_t* pd) {
	using namespace PhysMem;
	for (uint32_t pdi = 0; pdi < PAGES_PER_TABLE; pdi++) {
	    uint32_t pde = pd[pdi];
	    if ((pde & (Flag::NG | Flag::P)) == (Flag::NG | Flag::P)) {		
		uint32_t* pt = (uint32_t*) (pde & Flag::MASK);
		Debug::printf("page table at 0x%x\n", pt);
		for (uint32_t pti = 0; pti < PAGES_PER_TABLE; pti++) {
		    uint32_t pte = pt[pti];
		    if ((pte & (Flag::NG | Flag::P)) == (Flag::NG | Flag::P))
			Debug::printf("page at 0x%x\n", pte & Flag::MASK);
		}
	    }
	}
    }
    
    void map(uint32_t* pd, uint32_t va, uint32_t pa, uint32_t flags) {
	using namespace PhysMem;
	uint32_t pdi = va >> 22;
	uint32_t pti = (va >> 12) & 0x3ff;

	uint32_t pde = pd[pdi];
	ASSERT((flags & 1));
	if (!(pde & 1))
	    pd[pdi] = alloc_frame() | flags;
	else if ((pde & flags) != flags) {
	    pd[pdi] = copy_pt((uint32_t*) (pde & Flag::MASK)) | (pde & 0xfff) | flags;
	    if (pde & Flag::NG) decref(pde & Flag::MASK, destroy_pt);
	}

	uint32_t* pt = (uint32_t*) (pd[pdi] & 0xfffff000);
	pt[pti] = pa | flags;
    }

    void map_copy(uint32_t* pd, uint32_t va, uint32_t pa, uint32_t flags) {
	using namespace PhysMem;
	uint32_t pdi = va >> 22;
	uint32_t pti = (va >> 12) & 0x3ff;

	uint32_t pde = pd[pdi];
	ASSERT((flags & 1));
	if (!(pde & 1))
	    pd[pdi] = alloc_frame() | flags;
	else if ((pde & flags) != flags) {
	    pd[pdi] = copy_pt((uint32_t*) (pde & Flag::MASK)) | (pde & 0xfff) | flags;
	    if (pde & Flag::NG) decref(pde & Flag::MASK, destroy_pt);
	}

	uint32_t* pt = (uint32_t*) (pd[pdi] & Flag::MASK);
	uint32_t pte = pt[pti];	
	if (pte & Flag::P) {
	    if (pte & Flag::RW) SYS::exit(-1);
	    memcpy((void*) pa, (void*) (pte & Flag::MASK), PAGE_BYTES);
	    decref(pte & Flag::MASK, destroy_pg);
	}
	pt[pti] = pa | flags;
    }   

    void write_protect(uint32_t* pd) {
	constexpr uint32_t pFlag = Flag::NG | Flag::RW | Flag::P;
	for (uint32_t pdi = 0; pdi < PAGES_PER_TABLE; pdi++) {
	    if ((pd[pdi] & pFlag) == pFlag) {
		pd[pdi] ^= Flag::RW;
		uint32_t* pt = (uint32_t*) (pd[pdi] & Flag::MASK);
		for (uint32_t pti = 0; pti < PAGES_PER_TABLE; pti++) {
		    if ((pt[pti] & pFlag) == pFlag) {
			pt[pti] ^= Flag::RW;
			invlpg((pdi << 22) | (pti << 12));
		    } else if ((pt[pti] & pFlag) == (Flag::RW | Flag::P))
			pd[pdi] |= Flag::RW;
		}		    
	    }
	}
    }
    
    void global_init() {
	using namespace PhysMem;
        kpd = (uint32_t*) unsafe_alloc_frame();

	constexpr uint32_t kFlags = Flag::G | Flag::RW | Flag::P;

	for (uint32_t pdi = 0; pdi < (kConfig.memSize >> 22); pdi++) {
	    uint32_t* pt = (uint32_t*) unsafe_alloc_frame();
	    for (uint32_t pti = 0; pti < PAGES_PER_TABLE; pti++)
		pt[pti] = (pdi << 22) | (pti << 12) | kFlags;
	    kpd[pdi] = (uint32_t) pt | kFlags;
	}

	((uint32_t*) (kpd[0] & Flag::MASK))[0] = 0;

	uint32_t va = kConfig.memSize & 0xffc00000;
	if (va < kConfig.memSize) {
	    kpd[(va >> 22)] = unsafe_alloc_frame() | kFlags;
	    for (; va < kConfig.memSize; va += PAGE_BYTES)
		map(kpd, va, va, kFlags);
	}

	map(kpd, kConfig.localAPIC, kConfig.localAPIC, kFlags);
	map(kpd, kConfig.ioAPIC, kConfig.ioAPIC, kFlags);
    }

    void per_core_init() {
	ASSERT(kpd);
	setWP();
//	setPGE();
	vmm_on((uint32_t) kpd);
    }
    
} /* namespace vmm */

extern "C" void vmm_pageFault(uintptr_t va_, uintptr_t *saveState) {
    using namespace VMM;
    
    // TODO mmap support
    // We need code to grab the current process so we can access its descriptors
    
    uint32_t error_code = saveState[8];
    
    if (va_ == 0)
	SYS::exit(-1);
    
    if ((error_code & 0x7) == 0x5)
	SYS::exit(-1);
    
    uint32_t flags = Flag::P;

    flags |= Flag::NG | Flag::RW;
    if (va_ >= 0x80000000) flags |= Flag::US;
    else SYS::exit(-1);

    if ((error_code & 0x3) == 0x3) {
	map_copy((uint32_t*) getCR3(), va_ & Flag::MASK, PhysMem::alloc_frame(), flags);
	invlpg(va_);
    } else if ((error_code & 0x1) == 0x1) {
	SYS::exit(-1);
    } else {
	map((uint32_t*) getCR3(), va_ & Flag::MASK, PhysMem::alloc_frame(), flags);
    }
}
