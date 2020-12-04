#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"
#include "max_heap.h"

/***********************************/
/* ASSUMPTIONS			   */
/* 1) Worst fit allocation         */
/* 2) Max heap of free blocks      */
/***********************************/

/*************************/
/* Some Helper Functions */
/*************************/

/***********************************/
/* Global Variables and Structures */
/***********************************/

namespace rdanait {

        static BlockingLock *theLock = nullptr;
	//pointer to max heap free array
        MaxHeap* freeArray = nullptr;
};


/******************/
/* Heap Functions */
/******************/

void heapInit(void* start, size_t bytes) {

    Debug::printf("initializing heap\n");
    using namespace rdanait;
    freeArray->arr = (int*) start;
    Debug::printf("Size of free array is %d\n", sizeof(freeArray->arr));
    freeArray->size = bytes / 4;
    Debug::printf("Length of free array is %d\n", freeArray->size);
    freeArray->arr[0] = (int) bytes;
    Debug::printf("max heap array size starts at %d\n", freeArray->arr[0]);
    theLock = new BlockingLock();
}

void* malloc(size_t bytes) {

    using namespace rdanait;
    LockGuardP g{theLock};
    Debug::printf("Mallocing %d bytes\n", bytes);
    if(bytes == 0) return (void*) freeArray->arr;
    int req = (int) bytes;
    int max = freeArray->getMax();
    Debug::printf("Max is %d\n", max);
    //max value is greater than request size, we need to make sure we put the remaining
    //free space back into our max heap
    if(req < max) {
	Debug::printf("--Max value %d greater than request size %d\n", max, req);
        int remaining = freeArray->getMax() - req;
	Debug::printf("----Remaining value is %d\n", remaining);
	Debug::printf("----Size is %d\n", freeArray->size);
	int result = freeArray->extractMax();
        Debug::printf("----Size after extracting max is %d\n", freeArray->size);
	Debug::printf("----Max is %d\n", result);
	freeArray->insert(remaining);
	Debug::printf("--------New max should be remaining, new max is: %d\n", freeArray->getMax());
	//Debug::printf("--------Calling heapify...\n");
	//freeArray->heapify(freeArray->arr, freeArray->size);
	//Debug::printf("--------Value returned is %p\n", (void*) result);
	return (void*) result;
    }
    else if(req == max) {
	Debug::printf("--Max value %d equals request size %d\n", max, req);
    	Debug::printf("----Size is %d\n", freeArray->size);
	int result = freeArray->extractMax();
	Debug::printf("----Size after extracting max is %d\n", freeArray->size);
        Debug::printf("----Max is %d\n", result);
	//Debug::printf("--------Calling heapify...\n");
	//freeArray->heapify(freeArray->arr, freeArray->size);
	Debug::printf("--------Value returned is %p\n", (void*) result);
	return (void*) result;
    }
    //max value is less than request size, cannot satisfy
    else {
	Debug::printf("Max value %d less than request size %d\n", max, req);
    	return nullptr;
    }
}

void free(void* p) {

    using namespace rdanait;
    LockGuardP g{theLock};
    int bytes = (int) p;
    freeArray->insert(bytes);  
}

int spaceUnallocated() {
    using namespace rdanait;
    int totSpace = 0;
    
    return totSpace;
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
