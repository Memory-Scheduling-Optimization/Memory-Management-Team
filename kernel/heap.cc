#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"

/***********************************/
/* ASSUMPTIONS			   */
/* 1) Worst fit allocation         */
/* 2) Explicit list of free blocks */
/* 3) Coalescing done during free  */
/***********************************/

/*************************/
/* Some Helper Functions */
/*************************/

//purely for alignment purposes
//recall 1 word is 4 bytes, so conversionFactor = 4
//we need to return the minimum number of words needed to satisfy the number of bytes
inline int convertByteToWord(int bytes, int conversionFactor) {
	if(bytes % conversionFactor == 0) {
		//divisible by 4 bytes per word, we're good to go
		return bytes / conversionFactor;
	}
	else {
		//we need to round up
		return (bytes / conversionFactor) + 1;
	}
}

//return abs|x|
template<typename T>
inline T abs(T v) {
	return (v < 0) ? -v : v;
}

//add value to ptr
//return ptr casted as T*
template<typename T>
inline T* ptr_add(void* ptr, int value) {
	return (T*) (((char*) ptr) + value);
}


/***********************************/
/* Global Variables and Structures */
/***********************************/

namespace rdanait {

        static BlockingLock *theLock = nullptr;

	struct Footer {
		int sizeOfNode;
	};

	struct Header {
		//positive means allocated node, negative means free node
		//size is in words
		int sizeOfNode;

		//ptr to header of the previous free node
		Header* prevNode;
		//ptr to header of the next free node
		Header* nextNode;

		//return the ptr to header of the next node
		//we just have to add to this node
		inline Header* getRightHeader() {
			return ptr_add<Header>(this, abs<int>(sizeOfNode) * 4);
		}

		//return the ptr to the footer of this node
		inline Footer* getFooter() {
			return ptr_add<Footer>(getRightHeader(), -4);
		}

		//return the ptr to header of the previous node
		//we just have to subtract from this node
		inline Header* getLeftHeader() {
			int sizeOfLeftNode = abs(ptr_add<Footer>(this, -4) -> sizeOfNode);
			return ptr_add<Header>(this, -sizeOfLeftNode * 4);
		}

		inline bool isThisNodeFree() {
			return sizeOfNode < 0;
		}

	};

	//ptr to header of first free node in the list
	Header* freeListPtr = nullptr;
	//ptr to start of the heap
	void* heapPtr = nullptr;
	//heap size
	int heapSize = 0;

	/*************************/
	/* More Helper Functions */
	/*************************/

	//Node to be removed is no longer free
	void removeFreeNode(Header* nodeToRemove) {
		//Looks like this node is the only node in the free list
		if(nodeToRemove -> prevNode == nullptr && nodeToRemove -> nextNode == nullptr) {
			//Just make the ptr to header of the first free node nullptr
			freeListPtr = nullptr;
		}
		//Looks like this node is the last node in the free list
		else if(nodeToRemove -> prevNode != nullptr && nodeToRemove -> nextNode == nullptr) {
			//Remove the previous node's next reference
			nodeToRemove -> prevNode -> nextNode = nullptr;
		}
		//Looks like this node is the first node in the free list, but not the only node
		else if(nodeToRemove -> prevNode == nullptr && nodeToRemove -> nextNode != nullptr) {
			//Make the ptr to header of the first node in the free list be the next node now
			freeListPtr = nodeToRemove -> nextNode;
			//Remove the next node's prev reference
			nodeToRemove -> nextNode -> prevNode = nullptr;
		}
		//Looks like this node is in between two free nodes in the free list
		else {
			//set the next reference of the prev node to this node's next node
			nodeToRemove -> prevNode -> nextNode = nodeToRemove -> nextNode;
			//set the prev reference of the next node to this node's prev node
			nodeToRemove -> nextNode -> prevNode = nodeToRemove -> prevNode;
		}
	}

	//Current and next node (which exists) must be free
	void combineWithRightNode(Header* nodePtr) {
		//We first need to get the header of the right node
		Header* nextNodeHeader = nodePtr -> getRightHeader();
		//Now, we need to update the size of the new node
		int newSize = nextNodeHeader -> sizeOfNode + nodePtr -> sizeOfNode;
		nodePtr -> sizeOfNode = newSize;
		//Don't forget to update the footer too!
		nodePtr -> getFooter() -> sizeOfNode = newSize;
		//Now we can remove the next node from the free node list
		removeFreeNode(nextNodeHeader);
	}
};


/******************/
/* Heap Functions */
/******************/

void heapInit(void* start, size_t bytes) {

    using namespace rdanait;
    heapPtr = start;
    heapSize = bytes;
    freeListPtr = (Header*) start;
    //Get the size, indicate it is free with negative
    freeListPtr -> sizeOfNode = -convertByteToWord(heapSize, 4);
    //Init the prev and next nodes to nullptr
    freeListPtr -> prevNode = nullptr;
    freeListPtr -> nextNode = nullptr;
    //Make sure to put the size in the footer as well
    freeListPtr -> getFooter() -> sizeOfNode = freeListPtr -> sizeOfNode;
    theLock = new BlockingLock();
}

