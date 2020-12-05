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

namespace rdanait {

    static BlockingLock *theLock = nullptr;
    struct TBlock {
	TBlock* nextFree;
	TBlock* self;
	bool free;
	int size;
    };

    TBlock* start;
    int blocks;
    int size;
    TBlock* freeBlocks[22];

    int powerOfTwo(int val) {
	int i = 0;
	while(val >>= 1) i++;
	return i;
    }

    void addToList(TBlock* block, int i) {
	if(!block) return;
	block->nextFree = freeBlocks[i];
	freeBlocks[i] = block;
    }

    void removeFromList(TBlock* block, int i) {
	if(!block) return;
	TBlock* temp = freeBlocks[i];
	if(block == temp) {
	    freeBlocks[i] = block->nextFree;
	    return;
        }
	while(temp) {
	    if(temp->nextFree == block) temp->nextFree = block->nextFree;
	    temp = temp->nextFree;  
	}
    }

    TBlock* divide(TBlock* block, int i) {
        int size = ((block->size + sizeof(TBlock)) / 2) - sizeof(TBlock);
        removeFromList(block, i);
        block->free = true;
        block->size = size;
        block->self = block;
        TBlock* buddy;
        buddy = (TBlock*) ((uint8_t*)block + sizeof(TBlock) + size);
        buddy->free = true;
        buddy->size = size;
        buddy->self = buddy;
        addToList(buddy, i - 1);
        return block;
    }

    TBlock* findBuddy(TBlock* block, int i) {
        long addr = ((uint8_t*) block - (uint8_t*) start);
        return ((TBlock*)((addr ^= (1 << i)) + (size_t) start));
    }

    TBlock* merge(TBlock* block) {
        TBlock* buddy;
        int i = powerOfTwo(block->size + sizeof(TBlock));
        buddy = findBuddy(block, i);
        if(!buddy->free || buddy->size != block->size) return nullptr;
        if(block > buddy) {
            TBlock* x = block;
            block = buddy;
            buddy = x;
        }
        removeFromList(block, i);
        removeFromList(buddy, i);
        block->size = block->size * 2 + sizeof(TBlock);
        block->free = true;
        block->self = block;
        addToList(block, i + 1);
        return block;
    }

};


/******************/
/* Heap Functions */
/******************/

void heapInit(void* startLoc, size_t bytes) {
    using namespace rdanait;
    int p = powerOfTwo((int) bytes);
    bytes = 1 << p;
    start = (TBlock*) startLoc;
    start->size = bytes - sizeof(TBlock);
    start->free = true;
    start->self = start;
    blocks = 0;
    size = start->size;
    for(int i = 0; i < 22; i++) freeBlocks[i] = nullptr;
    addToList(start, p);
    theLock = new BlockingLock();
}

void* malloc(size_t bytes) {
    using namespace rdanait;
    LockGuardP g{theLock};
     
    int i = powerOfTwo(bytes + sizeof(TBlock)) + 1;
    while(!freeBlocks[i] && i < 22) i++;
    if(i > 22) return nullptr;
    TBlock* temp;
    temp = freeBlocks[i];
    removeFromList(temp, i);
    while((temp->size + sizeof(TBlock)) / 2 >= bytes + sizeof(TBlock)) {
	temp = divide(temp, i);
	i--;
    }
    temp->free = false;
    temp->self = temp;
    blocks++;
    return temp + 1;
}

void free(void* block) {

    using namespace rdanait;
    LockGuardP g{theLock};

    TBlock* temp = (TBlock*)((uint8_t*) block - sizeof(TBlock));
    if(temp->self == temp) {
	temp->free = true;
	addToList(temp, powerOfTwo(temp->size + sizeof(TBlock)));
	while(temp) {
	    if(temp->size == size) break;
	    else temp = merge(temp);
	}
	blocks--;
    } 
}

int spaceUnallocated() {
    using namespace rdanait;
    int totSize = 0;
    for(int i = 0; i < 22; i++) {
	TBlock* temp = freeBlocks[i];
	if(temp != nullptr) totSize += temp->size;
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
