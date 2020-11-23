#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"

/* A first-fit heap */

static constexpr uint32_t HEAP_HEAP_START = 1 * 1024 * 1024;
static constexpr uint32_t HEAP_HEAP_SIZE = 5 * 1024 * 1024;
static constexpr uint32_t ARENA_SIZE = 16 * 1024;

namespace gheith {
    
// static int *array;
// static int len;
// static int safe = 0;
// static int avail = 0;
// static BlockingLock *theLock = nullptr;

// void makeTaken(int i, int ints);
// void makeAvail(int i, int ints);

// int abs(int x) {
//     if (x < 0) return -x; else return x;
// }

// int size(int i) {
//     return abs(array[i]);
// }

// int headerFromFooter(int i) {
//     return i - size(i) + 1;
// }

// int footerFromHeader(int i) {
//     return i + size(i) - 1;
// }
    
// int sanity(int i) {
//     if (safe) {
//         if (i == 0) return 0;
//         if ((i < 0) || (i >= len)) {
//             Debug::panic("bad header index %d\n",i);
//             return i;
//         }
//         int footer = footerFromHeader(i);
//         if ((footer < 0) || (footer >= len)) {
//             Debug::panic("bad footer index %d\n",footer);
//             return i;
//         }
//         int hv = array[i];
//         int fv = array[footer];
  
//         if (hv != fv) {
//             Debug::panic("bad block at %d, hv:%d fv:%d\n", i,hv,fv);
//             return i;
//         }
//     }

//     return i;
// }

// int left(int i) {
//     return sanity(headerFromFooter(i-1));
// }

// int right(int i) {
//     return sanity(i + size(i));
// }

// int next(int i) {
//     return sanity(array[i+1]);
// }

// int prev(int i) {
//     return sanity(array[i+2]);
// }

// void setNext(int i, int x) {
//     array[i+1] = x;
// }

// void setPrev(int i, int x) {
//     array[i+2] = x;
// }

// void remove(int i) {
//     int prevIndex = prev(i);
//     int nextIndex = next(i);

//     if (prevIndex == 0) {
//         /* at head */
//         avail = nextIndex;
//     } else {
//         /* in the middle */
//         setNext(prevIndex,nextIndex);
//     }
//     if (nextIndex != 0) {
//         setPrev(nextIndex,prevIndex);
//     }
// }

// void makeAvail(int i, int ints) {
//     array[i] = ints;
//     array[footerFromHeader(i)] = ints;    
//     setNext(i,avail);
//     setPrev(i,0);
//     if (avail != 0) {
//         setPrev(avail,i);
//     }
//     avail = i;
// }

// void makeTaken(int i, int ints) {
//     array[i] = -ints;
//     array[footerFromHeader(i)] = -ints;    
// }

// int isAvail(int i) {
//     return array[i] > 0;
// }

// int isTaken(int i) {
//     return array[i] < 0;
// }

// Garbage code wohoo
// void makeFreeBlock(void* base, size_t bytes) {
//     uint32_t* temp = (uint32_t*) base;
//     temp[0] = bytes;
//     temp[(bytes/4) - 1] = bytes;
//     Debug::printf("Free block: %x\t%x\n", &temp[0], &temp[(bytes/4) - 1]);
// }

// Start of free list
static int* free_start = nullptr;
// End of free list, need to add at end for when we have partially filled block
static int* free_end = nullptr;
static BlockingLock *theLock = nullptr;

struct Header {
    int num_allocated;
    int* this_arena_offset;
    int* next_arena;
};

void addToFreeList(void* base);

/*
 * Initializes an arena block starting at base
 */
void arenaInit(void* base) {
    Header* arena_head = (Header*)base;
    arena_head->num_allocated = 0;
    // + 3 since Header is 3 ints
    arena_head->this_arena_offset = (((int*)base) + 3);
    arena_head->next_arena = nullptr;

    // Need to add new Arena to linked list
    addToFreeList(base);

    /* Debug printing */
    // Debug::printf("allocated: %d, offset: %x, next: %x\n", arena_head->num_allocated, 
    //                     arena_head->this_arena_offset, arena_head->next_arena);
    // Debug::printf("free_start: %x, free_end: %x\n", free_start, free_end);
}

/*
 * Adds an arena block starting at base to the free list
 */
void addToFreeList(void* base) {
    int* temp_base = (int*)base;
    // If it is the only free arena, set free_start and free_end
    if (free_start == nullptr) {
        free_start = temp_base;
        free_end = temp_base;
    } 
    // Not only free arena, put at end
    else {
        ASSERT(free_end != nullptr);
        Header* free_end_head = (Header*)free_end;
        free_end_head->next_arena = temp_base;
        // Set end to this block
        free_end = temp_base;
    }
}

/*
 * Returns the size remaining in the arena block at base
 */
int sizeRemaining(void* base) {
    Header* temp_base = (Header*)base;
    return ARENA_SIZE - ((int)(temp_base->this_arena_offset) & 0x3FFF);
}

/*
 * Updates first arena with size and allocation
 * Returns the malloc block location
 */
int* updateArena(int size) {
    Header* free_start_temp = (Header*)free_start;

    /* Debug printing */
    // Debug::printf("Aligned malloc size: %x\n", size);
    // Debug::printf("Free list before num_alloc: %d, offset: %x\n", free_start_temp->num_allocated, 
    //                 free_start_temp->this_arena_offset);

    // Increment num allocated
    free_start_temp->num_allocated++;
    // Mark block with size and as allocated
    *(free_start_temp->this_arena_offset) = size | 1;

    int* free_fit = free_start_temp->this_arena_offset + 1;

    /* Divide size by 4 since int* */
    free_start_temp->this_arena_offset += (((uint32_t)size)>>2);

    /* Debug printing */
    // Debug::printf("Free list after num_alloc: %d, offset: %x\n", free_start_temp->num_allocated, 
    //                 free_start_temp->this_arena_offset);
    // Debug::printf("malloc return: %x, size:%x\n\n", free_fit, *(free_fit - 1));
        
    return free_fit;
}

/*
 * Finds a fit for as set size in the free list
 * Returns the malloc block location
 */
int* findFit(int size) {
    // Free list is empty, we fail
    if (free_start == nullptr)
        return nullptr;

    // size fits inside of first free block
    if (size <= sizeRemaining((void*)free_start)) {
        // Header* free_start_temp = (Header*)free_start;

        // /* Debug printing */
        // // Debug::printf("Aligned malloc size: %x\n", size);
        // // Debug::printf("Free list before num_alloc: %d, offset: %x\n", free_start_temp->num_allocated, 
        // //                 free_start_temp->this_arena_offset);

        // // Increment num allocated
        // free_start_temp->num_allocated++;
        // // Mark block with size and as allocated
        // *(free_start_temp->this_arena_offset) = size | 1;

        // int* free_fit = free_start_temp->this_arena_offset + 1;

        // /* Divide size by 4 since int* */
        // free_start_temp->this_arena_offset += (((uint32_t)size)>>2);

        // /* Debug printing */
        // // Debug::printf("Free list after num_alloc: %d, offset: %x\n", free_start_temp->num_allocated, 
        // //                 free_start_temp->this_arena_offset);
        // // Debug::printf("malloc return: %x, size:%x\n\n", free_fit, *(free_fit - 1));
        
        // return free_fit;
        return updateArena(size);
    }
    // size does not fit inside first free block
    else {
        /* Debug printing */
        Header* free_start_temp = (Header*)free_start;
        // Debug::printf("FreeListUnable fit num_alloc: %d, offset: %x\n", free_start_temp->num_allocated, 
        //                 free_start_temp->this_arena_offset);
        
        // Mark free block as full by removing from free list
        free_start = free_start_temp->next_arena;
        free_start_temp->next_arena = nullptr;
        
        // If next arena exists
        if (free_start != nullptr) {
            // Header* free_start_temp_next = (Header*)free_start;

            // // Increment num allocated
            // free_start_temp_next->num_allocated++;
            // // Mark block with size and as allocated
            // *(free_start_temp_next->this_arena_offset) = size | 1;

            // int* free_fit = free_start_temp_next->this_arena_offset + 1;

            // /* Divide size by 4 since int* */
            // free_start_temp_next->this_arena_offset += (((uint32_t)size)>>2);
        
            // return free_fit;
            return updateArena(size);
        } 
        // Next arena does not exist
        else {
            return nullptr;
        }

        return 0;
    }
}

void updateFree(void* p) {
    // Check if p is already free
    if ((*(((int*)p) - 1) & 1) == 0) {
        Debug::printf("WE HAVE A DOUBLE FREE");
        return;
    }
    // Is allocated, update to be free
    else {
        // Update malloc block header to be free
        int* p_temp = ((int*) p) - 1;
        *p_temp = *p_temp & ~0x1;

        // Update arena block num allocated
        Header* arena_temp = ((Header*)((int)p & ~0x3FFF));
        arena_temp->num_allocated--;

        ASSERT(arena_temp->num_allocated >= 0);

        // If arena completely empty, we add back to free list
        if (arena_temp->num_allocated == 0) {
            addToFreeList((void*)arena_temp);
        }
    }
}

};



