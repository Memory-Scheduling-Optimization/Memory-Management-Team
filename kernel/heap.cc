/*
 * heap
 */

#include "heap2.h"
#include "debug.h"
#include "stdint.h"
#include "atomic.h"
#include "blocking_lock.h"

#define height(n) ((n == nullptr) ? (-1) : (n->height))
#define balance(n) ((n == nullptr) ? (0) : (height(n->left_child) - height(n->right_child)))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define infinity 2147483647
#define GET_DATA(n) ((n == nullptr) ? (infinity) : (n->get_block_size()))

struct Header;
struct Footer;

static void* heap_start = nullptr;
static void* heap_end = nullptr;
static size_t heap_size = 0;
static BlockingLock *heap_lock = nullptr;
static Header* avail_list = nullptr;
static int const MIN_NODE_SIZE = 20;
static int const MIN_BLOCK_SIZE = 12;
static int const NODE_OVERHEAD = 8;

template <typename T>
inline T abs(T v) {
    return (v < 0) ? -v : v;
}

template <typename DataType, typename ValueType>
inline DataType* ptr_add(DataType* ptr, ValueType value) { // for pointer arithmetic
    return (DataType*) (((uintptr_t) ptr) + value);
}

template <typename T>
inline T round_down_mult_four(T v) { // rounds down number to a multiple of four
    size_t val = reinterpret_cast<size_t>(v);
    val = (val % 4 == 0) ? val : (val - (val % 4));
    return reinterpret_cast<T>(val);
}

template <typename T>
inline T round_up_mult_four(T v) { // rounds up number to a multiple of four
    size_t val = reinterpret_cast<size_t>(v);
    val = (val % 4 == 0) ? val : (val + 4 - (val % 4));
    return reinterpret_cast<T>(val);
}

inline int32_t get_negative(int32_t val) {
    return -val;
}

struct Header {
    int32_t size_and_state;
    Header* left_child;
    int32_t height;
    Header* right_child;

    inline bool is_allocated() {
        return size_and_state >= 0; // zero means allocated
    }

    inline int32_t get_block_size() {
        return abs(size_and_state);
    }

    inline void* get_block() {
        return ptr_add((void*)this,sizeof(size_and_state)); //sizeof(size_and_state) = 4 bytes
    }

    inline Footer* get_footer() {
        return (Footer*)ptr_add(get_block(),get_block_size());
    }

    inline Header* get_right_header() { // header of block to the right
        return (Header*)ptr_add(get_footer(),4);
    }

    inline Footer* get_left_footer() { // footer of block to the left
        return (Footer*)ptr_add(this,-4);
    }
};

struct Footer {
     int32_t size_and_state;

     inline bool is_allocated() {
         return size_and_state >= 0; // zero means allocated
     }

     inline int32_t get_block_size() {
         return abs(size_and_state);
     }

     inline void* get_block() {
        return ptr_add((void*)this, -get_block_size()); //addr_block = addr_footer - block_size
     }

     inline Header* get_header() {
         return (Header*)ptr_add(get_block(), -sizeof(size_and_state));
     }
};


void print_heap() {
    Header* h = (Header*) heap_start;
    while (h < heap_end) {
	Debug::printf("*** (%p):  %d ", h, h->size_and_state);
        h = h -> get_right_header();
    }
    Debug::printf("*** \n");
}


void sanity_checker() {
    Header* current_node = (Header*) heap_start;

    while (current_node < heap_end) {

	// error conditions
        if(current_node->size_and_state != current_node->get_footer()->size_and_state) {
	    print_heap();
	    Debug::panic("Node at address %p has mismatched header(%d) and footer(%d)", current_node, current_node->size_and_state, current_node->get_footer()->size_and_state);
	}
        else if (((int32_t)current_node) % 4 != 0) {
	    print_heap();
	    Debug::panic("Node has misaligned address at %p", current_node);
	}
	else if (current_node < heap_start || current_node >= heap_end) {
	    print_heap();
	    Debug::panic("Node is outside heap bounds (%p)", current_node);
	}
	else if (current_node -> get_right_header() <= current_node) {
	    print_heap();
	    Debug::panic("Cycle found (%p)", current_node);
        }
        else if (current_node -> size_and_state == 0 || current_node -> size_and_state == 4 || current_node -> size_and_state == -4 || current_node -> size_and_state == 8 || current_node -> size_and_state == -8) {
	    print_heap();
            Debug::panic("Node with size_and_state = %d (this is not allowed) was found at (%p)!", current_node -> size_and_state, current_node);
	}
	current_node = current_node -> get_right_header(); // next node in the heap
    }
}

