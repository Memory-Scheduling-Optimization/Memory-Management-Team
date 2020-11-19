#include "physmem.h"
#include "blocking_lock.h"
#include "atomic.h"
#include "debug.h"
#include "atomic.h"
#include "idt.h"

namespace PhysMem {
    
    static InterruptSafeLock lock{};

    InterruptSafeLock reflock{};
    
    struct Frame {
        Frame* next;
    };

    static Frame* firstFree = nullptr;
    static uint32_t avail;
    static uint32_t limit;
    uint32_t start;

    uint32_t* refs;    
    
    uint32_t unsafe_alloc_frame() {
	LockGuard g{lock};

        uint32_t p;

        if (firstFree != nullptr) {
            p = (uint32_t) firstFree;
            firstFree = firstFree->next;
        } else {
            if (avail == limit) {
                Debug::panic("no more frames");
            }
            p = avail;
            avail += FRAME_SIZE;
        }

        ASSERT(offset(p) == 0);

        bzero((void*)p,FRAME_SIZE);
	
        return p;
    }    
    
    uint32_t alloc_frame() {
	uint32_t p = unsafe_alloc_frame();
	incref(p);
        return p;
    }

    void dealloc_frame(uint32_t p) {
        LockGuard g{lock};

        ASSERT(offset(p) == 0);

        if ((p<start) || (p >= limit)) {
            Debug::printf("| not freeing %x\n",p);
        }

        Frame* f = (Frame*) p;    
        f->next = firstFree;
        firstFree = f;
    }


    void init(uint32_t start_, uint32_t size) {
        start = start_;
        ASSERT(offset(start) == 0);
        ASSERT(offset(size) == 0);
        Debug::printf("| physical range 0x%x 0x%x\n",start,start+size);
        avail = start;
        limit = start + size;

	/* Initialize refs */
	refs = new uint32_t[((size-1) >> 23)+1]{0};
	
        /* register the page fault handler */
        IDT::trap(14,(uint32_t)pageFaultHandler_,3);
    }
    
};

