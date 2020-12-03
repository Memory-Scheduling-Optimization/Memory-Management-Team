#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"

/***********************************/
/* ASSUMPTIONS                     */
/* 1) Buddy block allocation       */
/***********************************/

/***********************************/
/* Global Variables and Structures */
/***********************************/

#define MAX_ORDER       22
#define MIN_ORDER       4   //2^4 = 16 bytes
/* the order ranges 0..MAX_ORDER, the largest memblock is 2^(MAX_ORDER) */
#define POOLSIZE        (1 << MAX_ORDER)
/* blocks are of size 2^i */
#define BLOCKSIZE(i)    (1 << (i))
/* the address of the buddy of a block from freelists[i]. */
#define _MEMBASE        ((uintptr_t)BUDDY->pool)
#define _OFFSET(b)      ((uintptr_t)b - _MEMBASE)
#define _BUDDYOF(b, i)  (_OFFSET(b) ^ (1 << (i)))
#define BUDDYOF(b, i)   ((void*)( _BUDDYOF(b, i) + _MEMBASE))
// not used yet, for higher order memory alignment
#define ROUND4(x)       ((x % 4) ? (x / 4 + 1) * 4 : x)

namespace rdanait {

        static BlockingLock *theLock = nullptr;

	typedef struct buddy {
  	    void* freelist[MAX_ORDER + 2];  // one more slot for first block in pool
            uint8_t pool[POOLSIZE];
        } buddy_t;

	buddy_t* BUDDY = 0;
};


/******************/
/* Heap Functions */
/******************/

void heapInit(void* start, size_t bytes) {

    using namespace rdanait;
    BUDDY->freelist[MAX_ORDER] = BUDDY->pool;
    theLock = new BlockingLock();
}

void* malloc(size_t bytes) {
    using namespace rdanait;
    LockGuardP g{theLock};

    int i, order;
    void* block;
    void* buddy;

    //calculate minimal order for this size
    i = 0;
    //one more byte for storing the order
    while ((size_t) BLOCKSIZE(i) < bytes + 1) i++;

    order = i = (i < MIN_ORDER) ? MIN_ORDER : i;

    //level up until non-null list found
    for (;; i++) {
        if (i > MAX_ORDER) return nullptr;
        if (BUDDY->freelist[i]) break;
    }

    //remove block from list
    block = BUDDY->freelist[i];
    BUDDY->freelist[i] = *(void**) BUDDY->freelist[i];
    
    //split until i = order
    while (i-- > order) {
        buddy = BUDDYOF(block, i);
	BUDDY->freelist[i] = buddy;
    }

    //store order in previous byte
    *((uint8_t*) (block) - 1) = order;
    return block;
}

void free(void* block) {

    using namespace rdanait;
    LockGuardP g{theLock};

    int i;
    void* buddy;
    void** p;

    //fetch order in previous byte
    i = *((uint8_t*) (block) - 1);

    for (;; i++) {
	buddy = BUDDYOF(block, i);
	p = &(BUDDY->freelist[i]);
	//find buddy in the list
	while ((*p != nullptr) && (*p != buddy)) p = (void**) *p;
	//if not found, insert into list
	if (*p != buddy) {
	    *(void**) block = BUDDY->freelist[i];
	    BUDDY->freelist[i] = block;
	    return;
	}
	else {
	    //found, merge block starts from the lower one
	    block = (block < buddy) ? block : buddy;
	    //remove buddy from list
	    *p = *(void**) *p;
	}
    }

}

int count_blocks(int i) {
    using namespace rdanait;
    int count = 0;
    void** p = &(BUDDY->freelist[i]);

    while (*p != nullptr) {
	count++;
	p = (void**) *p;
    }	

    return count;
}

int spaceUnallocated() {
    using namespace rdanait;
    int totSize = 0;
    for(int i = 0; i <= MAX_ORDER; i++) {
	totSize += count_blocks(i) * BLOCKSIZE(i);
    }

    return totSize;
}

/*****************/
/* C++ operators */
/*****************/

void* operator new(size_t size) {
    void* p =  malloc(size);
    if (p == 0) Debug::panic("out of memory");
    return p;
}

void operator delete(void* p) noexcept {
    return free(p);
}

void operator delete(void* p, size_t sz) {
    return free(p);
}

void* operator new[](size_t size) {
    void* p =  malloc(size);
    if (p == 0) Debug::panic("out of memory");
    return p;
}

void operator delete[](void* p) noexcept {
    return free(p);
}

void operator delete[](void* p, size_t sz) {
    return free(p);
}
