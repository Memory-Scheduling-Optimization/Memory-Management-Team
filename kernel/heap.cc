#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"

/* A first-fit heap */


namespace gheith {

static BlockingLock *theLock = nullptr;

    struct Header;
    struct Footer;

    #define ALIGNMENT 4
    #define WSIZE	4
    #define DSIZE	8
    #define MINSIZE 16
    #define HEADSIZE 12
    #define SEGREGATEDLENGTH 20
    
    #define MAX(x, y)	((x) > (y)? (x) : (y))
    #define PACK(size, alloc)	((size) | (alloc))
    #define PUT(p, val)	(*(unsigned int *)(p) = (val))
    /* rounds up to the nearest multiple of ALIGNMENT */
    #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x3)
    #define HDRP(bp)	((char *)(bp) - WSIZE)

    struct Footer {
        uint32_t size_and_state;

        inline bool is_allocated() {
            return size_and_state & 0x1;
        }

        inline uint32_t get_size() {
            return size_and_state & ~0x1;
        }
    };

    struct Header {
        uint32_t size_and_state;
        Header* prev_avail;
        Header* next_avail;

        inline bool is_allocated() {
            return size_and_state & 0x1;
        }

        inline uint32_t get_size() {
            return size_and_state & ~0x1;
        }

        inline Footer* get_footer() {
            return (Footer*)((char*)this + get_size() - WSIZE);
        }

        inline Header* get_next() {
            return (Header*)((char*)this + get_size());
        }

        inline Header* get_prev() {
            Footer* prev_foot = (Footer*)((char*)this - WSIZE);
            uint32_t prev_size = prev_foot->get_size();
            return (Header*)((char*)this - prev_size);
        }
    };

    // Segregated Free list set at 20 blocks
    Header** freeList;
    char* heapstart; // Used for debugging
    char* endheap; // Used for case malloc (0)

};

void mm_check();
void add_free(void* hp, size_t size);

void heapInit(void* base, size_t bytes) {
    using namespace gheith;

    Debug::printf("| heap range 0x%x 0x%x\n",(uint32_t)base,(uint32_t)base+bytes);

    char* start1 = (char*) base;

    // Set start of heap to base
    heapstart = start1;
    freeList = (Header**) base;

    // Set end of heap
    endheap = start1 + bytes;

    // Reserve first 80 bytes for segregated free list, empty out
    for(int i = 0; i < SEGREGATEDLENGTH; i++) {
        freeList[i] = nullptr;
    }

    // Set up prologue
    start1 += SEGREGATEDLENGTH * 4;
    Header* prologue = (Header*)start1;
    prologue->size_and_state = PACK(WSIZE, 1);

    // Set up free block
    start1 += WSIZE;
    Header* freestart = (Header*)start1;
    size_t freesize = bytes - 80 - DSIZE;
    freestart->size_and_state = PACK(freesize, 0);
    freestart->prev_avail = nullptr;
    freestart->next_avail = nullptr;

    Footer* freefoot = freestart->get_footer();
    freefoot->size_and_state = PACK(freesize, 0);

    // Add first free block to free list
    add_free(freestart, freesize);

    // Set up epilogue
    start1 += freesize;
    Header* epilogue = (Header*)start1;
    epilogue->size_and_state = PACK(WSIZE, 1);

    // mm_check();

    theLock = new BlockingLock();
}

// Adapted
void add_helper(void* hp, size_t index) {
    using namespace gheith;
    Header* headptr = (Header*)hp;
    // if no blocks in free list
    if (freeList[index] == nullptr) {
        headptr->next_avail = nullptr;
        headptr->prev_avail = nullptr;
        freeList[index] = headptr;
    }
    // blocks exist shift blocks to add
    else {
        freeList[index]->prev_avail = headptr;
        headptr->next_avail = freeList[index];
        headptr->prev_avail = nullptr;
        freeList[index] = headptr;
    }
}

// Adapted
void add_free(void* hp, size_t size) {\
    using namespace gheith;
    int free_index = 0;
    while(size >= (size_t)(2 << (free_index + 4))) {
        // Debug::printf("%x\t%d\n", free_index, 2 << (free_index + 4));
        free_index++;
    }

    // Debug printing
    // Debug::printf("size: %x\tindex: %d\tsize range: %x\n", size, free_index, 2 << (free_index + 4));
    ASSERT(free_index < SEGREGATEDLENGTH);
    
    add_helper(hp, free_index);
}

void remove_from_free_list(void* hp) {
    using namespace ericchen;
    Header* headptr = (Header*)hp;

    if(freeList == headptr) {
        // First free node
        freeList = headptr->next_avail;
    } else if (headptr->next_avail == nullptr) {
        // End node
        headptr->prev_avail->next_avail = nullptr;
    } else {
        // Middle node
        // set previous' next to hp's next
        headptr->prev_avail->next_avail = headptr->next_avail;
        // set next's previous to bp's previous
        headptr->next_avail->prev_avail = headptr->prev_avail;
    }
}

