#include "stdint.h"
#include "debug.h"
#include "random.h"
#include "shared.h"
#include "threads.h"
#include "ext2.h"
#include "elf.h"
#include "vmm.h"
#include "process.h"
#include "barrier.h"
#include "sys.h"
#include "pit.h"

// static void* blockPointersTest1[100];
// static void* blockPointersTest2[1000];
constexpr uint32_t BIG_MALLOCS = 235;
struct BigChunkOfMemory {
    uint8_t a;
    uint16_t b;
    uint8_t c;
    uint32_t d;
    uint64_t e;
    uint8_t f;
    uint16_t g;
    void* h[100];

    struct LittleChunkOfMemory {
        int8_t i;
        uint64_t j;
        int8_t k;
    } l;

    struct MoreGarbage {
        int8_t m;
        void* n[1000];
        uint64_t o;
    } p;
};
void doEfficiencyTest(void);
void doLargeStressTest(void);
void doSpeedTest(void);
void doTimingSpeedTest(void);
void memUtil(void);

void kernelMain(void) {

    Debug::printf("\n");
    // Debug::printf("Start of simple stress/correctness tests\n");
    // Debug::printf("Test 1\n");

    // const int LOOPS = 10000;

    // for (int i = 0; i < LOOPS; i++) {

	// for (int i = 0; i < 100; i++) {
    //         blockPointersTest1[i] = malloc(100);
    //     }

    //     for (int i = 0; i < 100; i += 2) {
    //         free(blockPointersTest1[i]);
    //     }

    //     for (int i = 1; i < 100; i += 2) {
    //         free(blockPointersTest1[i]);
    //         void *p = malloc(LOOPS * 100);
    //         if (p == 0) Debug::panic("*** Unable to free every alternate block\n");
    //         else free(p);
    //     }
    // }

    // Debug::printf("*** Passed Test 1: freeing alternate blocks\n");
    
    // Debug::printf("Test 2\n");

    // for(int i = 0; i < 1000; i++) {
	// blockPointersTest2[i] = malloc(1);
    // }
    
    // for(int i = 0; i < 1000; i++) {
	// free(blockPointersTest2[i]);
    // }

    // void* p = malloc(10000);
    // if (p == 0) Debug::panic("*** Unable to allocate large block after freeing many small allocations\n");
    // else Debug::printf("*** Passed Test 2: allocating large block after freeing many small allocations\n");

    // Debug::printf("Test 3\n");

    // void* p2 = malloc(0);
    // if (p2 == 0) Debug::panic("*** Unable to malloc(0)\n");
    // else {
	// free(p2);
	// Debug::printf("*** Passed test 3: malloc(0) and free(0)\n");
    // }

    // Debug::printf("End of simple stress/correctness tests\n");
    
    // Debug::printf("\n");

    Debug::printf("Start of efficiency test\n");
    doEfficiencyTest();
    Debug::printf("End of efficiency test\n");

    Debug::printf("\n");

    Debug::printf("Start of big malloc stress trest\n");
    doLargeStressTest();
    Debug::printf("End of big malloc stress test\n");

    Debug::printf("\n");

    Debug::printf("Start of speed test\n");
    doSpeedTest();
    Debug::printf("End of speed test\n");

    Debug::printf("\n");

    Debug::printf("Memory utilization calculation\n");
    memUtil();


    doTimingSpeedTest();
}

void doEfficiencyTest() {
    constexpr uint32_t len = 10 * 1000;
    void** arr = new void*[len];
    //Debug::printf("Made it here, 1. \n");
    //A bunch of large mallocs
    constexpr size_t large_bytes = 1000;
    for (uint32_t i = 0; i < len/3; ++i) {
        arr[i] = malloc(large_bytes);
    }

    //Debug::printf("Made it here, 2. \n");

    //Free up some of the large blocks
    for (uint32_t i = 0; i < len/12; ++i) {
        free(arr[i]);
    }

    //Debug::printf("Made it here, 3. \n");

    //A bunch of relatively small mallocs
    constexpr size_t small_bytes = 500;
    for (uint32_t i = len/3; i < (len * 2)/3; ++ i) {
        arr[i] = malloc(small_bytes);
    }

    //Debug::printf("Made it here, 4. \n");

    //Some relatively big mallocs
    auto medium_bytes = (large_bytes * 3) / 4;
    for (uint32_t i = (len * 2)/3; i < len; ++ i) {
        arr[i] = malloc(medium_bytes);
    }

    //Debug::printf("Made it here, 5. \n");

    for (uint32_t i = len/12; i < len; ++ i) {
	//Debug::printf("i: %d, arr[i] %x\n", i, arr[i]);
        free(arr[i]);
    }

    Debug::printf("*** Passed space efficiency test\n");
}

void doLargeStressTest() {
    BigChunkOfMemory** arr = new BigChunkOfMemory*[BIG_MALLOCS];

    for (uint32_t i = 0; i < BIG_MALLOCS; ++ i) {
        arr[i] = new BigChunkOfMemory;
    }

    for (uint32_t i = 0; i <= BIG_MALLOCS / 2; ++ i) {
        free(arr[i]);
    }

    for (uint32_t i = BIG_MALLOCS - 1; i > BIG_MALLOCS / 2; -- i) {
        free(arr[i]);
    }

    free(arr);
   
    Debug::printf("*** Passed big malloc stress test\n");
}

