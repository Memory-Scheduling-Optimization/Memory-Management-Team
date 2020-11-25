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

// functions for debugging and AVL tree unit tests
template <typename T>
void print_avl_node (avl_node<T>* n) {
    Debug::printf("Node: %x, Height: %d, Data: %d = %c, Left: %x, Right: %x", n, n->height, n->data, n->data, n->left, n->right);
}

template <typename T>
void print_bt(avl_node<T>* root, int level) {
    if (root == nullptr) return;
    Debug::printf("Level: %d ", level);
    print_avl_node(root);
    Debug::printf("\n");
    print_bt(root->left, level + 1);
    print_bt(root->right, level + 1);
}

void do_tree_test(int n1, int n2, int n3) {
    AVL<uint32_t>* _avl = new AVL<uint32_t>();
    _avl->insert((uint32_t) n1);
    _avl->insert((uint32_t) n2);
    _avl->insert((uint32_t) n3);
    print_bt(_avl->get_root(), 0);
}


template <typename T>
void do_tree_test_2 (T* arr, int arr_size) {
    AVL<T>* _avl = new AVL<T>();
    
    for (int i = 0; i < arr_size; ++i) {
        _avl->insert((T) arr[i]);
    }
    print_bt(_avl->get_root(), 0);
}

void kernelMain(void) {
    {
	do_tree_test(3, 2, 1); // Right rotation test
	Debug::printf("*** END OF TEST ONE *** \n\n");
        do_tree_test(1, 2, 3); // Left rotation test
	Debug::printf("*** END OF TEST TWO *** \n\n");
        do_tree_test(3, 1, 2); // Left-Right rotation test
	Debug::printf("*** END OF TEST THREE *** \n\n");
        do_tree_test(1, 3, 2); // Right-Left rotation test
	Debug::printf("*** END OF TEST FOUR *** \n\n");
        // for these 4 tests, result should be same for each:	
	// Level: 0 Node: xxxxxx, Height: 1, Data: 2, Left: yyyyyy, Right: zzzzzzz
        // Level: 1 Node: yyyyyy, Height: 0, Data: 1, Left: 0, Right: 0
        // Level: 1 Node: zzzzzz, Height: 0, Data: 3, Left: 0, Right: 0
	char data[] = {'M', 'N', 'O', 'L', 'K', 'Q', 'P', 'H', 'I', 'A'}; // big test.
	do_tree_test_2(data, 10);
	Debug::printf("*** END OF TEST FIVE *** \n\n");
	// this test has the result: 
	// Level: 0 Node: <N>, Height: 3, Data: 78 = N, Left: <I>, Right: <P>
        // Level: 1 Node: <I>, Height: 2, Data: 73 = I, Left: <H>, Right: <L>
        // Level: 2 Node: <H>, Height: 1, Data: 72 = H, Left: <A>, Right: 0
        // Level: 3 Node: <A>, Height: 0, Data: 65 = A, Left: 0, Right: 0
        // Level: 2 Node: <L> Height: 1, Data: 76 = L, Left: <K>, Right: <M>
        // Level: 3 Node: <K>, Height: 0, Data: 75 = K, Left: 0, Right: 0
        // Level: 3 Node: <M>, Height: 0, Data: 77 = M, Left: 0, Right: 0
        // Level: 1 Node: <P>, Height: 1, Data: 80 = P, Left: <O>, Right: <Q>
        // Level: 2 Node: <O>, Height: 0, Data: 79 = O, Left: 0, Right: 0
        // Level: 2 Node: <Q>, Height: 0, Data: 81 = Q, Left: 0, Right: 0

	Debug::shutdown(); // temp end
        	
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



