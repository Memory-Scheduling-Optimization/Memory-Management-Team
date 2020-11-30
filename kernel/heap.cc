#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"

/* A first-fit heap */

static constexpr uint32_t HEAP_HEAP_START = 1 * 1024 * 1024;
static constexpr uint32_t HEAP_HEAP_SIZE = 5 * 1024 * 1024;
static constexpr uint32_t ARENA_SIZE = 512 * 1024;
static constexpr uint32_t ARENA_MASK = 0x7FFFF;

namespace gheith {

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
        // Debug::printf("Free start is nullptr\n");
        free_start = temp_base;
        free_end = temp_base;
    } 
    // Not only free arena, put at end
    else {
        // Debug::printf("add to free list\n");
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
    // Debug::printf("%x\n", ARENA_SIZE - ((int)(temp_base->this_arena_offset) & 0x3FFF));
    if (((int)(temp_base->this_arena_offset) & ARENA_MASK) == 0)
        return 0;

    return ARENA_SIZE - ((int)(temp_base->this_arena_offset) & ARENA_MASK);
}

/*
 * Updates first arena with size and allocation
 * Returns the malloc block location
 */
int* updateArena(int size) {
    Header* free_start_temp = (Header*)free_start;

    /* Debug printing */
    // Debug::printf("\nAligned malloc size: %x\n", size);
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
    // Debug::printf("malloc return: %x, size:%x\n", free_fit, *(free_fit - 1));
        
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
        Debug::printf("WE HAVE A DOUBLE FREE %x\n", p);
        return;
    }
    // Is allocated, update to be free
    else {
        // Update malloc block header to be free
        int* p_temp = ((int*) p) - 1;
        *p_temp = *p_temp & ~0x1;

        // Update arena block num allocated
        Header* arena_temp = ((Header*)((int)p & ~ARENA_MASK));
        arena_temp->num_allocated--;

        // Debug::printf("%x %d\n", ((int)p & ~ARENA_MASK), arena_temp->num_allocated);

        ASSERT(arena_temp->num_allocated >= 0);

        // If arena completely empty, we add back to free list
        if (arena_temp->num_allocated == 0) {
            // Debug::printf("add to free\n");
            arena_temp->this_arena_offset = (int*)arena_temp + 3;
            if (free_start != (int*)arena_temp) {
                arena_temp->next_arena = nullptr;
                addToFreeList((void*)arena_temp);
            }
        }
    }
}

int freeListSpace() {
    int total_space = 0;
    int* curr_free_block = free_start;

    // Debug::printf("%x\n", free_start);
    // Loop through free list
    while (curr_free_block != nullptr) {
        total_space += sizeRemaining(curr_free_block);
        // Go to next block
        curr_free_block = (int*)*(curr_free_block + 2);
    }
    return total_space;
}

};



void heapInit(void* base, size_t bytes) {
    using namespace gheith;

    Debug::printf("| heap range 0x%x 0x%x\n",(uint32_t)base,(uint32_t)base+bytes);

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

    LockGuardP g{theLock};

    // Align size and add 1 for size storage
    int alloc_size = ((bytes + 0x3) & ~0x3) + 4;
    // Assume that malloc size never greater than Arena size
    if (alloc_size > ((int)ARENA_SIZE - 12)) {
        Debug::panic("malloc greater than arena size %d\n", bytes);
        return 0;
    }

    return (void*) findFit(alloc_size);
}

void free(void* p) {
    using namespace gheith;
    if (p == 0) return;
    // if (p == (void*) array) return;

    LockGuardP g{theLock};

    // if ((((uint32_t)p) & 0x3FFF) < 0xF)
    //     Debug::printf("%x\n", p);

    updateFree(p);
}

int spaceUnallocated() {
    using namespace gheith;
    // Debug::printf("base method: %d\n",freeListSpace());
    return freeListSpace();
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
