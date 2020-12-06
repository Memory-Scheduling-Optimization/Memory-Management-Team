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
#include "random.h"

void doTimingTest(void);
void doSpeedTest(void);
void doTimingSpeedTest(void);
void memUtil(void);

void kernelMain(void) {

    Debug::printf("Start of Timing test\n");
    int number_of_millions = 10; // 10 --> 1 million (1 of 10 mils)

    for (int i = 0; i < 1 * number_of_millions; i++) { // i = 100 for 10 million
        doTimingTest();
    }
    Debug::printf("End of Timing test\n");
}

void doTimingTest() {
    int** arr = new int*[100000];
    auto r = new Random(3487);

    for (uint32_t i = 0; i < 100000; i++) {
	int temp = (r->next() % 32);
        arr[i] = (int*)malloc(temp + 1);
    }
    memUtil();
    for (uint32_t i = 0; i < 100000; i++) {
        free(arr[i]);
    }
    
    free(arr);
    delete(r);
    //memUtil();
}

template <typename Work>
uint32_t howLong(Work work) {
    uint32_t start = Pit::jiffies;
    work();
    return (Pit::jiffies - start);
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
