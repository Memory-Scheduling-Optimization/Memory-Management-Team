#include "stdint.h"
#include "debug.h"
#include "shared.h"
#include "threads.h"
#include "ext2.h"
#include "elf.h"
#include "vmm.h"
#include "process.h"
#include "barrier.h"
#include "sys.h"
#include "pit.h"

static void* blockPointersTest1[100];
static void* blockPointersTest2[1000];
void doEfficiencyTest(void);
void doLargeStressTest(void);
void doSpeedTest(void);
void memUtil(void);

void kernelMain(void) {

    Debug::printf("\n");
    Debug::printf("Start of simple stress/correctness tests\n");
    Debug::printf("Test 1\n");

    const int LOOPS = 10000;

    for (int i = 0; i < LOOPS; i++) {

	for (int i = 0; i < 100; i++) {
            blockPointersTest1[i] = malloc(100);
        }

        for (int i = 0; i < 100; i += 2) {
            free(blockPointersTest1[i]);
        }

        for (int i = 1; i < 100; i += 2) {
            free(blockPointersTest1[i]);
            void *p = malloc(LOOPS * 100);
            if (p == 0) Debug::panic("*** Unable to free every alternate block\n");
            else free(p);
        }
    }

    Debug::printf("*** Passed Test 1: freeing alternate blocks\n");

    Debug::printf("Test 2\n");

    for(int i = 0; i < 1000; i++) {
	blockPointersTest2[i] = malloc(1);
    }

    for(int i = 0; i < 1000; i++) {
	free(blockPointersTest2[i]);
    }

    void* p = malloc(10000);
    if (p == 0) Debug::panic("*** Unable to allocate large block after freeing many small allocations\n");
    else Debug::printf("*** Passed Test 2: allocating large block after freeing many small allocations\n");

    Debug::printf("Test 3\n");

    void* p2 = malloc(0);
    if (p2 == 0) Debug::panic("*** Unable to malloc(0)\n");
    else {
	free(p2);
	Debug::printf("*** Passed test 3: malloc(0) and free(0)\n");
    }

    Debug::printf("End of simple stress/correctness tests\n");

    Debug::printf("\n");

    Debug::printf("Start of efficiency test\n");
    doEfficiencyTest();
    Debug::printf("End of efficiency test\n");
    
    Debug::printf("\n");

    Debug::printf("Start of big malloc stress test\n");
    doLargeStressTest();
    Debug::printf("End of big malloc stress test\n");

    Debug::printf("\n");

    Debug::printf("Start of speed test\n");
    doSpeedTest();
    Debug::printf("End of speed test\n");

    Debug::printf("\n");

    Debug::printf("Memory utilization calculation\n");
    memUtil();

}

//len in next fit's kernel.cc was 10000
//but worst fit is apparently not as space efficient 
//as I though
void doEfficiencyTest() {

    constexpr uint32_t len = 1000;
    void** arr = new void*[len];

    //A bunch of large mallocs
    constexpr size_t large_bytes = 1000;
    for (uint32_t i = 0; i < len/3; ++i) {
        arr[i] = malloc(large_bytes);
    }

    //Free up some of the large blocks
    for (uint32_t i = 0; i < len/12; ++i) {
        free(arr[i]);
    }

    //A bunch of relatively small mallocs
    constexpr size_t small_bytes = 500;
    for (uint32_t i = len/3; i < (len * 2)/3; ++ i) {
        arr[i] = malloc(small_bytes);
    }

    //Some relatively big mallocs
    auto medium_bytes = (large_bytes * 3) / 4;
    for (uint32_t i = (len * 2)/3; i < len; ++ i) {
        arr[i] = malloc(medium_bytes);
    }

    for (uint32_t i = len/12; i < len; ++ i) {
        free(arr[i]);
    }

    Debug::printf("*** Passed space efficiency test\n");

}

void doLargeStressTest() {

    void** arr = new void*[100];
    constexpr size_t large_bytes = 10000;

    for (uint32_t i = 0; i < 100; ++ i) {
	arr[i] = malloc(large_bytes);
    }

    for (uint32_t i = 0; i <= 50; ++ i) {
        free(arr[i]);
    }

    for (uint32_t i = 99; i > 50; --i) {
        free(arr[i]);
    }

    free(arr);
    
    Debug::printf("*** Passed big malloc stress test\n");

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

    Debug::printf("*** Time taken for space efficiency test: %d\n", x);

}

void memUtil() {
    double heapSize = 5 * 1024 * 1024;
    double totalAmountUnallocated = (double) spaceUnallocated();
    Debug::printf("*** Memory utilized = %f%%\n", ((heapSize - totalAmountUnallocated)/heapSize)*100);
}