void heapInit(void* base, size_t bytes) { 
    heap_lock = new BlockingLock(); 
    heap_start = round_up_mult_four(base);
    heap_size = round_down_mult_four(bytes);
    //Debug::printf("heap_start: %d, heap_size: %d \n", heap_start, heap_size);
    heap_end = ptr_add((void*) heap_start, heap_size);

    if (heap_size >= MIN_NODE_SIZE) { // if there is room for anything in the heap
        Header* middle_node = (Header*)heap_start;
        middle_node->size_and_state = get_negative(heap_size - NODE_OVERHEAD);
        middle_node->get_footer()->size_and_state = get_negative(heap_size - NODE_OVERHEAD);
	middle_node->left_child = nullptr;
        middle_node->right_child = nullptr;
	middle_node->height = 0; // since it's a leaf of the tree
	avail_list = middle_node; // set the root of the AVL tree  
    }
    //sanity_checker();
}

Header* rotate_right(Header* n) {
    auto left_node = n->left_child;
    n->left_child = left_node->right_child;
    left_node->right_child = n;
    n->height = 1+MAX(height(n->left_child), height(n->right_child));
    left_node->height = 1+MAX(height(left_node->left_child), height(left_node->right_child));
    return left_node;
}

Header* rotate_left(Header* n) {
    auto right_node = n->right_child;
    n->right_child = right_node->left_child;
    right_node->left_child = n;
    n->height = 1+MAX(height(n->left_child), height(n->right_child));
    right_node->height = 1+MAX(height(right_node->left_child), height(right_node->right_child));
    return right_node;
}

Header* rotate_left_right(Header* n) {
    n->left_child = rotate_left(n->left_child);
    return rotate_right(n);
}

Header* rotate_right_left(Header* n) {
    n->right_child = rotate_right(n->right_child);
    return rotate_left(n);
}

Header* adjust(Header* cur) {
    cur->height = 1+MAX(height(cur->left_child), height(cur->right_child));
    const int thresh = 1; // threshold of 1 for now
    if (balance(cur) > thresh) {
        if (balance(cur->left_child) >= 0) {
            cur = rotate_right(cur);
         } else { // balance(cur->left) < 0
             cur = rotate_left_right(cur);
         }
    } else if (balance(cur) < -thresh) {
        if (balance(cur->right_child) <= 0) {
            cur = rotate_left(cur);
        } else {
            cur = rotate_right_left(cur);
        }
    }
    return cur;
}

Header* best_fit_help(Header* cur, int32_t target) {
    if (cur == nullptr) return nullptr;
    if (cur->get_block_size() < target) return best_fit_help(cur->right_child, target); // too small so it doesn't work
    if (cur->get_block_size() > target) {
        Header* a_try = best_fit_help(cur->left_child, target);
        return (cur->get_block_size() < GET_DATA(a_try)) ? (cur) : (a_try);
    }
    return cur; // first found; perfect match
}

Header* insert_help(Header* cur, Header* node) {
    if (cur == nullptr) {
        return node;
    }
    if (node->get_block_size() < cur->get_block_size()) {
        cur->left_child = insert_help(cur->left_child, node); // recursive call
    } else {
        cur->right_child = insert_help(cur->right_child, node); // recursive call
    }
    return adjust(cur);
}


Header* remove_help(Header* cur, Header* node) {
        if (cur == nullptr) return nullptr;
	// val replaced with node->get_block_size() OR node
	// cur->data replaced with cur->get_block_size()
	// cur->left replaced with cur->left_child
	// cur->right replaced with cur->right_child
	// cur->height is still cur->height
        if (node->get_block_size() < cur->get_block_size()) cur->left_child = remove_help(cur->left_child, node);
        else if (node->get_block_size() > cur->get_block_size()) cur->right_child = remove_help(cur->right_child, node);
        // o.w. it's a match
        else {
            if (cur->left_child == nullptr || cur->right_child == nullptr) {
                if (cur->left_child == nullptr && cur->right_child == nullptr) { // leaf case
                    cur = nullptr;
                } else {
                    Header* nonempty = (cur->left_child == nullptr) ? cur->right_child : cur->left_child;
                    cur = nonempty; // gonna update parent's pointers since it returns cur from this function
                }
            } else {
                // two children case
                Header* replacement = cur->right_child;
                
                while (replacement->left_child != nullptr) { // get min value in cur's right subtree
                    replacement = replacement->left_child;
                }
                cur->right_child = remove_help(cur->right_child, replacement); // recursive call to remove replacement from the right subtree 
		replacement->right_child = cur->right_child;
		replacement->left_child = cur->left_child;
                cur = replacement; // for updating cur's parent's pointer to cur to now point to replacement
            }
        }

        if (cur == nullptr) return cur; // if cur now is nullptr, can just return
        return adjust(cur);
}

Header* get_best_fit (int32_t val) {
        Header* h = best_fit_help(avail_list, val);
        return h; // could be nullptr
}

void add_to_tree(Header* node) {
    avail_list = insert_help(avail_list, node);
}

