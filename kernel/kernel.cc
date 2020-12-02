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

const char* initName = "/sbin/init";

void printFile(Shared<Node> file) {
    if (file == nullptr) {
	Debug::printf("file does not exist\n");
	return;
    }
    auto buffer = new char[file->size_in_bytes()];
    file->read_all(0, file->size_in_bytes(), buffer);
    for (uint32_t i = 0; i < file->size_in_bytes(); i++)
	Debug::printf("%c", buffer[i]);
    delete[] buffer;
}

void segregatedBasicTest(void);
void segregatedRightCoalesceTest(void);
void segregatedLeftCoalesceTest(void);
void segregatedBothCoalesceTest(void);

void kernelMain(void) {
    Debug::printf("*** Start of kernelMain()\n");
    
    segregatedBasicTest();

    segregatedRightCoalesceTest();

    segregatedLeftCoalesceTest();

    segregatedBothCoalesceTest();
}

void checkAlignment(void* p) {
    if (((int)p & 3) != 0) {
        Debug::printf("*** Alignment failure at %x\n", p);
    }
}

void segregatedBasicTest(void) {
    int* temp = (int*)malloc(4);
    checkAlignment((void*)temp);
    *temp = 420;

    int* temp2 = (int*)malloc(26);
    checkAlignment((void*)temp2);
    *temp2 = 0xFFFFFFFF;

    if (*temp != 420) {
        Debug::printf("*** Basic Segregated Test failed\n");
    } else {
        Debug::printf("*** Basic Segregated Test passed\n");
    }

    free(temp2);
    free(temp);
}

void segregatedRightCoalesceTest(void) {
    // Check right coalescing
    int* temp = (int*)malloc(16);
    int* temp2 = (int*)malloc(16);
    int* temp3 = (int*)malloc(16);

    free(temp2);
    free(temp);

    int* temp4 = (int*)malloc(24);

    if (temp == temp4) {
        Debug::printf("*** Coalesce right Segregated Test passed\n");
    } else {
        Debug::printf("*** Coalesce right Segregated Test failed %x\t%x\n", temp, temp4);
    }
    free(temp3);
    free(temp4);
}

void segregatedLeftCoalesceTest(void) {
    // Check left coalescing
    int* temp = (int*)malloc(16);
    int* temp2 = (int*)malloc(16);
    int* temp3 = (int*)malloc(16);

    free(temp);
    free(temp2);

    int* temp4 = (int*)malloc(24);

    if (temp == temp4) {
        Debug::printf("*** Coalesce left Segregated Test passed\n");
    } else {
        Debug::printf("*** Coalesce left Segregated Test failed %x\t%x\n", temp, temp4);
    }
    free(temp3);
    free(temp4);
}

void segregatedBothCoalesceTest(void) {
    // Check coalescing on both sides
    int* temp = (int*)malloc(16);
    int* temp2 = (int*)malloc(16);
    int* temp3 = (int*)malloc(32);
    int* temp4 = (int*)malloc(16);

    free(temp);
    free(temp3);
    free(temp2);
    
    int* temp5 = (int*)malloc(56);

    if (temp == temp5) {
        Debug::printf("*** Coalesce both Segregated Test passed\n");
    } else {
        Debug::printf("*** Coalesce both Segregated Test failed %x\t%x\n", temp, temp5);
    }
    free(temp4);
    free(temp5);
}