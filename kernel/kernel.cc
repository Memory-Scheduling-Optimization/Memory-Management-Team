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

void doGroupAllocation(void);
void doRandomAllocation(void);
void doRandomGroupAllocation(void);

void kernelMain(void) {

    // Debug::printf("Start of Timing test\n");
    // int number_of_millions = 10; // 10 --> 10 million (1 of 10 mils)

    // for (int i = 0; i < 10 * number_of_millions; i++) { // i = 100 for 10 million
    //     doTimingTest();
    // }
    // Debug::printf("End of Timing test\n");


    // Debug::printf("Start of Group test\n");
    // for (int i = 0; i < 10000; i++) {
    //     doGroupAllocation();
    // }
    // Debug::printf("End of Group test\n");


    // Debug::printf("Start of Random test\n");
    // for (int i = 0; i < 100; i++) {
    //     doRandomAllocation();
    // }
    // Debug::printf("End of Random test\n");

    doTimingTest();
    Debug::printf("Start of Timing test\n");
    doTimingTest();

    Debug::printf("\nStart of Random test\n");
    doRandomAllocation();

    Debug::printf("\nStart of Random Group test\n");
    doRandomGroupAllocation();
}

void doTimingTest() {
    int** arr = new int*[100000];
    auto r = new Random(3487);

    for (uint32_t i = 0; i < 100000; i++) {
        arr[i] = (int*)malloc((r->next() % 32) + 1);
    }
    memUtil();

    for (uint32_t i = 0; i < 100000; i++) {
        free(arr[i]);
    }
    
    free(arr);
    delete(r);
    //memUtil();
    // Debug::printf("\n");
}

void doGroupAllocation() {
    uint32_t groupsize = 1000;

    int** arr = new int*[groupsize];
    auto r = new Random(3487);

    for (uint32_t i = 0; i < groupsize; i++) {
        arr[i] = (int*)malloc((r->next() % 32) + 1);
    }

    // memUtil();

    for (uint32_t i = 0; i < groupsize; i++) {
        free(arr[i]);
    }

    free(arr);
    delete(r);
}

void doRandomAllocation() {
    uint32_t groupsize = 100000;

    int** arr = new int*[groupsize];
    auto r = new Random(3487);

    for (uint32_t i = 0; i < groupsize; i++) {
        arr[i] = (int*)malloc((r->next() % 32) + 1);
        if ((r->next() % 2) == 0) {
            free(arr[i]);
            arr[i] = 0;
        }
    }

    memUtil();

    for (uint32_t i = 0; i < groupsize; i++) {
        if (arr[i] != 0)
            free(arr[i]);
    }

    free(arr);
    delete(r);
}

void doRandomGroupAllocation() {
    uint32_t groupsize = 100000;
    uint32_t minigroupsize = 1000;
    uint32_t groupnum = 100;

    int** arr = new int*[groupsize];
    auto r = new Random(3487);

    uint32_t index = 0;
    for (uint32_t i = 0; i < groupnum; i++) {
        for (uint32_t ingroup = 0; ingroup < minigroupsize; ingroup++) {
            arr[index] = (int*)malloc((r->next() % 32) + 1);
            index++;
        }
        if ((r->next() % 2) == 0) {
            for (uint32_t removegroup = index - minigroupsize; removegroup < index; removegroup++) {
                free(arr[removegroup]);
                arr[removegroup] = 0;
            }
        }
    }

    memUtil();

    for (uint32_t i = 0; i < groupsize; i++) {
        if (arr[i] != 0)
            free(arr[i]);
    }

    free(arr);
    delete(r);
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