void remove_from_tree(Header* node) {
    avail_list = remove_help(avail_list, node);
}


void* malloc(size_t bytes) { //using best fit policy
    //Debug::printf("In malloc, bytes = %d \n", bytes);
    LockGuardP g{heap_lock};
    //sanity_checker();
    //print_heap();
    bytes = round_up_mult_four(bytes); // extra bytes if mallocing an amount that is not a multiple of four
    if (bytes >= 4 && bytes < MIN_BLOCK_SIZE) {
        bytes = MIN_BLOCK_SIZE; // for the minimum block size, so round up 4 & 8 to 12.
    }
    if (bytes == 0) {// malloc(0) special case
        return ptr_add(heap_end, 4); // returns out of bounds pointer.
    }
    if (avail_list == nullptr) { // case where we know that there are no free nodes
	return nullptr;
    }
    if (bytes > (heap_size - NODE_OVERHEAD)) { // because heap_size is actually an overestimation already (since header + footer)
        return nullptr;
    }
    //Header* current_node = avail_list;
    Header* best_fit_node = get_best_fit((int32_t) bytes);
    if (best_fit_node == nullptr) return nullptr;
    
    if (abs(best_fit_node -> size_and_state) >= (ssize_t)(MIN_BLOCK_SIZE + bytes + 8)) { // leftover header & footer from old node + min_block_size needed for new free node + len(new allocated node)=8+bytes,
        int32_t leftover_bytes = abs(best_fit_node -> size_and_state) + 8 - bytes - 8 - MIN_BLOCK_SIZE; 
	ASSERT(leftover_bytes >= 8); // leftover_bytes >= 8, still, by algebra
        Header* new_header = (Header*)ptr_add(best_fit_node->get_footer(), get_negative(bytes + 4)); // this indicates we malloc from RHS
        new_header->size_and_state = bytes; // bytes > 0
        new_header->get_footer()->size_and_state = bytes;
        
	remove_from_tree(best_fit_node);
        best_fit_node->size_and_state = get_negative(leftover_bytes); // <-- v <= -4, so this node is still in the avail list so this works.
        best_fit_node->get_footer()->size_and_state = get_negative(leftover_bytes);
	add_to_tree(best_fit_node);
        //print_heap();
        return new_header->get_block(); // since new_header is pointing to the allocated portion
    }
    else { // case where exact fit or 4/8/12/16 extra bytes more than exact fit
	best_fit_node -> size_and_state *= -1; //indicate that it's now full
	best_fit_node -> get_footer() -> size_and_state *= -1;
	remove_from_tree(best_fit_node); // since it's no longer available
        //print_heap();
        return best_fit_node->get_block();
    }
}

void free(void* p) {
    LockGuardP g{heap_lock}; 
    //Debug::printf("In free, p = %x \n", p);
    //sanity_checker();
    //print_heap();
    Header* node = (Header*)ptr_add(p, -4); // because user gives the pointer that is the start of the block

    if (p < heap_start || p > heap_end || ((int32_t)(p)) % 4 != 0 || !node->is_allocated()) { // cases where free will do nothing
        return;
    }

    Header* left_node = (node -> get_left_footer() < heap_start) ? nullptr : node->get_left_footer()->get_header();
    Header* right_node = (node -> get_right_header() >= heap_end) ? nullptr : node->get_right_header(); // heap_end is a multiple of 4, we can't have a right node start from there
    if (!left_node->is_allocated() || !right_node->is_allocated()) {
        if (!left_node->is_allocated() && !right_node->is_allocated()) { // left and right nodes are free
	    int32_t new_block_size = get_negative(abs(node->size_and_state) + abs(left_node->size_and_state) + 8 + abs(right_node->size_and_state) + 8);
	    remove_from_tree(left_node);
	    remove_from_tree(right_node);
            left_node->size_and_state = new_block_size;
	    right_node->get_footer()->size_and_state = new_block_size;
	    add_to_tree(left_node);
	    return;
	}
	Header* leftmost = (!left_node->is_allocated()) ? left_node : node;
	Header* rightmost = (!right_node->is_allocated()) ? right_node : node;
	ASSERT((leftmost == node || rightmost == node) && !(leftmost == node && rightmost == node)); //exactly one of them is node
	Header* not_node = (leftmost == node) ? (rightmost) : (leftmost); // the one that isn't node
        int32_t new_block_size = get_negative(abs(leftmost->size_and_state) + abs(rightmost->size_and_state) + 8);
        remove_from_tree(not_node);
        leftmost->size_and_state = new_block_size;
        rightmost->get_footer()->size_and_state = new_block_size;
        add_to_tree(leftmost);
        return;	
    }
    // o.w. right is allocated and left is allocated
    add_to_tree(node);
    node->size_and_state *= -1; // negative number is free anyways
    node->get_footer()->size_and_state *= -1;
    //sanity_checker();
    //print_heap();
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