void doTimingTest() {
    int** arr = new int*[100000];

    for (uint32_t i = 0; i < 100000; i++) {
        arr[i] = (int*)malloc(4);
    }
    memUtil();

    for (uint32_t i = 0; i < 100000; i++) {
        free(arr[i]);
    }
    
    free(arr);
    
    memUtil();
}

template <typename Work>
uint32_t howLong(Work work) {
    uint32_t start = Pit::jiffies;
    work();
    return (Pit::jiffies - start);
}

void doSpeedTest() {

    //Speed test calls large stress test
    auto x = howLong([] {
	doLargeStressTest();
    });

    Debug::printf("*** Time taken for big malloc stress test: %d\n", x);

}

void doTimingSpeedTest() {
    //Speed test calls large stress test
    auto x = howLong([] {
	doTimingTest();
    });

    Debug::printf("*** Time taken for big malloc timing test: %d\n", x);

}

void memUtil() {
    double heapSize = 5 * 1024 * 1024;
    double totalAmountUnallocated = spaceUnallocated();
    Debug::printf("*** Memory utilized = %f%%\n", ((heapSize - totalAmountUnallocated)/heapSize)*100);   
}


// #include "stdint.h"
// #include "debug.h"

// #include "shared.h"
// #include "threads.h"
// #include "ext2.h"
// #include "elf.h"
// #include "vmm.h"
// #include "process.h"
// #include "barrier.h"
// #include "sys.h"

// const char* initName = "/sbin/init";

// void printFile(Shared<Node> file) {
//     if (file == nullptr) {
// 	Debug::printf("file does not exist\n");
// 	return;
//     }
//     auto buffer = new char[file->size_in_bytes()];
//     file->read_all(0, file->size_in_bytes(), buffer);
//     for (uint32_t i = 0; i < file->size_in_bytes(); i++)
// 	Debug::printf("%c", buffer[i]);
//     delete[] buffer;
// }

// void basicArenaTest(void);
// void bulkArenaTest(void);
// void calculateSpaceUsage(void);

// void kernelMain(void) {

//     Debug::printf("*** Start of kernelMain()\n");

//     // Test for basic functionality of malloc and free
//     basicArenaTest();
//     // Debug::printf("%x\n", spaceRemaining());

//     // Testing to make sure we can allocate everything we should be able to
//     bulkArenaTest();
//     // Debug::printf("%x\n", spaceRemaining());

//     // Do twice to make sure we are freeing properly
//     bulkArenaTest();
//     // Debug::printf("%x\n", spaceRemaining());

//     calculateSpaceUsage();

//     Debug::printf("*** End of kernelMain()\n");
// }

// void checkAlignment(void* p) {
//     if (((int)p & 3) != 0) {
//         Debug::printf("*** Alignment failure at %x\n", p);
//     }
// }

// void basicArenaTest(void) {
//     int* temp = (int*)malloc(4);
//     checkAlignment((void*)temp);
//     *temp = 420;

//     int* temp2 = (int*)malloc(26);
//     checkAlignment((void*)temp2);
//     *temp2 = 0xFFFFFFFF;

//     if (*temp != 420) {
//         Debug::printf("*** Basic Arena Test failed\n");
//     } else {
//         Debug::printf("*** Basic Arena Test passed\n");
//     }

//     free(temp2);
//     free(temp);
// }

// void bulkArenaTest(void) {
//     // In theory, there should be 1 fully allocated arena, 
//     // 1 partially allocated, rest unallocated
//     Debug::printf("\nBulk Arena Test started\n");

//     int num_arenas_remaining = 5 * 16 - 1;
//     int maximum_malloc_size = 64 * 1024 - 16;
//     int* temp_free_list [num_arenas_remaining];

//     int failure = 0;

//     for (int i = 0; i < num_arenas_remaining; i++) {
//         // Debug::printf("%d\n", i);
//         temp_free_list[i] = (int*)malloc(maximum_malloc_size);
//         // Debug::printf("%x\n", temp_free_list[i]);
//         if (temp_free_list[i] == nullptr) {
//             failure++;
//         }
//     }

//     // Debug::printf("Failure number %d\n", failure);

//     if (failure > 0) {
//         Debug::printf("*** Bulk Arena Test failed with %d failures\n", failure);
//     } else {
//         int* final_malloc = (int*)malloc(100);
//         if (final_malloc == nullptr)
//             Debug::printf("*** Bulk Arena Test passed\n");
//         else {
//             Debug::printf("*** Bulk Arena Test failed, space remaining at %x\n", final_malloc);
//         }
//         free(final_malloc);
//     }

//     for(int i = 0; i < num_arenas_remaining; i++) {
//         // Debug::printf("%x\n", temp_free_list[i]);
//         free((void*)temp_free_list[i]);
//     }
// }

// void calculateSpaceUsage(void) {
//     double heap_size = 5 * 1024 * 1024;
//     // Debug::printf("calculate: %d\n", spaceRemaining());
//     double total_space_remaining = (double)spaceUnallocated();
//     Debug::printf("Space Usage: %f%%\n", ((heap_size - total_space_remaining) / heap_size)* 100);
// }