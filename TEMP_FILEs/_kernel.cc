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

#define BF_T(x, y) (Debug::printf("Best fit for %d: = %d\n", x, y))
#define END(x) (Debug::printf("*** END OF TEST %d *** \n\n", x))
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

template <typename T>
void do_tree_test_3 (T* arr, int arr_size, T* nodes_to_remove, int n_t_r_size) {
    AVL<T>* _avl = new AVL<T>();
    for (int i = 0; i < arr_size; ++i) {
        _avl->insert((T) arr[i]);
    }
    
    Debug::printf("After insertions: \n");
    print_bt(_avl->get_root(), 0);

    for (int i = 0; i < n_t_r_size; ++i) {
        _avl->remove((T) nodes_to_remove[i]);
    }
    Debug::printf("After deletions: \n");
    print_bt(_avl->get_root(), 0);
}

void do_tree_test_4() {
    AVL<uint32_t>* _avl = new AVL<uint32_t>();
    uint32_t arr[11] = {25, 20, 36, 10, 22, 30, 40, 12, 28, 38, 48};

    for (int i = 0; i < 11; ++i) {
        _avl->insert(arr[i]);
    }
    BF_T(25, _avl->best_fit(25)); // best fit should be 25
    BF_T(26, _avl->best_fit(26)); // best fit should be 28
    BF_T(27, _avl->best_fit(27)); // best fit should be 28
    BF_T(28, _avl->best_fit(28)); // best fit should be 28
    BF_T(29, _avl->best_fit(29)); // best fit should be 30
    BF_T(30, _avl->best_fit(30)); // best fit should be 30

    for (uint32_t i = 41; i < 49; ++i) {
        BF_T(i, _avl->best_fit(i)); // best fit should be 48
    }

    for (uint32_t i = 39; i < 41; ++i) {
        BF_T(i, _avl->best_fit(i)); // best fit should be 40
    }

    for (uint32_t i = 37; i < 39; ++i) {
        BF_T(i, _avl->best_fit(i)); // best fit should be 38
    }

    for (uint32_t i = 1; i < 25; ++i) {
        BF_T(i, _avl->best_fit(i)); // best fit should be:
	                            // 10 iff 1 <= i <= 10
				    // 12 iff 11 <= i <= 12
				    // 20 iff 13 <= i <= 20
				    // 22 iff 21 <= i <= 22
				    // 25 iff 23 <= i <= 24 (tbh 25 too)
    }
    // so overall: 25, 28, 28, 28, 30, 30, 48 X 8, 40, 40, 38, 38, 10 X 10, 12, 12, 20 X 8, 22, 22, 25, 25
}

void kernelMain(void) {
    {
	do_tree_test(3, 2, 1); // Right rotation test
	END(1);
        do_tree_test(1, 2, 3); // Left rotation test
	END(2);
        do_tree_test(3, 1, 2); // Left-Right rotation test
	END(3);
        do_tree_test(1, 3, 2); // Right-Left rotation test
	END(4);
        // for these 4 tests, result should be same for each:	
	// Level: 0 Node: xxxxxx, Height: 1, Data: 2, Left: yyyyyy, Right: zzzzzzz
        // Level: 1 Node: yyyyyy, Height: 0, Data: 1, Left: 0, Right: 0
        // Level: 1 Node: zzzzzz, Height: 0, Data: 3, Left: 0, Right: 0
	char data[] = {'M', 'N', 'O', 'L', 'K', 'Q', 'P', 'H', 'I', 'A'}; // big test.
	do_tree_test_2(data, 10);
	END(5);
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
        uint32_t a_1[] = {3, 2, 4, 1};
        uint32_t a_2[] = {4};
        do_tree_test_3(a_1, 4, a_2, 1);
        END(6);
	uint32_t a_3[] = {1, 2, 0, 3};
        uint32_t a_4[] = {0};
	do_tree_test_3(a_3, 4, a_4, 1);
        END(7);
	uint32_t a_5[] = {3, 1, 4, 2};
        uint32_t a_6[] = {4};
        do_tree_test_3(a_5, 4, a_6, 1);
        END(8);
	uint32_t a_7[] = {1, 3, 0, 2};
        uint32_t a_8[] = {0};
	do_tree_test_3(a_7, 4, a_8, 1);
        END(9);
	// for these 4 tests, after deletion result should be same for each (before is correct based on insertion logic):
        // Level: 0 Node: xxxxxx, Height: 1, Data: 2, Left: yyyyyy, Right: zzzzzzz
        // Level: 1 Node: yyyyyy, Height: 0, Data: 1, Left: 0, Right: 0
        // Level: 1 Node: zzzzzz, Height: 0, Data: 3, Left: 0, Right: 0
	
        do_tree_test_4();
	END(10);
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



