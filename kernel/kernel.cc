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

#include "avl.h"

const char* initName = "/sbin/init";

void print_bt(avl_node<uint32_t>* root, int level);


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

void kernelMain(void) {
    {
	AVL<uint32_t>* _avl = new AVL<uint32_t>();
        print_bt(_avl->get_root(), 0);
        _avl->insert((uint32_t) 5);
	print_bt(_avl->get_root(), 0);
	stop(); // temp end 
	auto ide = Shared<Ide>::make(1);
	auto fs = Shared<Ext2>::make(ide);	
	auto init = fs->open(fs->root, initName);
	auto pcb = Shared<PCB>{new Process(fs)};
	thread(pcb, [=]() mutable { SYS::exec(init, "init", 0); });
    }
    // Debug::printf("init exited with status %d\n",
    // 		  pcb->process()->exit_status->get());
    stop();
}


// functions for debugging and AVL tree unit tests
void print_avl_node (avl_node<uint32_t>* n) {
    Debug::printf("Height: %d, Data: %d, Left: %x, Right: %x", n->height, n->data, n->left, n->right);
}

void print_bt(avl_node<uint32_t>* root, int level) {
    if (root == nullptr) return;
    Debug::printf("Level: %d", level);
    print_avl_node(root);
    Debug::printf("\n");
    print_bt(root->left, level + 1);
    print_bt(root->right, level + 1);
}