void* malloc(size_t bytes) {

    using namespace rdanait;
    //What if bytes < 0 or bytes > heapSize? Just return nullptr
	if ((int) bytes < 0 || (int) bytes > heapSize - 8) return nullptr;

	//If malloc(0), we want to return one past the highest possible address malloc can return 
	if (bytes == 0) return (void*) (((char*) heapPtr) + heapSize);

	LockGuardP g{theLock};
	//Get the size in words
	int words = convertByteToWord(bytes, 4);
	if (words < 2) words = 2;

        Header* worst = nullptr;

        if(freeListPtr != nullptr) {
	    Header* newFreeNode = freeListPtr;
	    //need to search through the free list for a free node using worst fit algorithm 
	    while(newFreeNode != nullptr) {
		//We found a free node that's size is big enough
		if(abs(newFreeNode -> sizeOfNode) - 2 >= words) {
	            //First possible match is of course the current worst match
		    if(worst == nullptr) {
			worst = newFreeNode;
	            }
	            else { 
      	                //we found a match earlier
		        //only change if current free node is actually worse than worst
		        if(abs(worst -> sizeOfNode) < abs(newFreeNode -> sizeOfNode)) {
		            worst = newFreeNode;
	                }
	            }
	        }
		//This node doesn't have a large enough size, let's try the next one
		newFreeNode = newFreeNode -> nextNode;
	    }
	}
	if(worst == nullptr) return nullptr; //can just return now if we didn't find a worst node 
	else {
            //We have to split the node up
	    if(abs(worst -> sizeOfNode) >= words + 8) {
		//we need to update the size of the free node now
                worst -> sizeOfNode = -(abs(worst -> sizeOfNode) - words - 2);
                //update the footer's size as well
                worst -> getFooter() -> sizeOfNode = worst -> sizeOfNode;
                //Right node header size needs to update
                worst -> getRightHeader() -> sizeOfNode = words + 2;
                //Right node footer size also needs to be updated
                worst -> getRightHeader() -> getFooter() -> sizeOfNode = words + 2;
                //return a ptr to new node
                return ptr_add<void>(worst -> getRightHeader(), 4);
	    }
            else {
                //negate the size in the header and footer, since it is now allocated
                worst -> sizeOfNode *= -1;
                worst -> getFooter() -> sizeOfNode *= -1;
                //remove from the free nodes list
                removeFreeNode(worst);
                //return a ptr to the node
                return ptr_add<void>(worst, 4);	
	    }
	}

	return nullptr;
}

void free(void* p) {

    using namespace rdanait;

    //If nullptr, don't need to do anything
    if (p == nullptr) return;
        //Or if we're trying to free an invalid address, say, as a result of malloc(0)
	if (p == ((char*) heapPtr) + heapSize) return;
    if (ptr_add<Header>(p, -4) -> isThisNodeFree()) return;
        LockGuardP g{theLock};

	Header* headerOfP = ptr_add<Header>(p, -4);
	//We first need to make sure this node is indicated to be free in header and footer by negating
	headerOfP -> sizeOfNode *= -1;
    headerOfP -> getFooter() -> sizeOfNode *= -1;
    //Now we can add this node to our free nodes list
    //This will not be the only node in our free nodes list
    if (freeListPtr != nullptr) {
    	//Make the prev reference null
    	headerOfP -> prevNode = nullptr;
    	//Header's next reference needs to be the header ptr
	    headerOfP -> nextNode = freeListPtr;
	    //Beginning of the free nodes list
	    freeListPtr = headerOfP;
	    //Next node's prev reference needs to be this node
	    headerOfP -> nextNode -> prevNode = headerOfP;

    }
    //Else this is the only in our free nodes list
    else {
    	//Make this the first node in the free list
    	freeListPtr = headerOfP;
    	//Make the prev reference null
		headerOfP -> prevNode = nullptr;
		//Make the next reference null
		headerOfP -> nextNode = nullptr;
    }

    //Now we want to coalesce some nodes if they are free
    //We first coalesce with the right node if it exists and is free
    if ((char*) (headerOfP -> getRightHeader()) < (((char*) heapPtr) + heapSize) && headerOfP -> getRightHeader() -> isThisNodeFree()) {
		combineWithRightNode(headerOfP);
    }
    //We coalesce with the left node if it exists and is free
    if (((char*) headerOfP) > ((char*) heapPtr) && headerOfP -> getLeftHeader() -> isThisNodeFree()) {
		combineWithRightNode(headerOfP -> getLeftHeader());
    }

}

int spaceUnallocated() {
    using namespace rdanait;
    int totSpace = 0;
    Header* currentFreeNode = freeListPtr;
    while(currentFreeNode != nullptr && currentFreeNode -> isThisNodeFree()) {
	totSpace += abs(currentFreeNode -> sizeOfNode);
	currentFreeNode = currentFreeNode -> nextNode;
    }
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
