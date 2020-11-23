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

void basicArenaTest(void);
void bulkArenaTest(void);

void kernelMain(void) {
    // Ethan's old code

    // {
	// auto ide = Shared<Ide>::make(1);
	// auto fs = Shared<Ext2>::make(ide);	
	// auto init = fs->open(fs->root, initName);
	// auto pcb = Shared<PCB>{new Process(fs)};
	// thread(pcb, [=]() mutable { SYS::exec(init, "init", 0); });
    // }
    // // Debug::printf("init exited with status %d\n",
    // // 		  pcb->process()->exit_status->get());
    // stop();

    Debug::printf("*** Start of kernelMain()\n");

    // Test for basic functionality of malloc and free
    basicArenaTest();

    // Testing to make sure we can allocate everything we should be able to
    bulkArenaTest();

    // Do twice to make sure we are freeing properly
    bulkArenaTest();

    Debug::printf("*** End of kernelMain()\n");
}

void checkAlignment(void* p) {
    if (((int)p & 3) != 0) {
        Debug::printf("*** Alignment failure at %x\n", p);
    }
}

void basicArenaTest(void) {
    int* temp = (int*)malloc(4);
    checkAlignment((void*)temp);
    *temp = 420;

    int* temp2 = (int*)malloc(26);
    checkAlignment((void*)temp2);
    *temp2 = 0xFFFFFFFF;

    if (*temp != 420) {
        Debug::printf("*** Basic Arena Test failed\n");
    } else {
        Debug::printf("*** Basic Arena Test passed\n");
    }

    free(temp2);
    free(temp);
}

void bulkArenaTest(void) {
    // In theory, there should be 1 fully allocated arena, 
    // 1 partially allocated, rest unallocated
    Debug::printf("Bulk Arena Test started\n");

    int num_arenas_remaining = 5 * 64 - 2;
    int maximum_malloc_size = 16 * 1024 - 16;
    int* temp_free_list [num_arenas_remaining];

    int failure = 0;

    for (int i = 0; i < num_arenas_remaining; i++) {
        temp_free_list[i] = (int*)malloc(maximum_malloc_size);
        if (temp_free_list[i] == nullptr) {
            failure++;
        }
    }

    if (failure > 0) {
        Debug::printf("*** Bulk Arena Test failed with %d failures\n", failure);
    } else {
        int* final_malloc = (int*)malloc(100);
        if (final_malloc == nullptr)
            Debug::printf("*** Bulk Arena Test passed\n");
        else {
            Debug::printf("*** Bulk Arena Test failed, space remaining at %x\n", final_malloc);
        }
        free(final_malloc);
    }

    for(int i = 0; i < num_arenas_remaining; i++) {
        // Debug::printf("%x\n", temp_free_list[i]);
        free((void*)temp_free_list[i]);
    }
}