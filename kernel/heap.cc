#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"

/***********************************/
/* ASSUMPTIONS                     */
/* 1) Buddy block allocation       */
/* 2) Circular doubly linked list  */
/*    and each bucket corresponds  */
/*    to a certain allocation size */
/*    and stores a free list for   */
/*    size                         */
/***********************************/

/***********************************/
/* Global Variables and Structures */
/***********************************/

namespace rdanait {

        static BlockingLock *theLock = nullptr;
	//ptr to first free node in the list
	int* freeListPtr = nullptr;
	//ptr to start of the heap
	void* heapPtr = nullptr;
	//heap size
	int heapSize = 0;
	
        void offset_updater(int *heap_ptr, int old_offset) {
            int* temp = freeListPtr;
    	    while ((temp + temp[1]/4) != heap_ptr || temp[1] == 0) {
        	if(temp[1] == 0) return;
        	temp += temp[1]/4;
    	    }
    	    if(old_offset == 0) temp[1] = 0;
    	    else temp[1] += old_offset;
    	    return;
	}

	void free_offset_updater(int *header_ptr) {
            int* temp = freeListPtr;
    	    //traverse the list until the free block immediately above the newly freed block
	    while(temp + temp[1]/4 < header_ptr) {
        	temp += temp[1]/4;
    	    }
	    //update the offset
    	    int offset = header_ptr - temp;
    	    temp[1] = offset;
	}

        int* gruntWork(int* heap_ptr, int size) {
	    int* heap = heap_ptr;
	    //heap points to a block of the right size
	    if(heap[0] == size) {
		//check if this block is the first free block
		if(heap == freeListPtr) {
		    //update free list to point to the next free block
		    freeListPtr += heap[1]/4;
		}
		//save old offset
		int old_offset = heap[1];
		//mark as allocated
		heap[1] = 0;
		offset_updater(heap, old_offset);
		return heap + 2;
 	    }
	    //heap points to a block with smaller size
	    if(heap[0] < size) {
		if(heap[1] == 0) {
		    //if this is the last free block
		    return nullptr;
		}
		int ptr_offset = heap[1]/4;
		return gruntWork(heap + ptr_offset, size);
	    }
	    else {
		int buddy = heap[0]/2;
		//save old offset
		int old_offset = heap[1];
		//set size of current block
		heap[0] = buddy;
		heap[1] = buddy;
		int buddy_offset = buddy/4;
		//set size of buddy block
		heap[buddy_offset] = buddy;
		if(old_offset == 0) heap[buddy_offset + 1] = 0;
		else heap[buddy_offset + 1] = old_offset - buddy;
		return gruntWork(heap, size);
	    }
	}

	int* findBuddy(int* block) {
	    int* heap = (int*) heapPtr;
	    int count = 1;
	    int cumulative = 0;
	    int lastSize = 0;

	    //passes through the heap until the passed in memory block is met
	    while(heap != block) {
		if(heap[0] < block[0]) {
		    cumulative += heap[0];
		    if(cumulative < block[0]) {
			heap += heap[0]/4;
			lastSize = heap[0];
			//count is not incremented because cumulative is not yet of the same size as the passed in block
			continue;
		    }
		}
		if(heap[0] > block[0]) {
		    //count is not incremented because the block is too big 
		    heap += heap[0]/4;
                    lastSize = heap[0];
		    continue;
		}
		//count increments either when an accumulation of small memory blocks
		//reach the size of the passed in memory block or when the heap pointer
		//points to a memory block of equal size
		lastSize = heap[0];
		cumulative = 0;
		count++;
		heap += heap[0]/4;
	    }
    	    int* before = heap - (lastSize/4);
	    int* next = heap + heap[0]/4;
	    //if the count is odd we look at the next pointer 
	    if (count % 2) {
		//because it is the first buddy (when sequentially looking through the heap)
		if(next[0] == block[0] && next[1] >= block[0]) {
		    block[0] *= 2;
		    block[1] += next[1];
		    next[0] = 0;
		    next[1] = 0;
		    //open buddy is found and findBuddy is called again
		    return findBuddy(block);
		}
		else return block; //no open buddy, nothing is coalesced
	    }	    
	    else {
		//the count is even so we look at the before pointer 
		//because it is the second of the two buddies (sequentially)
		if(before[0] == block[0] && before[1] > 0) {
		    before[0] *= 2;
		    block[0] = 0;
		    block[1] = 0;
		    //open buddy at the before pointer and findBuddy is called again
		    return findBuddy(before);
		}
		else return block; //no open buddy, nothing is coalesced
            }
	}
};


/******************/
/* Heap Functions */
/******************/

void heapInit(void* start, size_t bytes) {

    using namespace rdanait;
    heapPtr = start;
    heapSize = bytes;
    freeListPtr = (int*) heapPtr;
    freeListPtr[0] = heapSize;
    freeListPtr[1] = 0; //0 for last free block
    theLock = new BlockingLock();
}

void* malloc(size_t bytes) {
    using namespace rdanait;
    LockGuardP g{theLock};
    int* ptr;
    bytes += 8;
    int size = sizeof(int)*2;
    while(size < (int) bytes) {
	size *= 2;
    }
    ptr = gruntWork(freeListPtr, size);
    Debug::printf("ptr is: %d\n", ptr);
    return (void*) ptr;

}

void free(void* p) {

    using namespace rdanait;
    LockGuardP g{theLock};
    int* headerPtr = (int*) p;
    headerPtr -= 2;
    //find the last free block
    int* lastFree = freeListPtr;
    while(lastFree[1] != 0) lastFree += lastFree[1]/4;
    //insert freed block into the free list, i.e. correct offsets
    //headerPtr needs to be the new last free block
    if(lastFree < headerPtr) {
	int offset = lastFree[0];
	int* temp = lastFree;
        //while the next block is allocated and is not the newly freed block
	while((temp + (temp[0]/4) + 1) == 0 && temp + temp[0]/4 != headerPtr) {
	    temp += temp[0]/4; //move temp the size of the allocated block
            offset += temp[0]; //add the size of the next block to offset
	}
	lastFree[1] = offset;
    }
    //else find what the offset needs to be for the freed block
    else {
	int offset = headerPtr[0];
	int* temp = headerPtr;
	//while the next block is allocated and is not the last free block
	while((temp + (temp[0]/4) + 1) == 0 && temp + temp[0]/4 != lastFree) {
	    temp += temp[0]/4; //move temp the size of the allocated block
            offset += temp[0]; //add the size of the next block to offset
	}
	headerPtr[1] = offset;
	//check if the freed block should be the new freelist head
	if(headerPtr < freeListPtr) {
	    freeListPtr = headerPtr;
	}
	//else update the offset of the free block above the newly freed block
	else free_offset_updater(headerPtr);
    }
    findBuddy(headerPtr);
}

int spaceUnallocated() {
    using namespace rdanait;
    int* free = freeListPtr;
    int totSize = 0;
    while(free[1] != 0) {
	int allocSize = free[1] - free[0];
	if(allocSize == 0) {
	    totSize += free[0];
	}	
	free += free[1]/4;
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