// Adapted
void* coalesce(void *hp) {
    using namespace gheith;
    Header* currptr = (Header*)hp;
    Header* prevptr = currptr->get_prev();
    Header* nextptr = currptr->get_next();
    bool prev_alloc = prevptr->is_allocated();
    bool next_alloc = nextptr->is_allocated();

    size_t size = currptr->get_size();

    if (prev_alloc && next_alloc) {
        // Both alloc, do nothing
    } else if (prev_alloc && !next_alloc) {
        // Next block free, coalesce
        remove_from_free_list(nextptr);
        size += nextptr->get_size();
        nextptr->get_footer()->size_and_state = PACK(size,0);
        currptr->size_and_state = PACK(size,0);
    } else if (!prev_alloc && next_alloc) {
        // Previous block free, coalesce
        remove_from_free_list(prevptr);
        size += prevptr->get_size();
        prevptr->size_and_state = PACK(size,0);
        currptr->get_footer()->size_and_state = PACK(size,0);
        hp = prevptr;
    } else {
        // Prev and next blocks free, coalesce
        remove_from_free_list(prevptr);
        remove_from_free_list(nextptr);
        size += prevptr->get_size() + nextptr->get_size();
        prevptr->size_and_state = PACK(size,0);
        nextptr->get_footer()->size_and_state = PACK(size,0);
        hp = prevptr;
    }
    // Add combined free block into free list
    add_free(hp, size);
    return nullptr;
}

void* find_fit(size_t asize) {
    using namespace ericchen;
    int index = 0;
    while
    // Header* findhead = freeList;
    // Header* finalhead = nullptr;
    // while (findhead != nullptr) {
    //     if (findhead->get_size() >= asize) 
    //     // Find free block that fits best fit
    //     {
    //         if (finalhead == nullptr || findhead->get_size() < finalhead->get_size())
    //             finalhead = findhead;
    //     }
    //     findhead = findhead->next_avail;
    // }
    // return finalhead;
}

// Adapted
void place(void* hp, size_t asize) {
    using namespace gheith;
    Header* placehead = (Header*)hp;
    size_t csize = placehead->get_size();

    /* Free block able to fit another free block after allocation */
    if ((csize - asize) >= MINSIZE)
    {
        remove_from_free_list(hp);
        placehead->size_and_state = PACK(asize, 1);
        placehead->get_footer()->size_and_state = PACK(asize, 1);

        /* Add leftover free memory into free list */
        Header* newhead = placehead->get_next();
        newhead->size_and_state = PACK(csize-asize, 0);
        newhead->get_footer()->size_and_state = PACK(csize-asize, 0); //Problem
        add_free(newhead, csize-asize);
    }
    /* allocate entire block due to lack of space */
    else {
        placehead->size_and_state = PACK(csize, 1);
        placehead->get_footer()->size_and_state = PACK(csize, 1);
        remove_from_free_list(hp);
    }
}

void* malloc(size_t bytes) {
    using namespace gheith;
    //Debug::printf("malloc(%d)\n",bytes);
    // if (bytes == 0) return (void*) array;
    // if (bytes == 0) return endheap;

    // LockGuardP g{theLock};

    // char *hp;

    // size_t asize = MAX(MINSIZE, ALIGN(bytes + DSIZE));

    // if ((hp = (char*)find_fit(asize)) != nullptr) {
    //     place(hp, asize);
    // }

    // // mm_check();

    // if(hp == nullptr)
    //     return nullptr;
    // return hp + WSIZE;
    return nullptr;

}

void free(void* p) {
    using namespace gheith;
    if (p == 0) return;
    // if (p == (void*) array) return;

    LockGuardP g{theLock};

    // int idx = ((((uintptr_t) p) - ((uintptr_t) array)) / 4) - 1;
    // sanity(idx);
    // if (!isTaken(idx)) {
    //     Debug::panic("freeing free block, p:%x idx:%d\n",(uint32_t) p,(int32_t) idx);
    //     return;
    // }

    // int sz = size(idx);

    // int leftIndex = left(idx);
    // int rightIndex = right(idx);

    // if (isAvail(leftIndex)) {
    //     remove(leftIndex);
    //     idx = leftIndex;
    //     sz += size(leftIndex);
    // }

    // if (isAvail(rightIndex)) {
    //     remove(rightIndex);
    //     sz += size(rightIndex);
    // }

    // makeAvail(idx,sz);
}

void mm_check() {
    // using namespace gheith;
    // Debug::printf("\nDEBUGGING\n");
    // // Free List check
    // if (freeList != nullptr) {
    // Header* freelisttemp = freeList;
    // int count = 0;
    // while(freelisttemp != nullptr)
    // {
    //     if(freelisttemp->is_allocated())
    //         Debug::printf("Free List node allocated\n");
    //     Debug::printf("Block size of %x at %x\n", freelisttemp->get_size(), freelisttemp);
    //     count++;
    //     freelisttemp = freelisttemp->next_avail;
    // }
    // Debug::printf("%d Free blocks\n", count);
    // } else
    //     Debug::printf("FREE LIST EMPTY\n");

    // // Heap checker
    // Header* check = (Header*)heapstart;
    // while (check != (Header*)endheap) {
    //     if(check->get_size() != check->get_footer()->get_size()) {
    //         Debug::printf("Size inconsistency at: %x\n", check);
    //         Debug::printf("Head: %x\tFoot:%x\n", check->get_size(), check->get_footer()->get_size());
    //     }
    //     if(check->is_allocated() != check->get_footer()->is_allocated()) {
    //         Debug::printf("Alloc inconsistency\n");
    //     }
    //     if(!check->get_prev()->is_allocated() && !check->is_allocated()) { //Coalesce fail
    //         Debug::printf("Coalesce failure %x and %x\n", check->get_prev(), check);
    //     }
    //     check = check->get_next();
    // }

    // Debug::printf("\n");
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