void heapInit(void* base, size_t bytes) {
    using namespace gheith;

    Debug::printf("| heap range 0x%x 0x%x\n",(uint32_t)base,(uint32_t)base+bytes);

    /* can't say new becasue we're initializing the heap */
    // array = (int*) base;
    // len = bytes / 4;
    // makeTaken(0,2);
    // makeAvail(2,len-4);
    // makeTaken(len-2,2);

    // Loop through entire heap and set up arenas
    for (uint32_t i = (uint32_t)base; i < (uint32_t)base+bytes; i += ARENA_SIZE) {
        arenaInit((void*)i);
    }

    // Debug::printf("End of heapInit\n");
    theLock = new BlockingLock();
}

void* malloc(size_t bytes) {
    using namespace gheith;
    // Debug::printf("malloc(%d)\n",bytes);
    // Debug::printf("malloc(%x)\n",bytes);

    if (bytes == 0) return (void*) HEAP_HEAP_START;

    // if (bytes == 0) return (void*) array;

    // int ints = ((bytes + 3) / 4) + 2;
    // if (ints < 4) ints = 4;

    LockGuardP g{theLock};

    // Align size and add 1 for size storage
    int alloc_size = ((bytes + 0x3) & ~0x3) + 4;
    // Assume that malloc size never greater than Arena size
    if (alloc_size > ((int)ARENA_SIZE - 12)) {
        Debug::panic("malloc greater than arena size");
        return 0;
    }

    return (void*) findFit(alloc_size);
    // void* res = 0;

    // int mx = 0x7FFFFFFF;
    // int it = 0;

    // {
    //     int countDown = 20;
    //     int p = avail;
    //     while (p != 0) {
    //         if (!isAvail(p)) {
    //             Debug::panic("block is not available in malloc %p\n",p);
    //         }
    //         int sz = size(p);

    //         if (sz >= ints) {
    //             if (sz < mx) {
    //                 mx = sz;
    //                 it = p;
    //             }
    //             countDown --;
    //             if (countDown == 0) break;
    //         }
    //         p = next(p);
    //     }
    // }

    // if (it != 0) {
    //     remove(it);
    //     int extra = mx - ints;
    //     if (extra >= 4) {
    //         makeTaken(it,ints);
    //         makeAvail(it+ints,extra);
    //     } else {
    //         makeTaken(it,mx);
    //     }
    //     res = &array[it+1];
    // }

    // return res;
}

void free(void* p) {
    using namespace gheith;
    if (p == 0) return;
    // if (p == (void*) array) return;

    LockGuardP g{theLock};

    updateFree(p);
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
