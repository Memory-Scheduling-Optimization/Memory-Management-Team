/*
 * heap
 */

#include "heap.h"
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
Header* slabs[4] = {nullptr, nullptr, nullptr, nullptr}; // 4 for now
int num_slab_lists = 0;
const int max_num_slab_lists = 4;

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


// usage: cast a Header* into a Slab for convenience
// ex: Slab* slb = (Header*) node;
struct Slab { // TODO
    int32_t size_and_state;
    uint32_t bitmap; // using extra 8 bytes of data (actual data part of a malloc())
    Header* next;
    
    // returns true if this slab node is full, false otherwise
    inline bool is_full() {
         return bitmap == 0xFFFFFFFF;
    }
    
    // there are 32 blocks, and we need to subtract 8 for the size_and_state plus the bitmap
    inline uint32_t get_block_size() {
        return (abs(size_and_state) - 8) / 32;
    }
    
   // returns a void* ptr to the start of the 32 contigious blocks 
    inline void* get_blocks_start() {
        return ptr_add((void*)this, 12); //4 for size_and_state, 4 for bitmap, 4 for next
    }
    
    // returns a void* ptr to the address right after the 32nd block ends
    inline void* get_blocks_end() {
        return ptr_add(get_blocks_start(), 32 * get_block_size()); // 32 blocks, each of size get_block_size()
    }

    // returns a void* ptr to the block in this slab at index i (zero-indexed)
    inline void* get_a_block(int i) { 
	uint32_t offs = get_block_size() * i;
        return ptr_add(get_blocks_start(), offs); 
    }

    inline bool block_is_free(int i) { // zero-based indexing. true => it's free, false => it's not free.
	ASSERT(i >= 0 && i < 32);
        return (bitmap & (1 << ((uint32_t)i))) == 0;
    }

    // returns index of earliest free block found
    // returns -1 if no free block is found
    inline int32_t get_free_block() {
        uint32_t cur = bitmap;

	for (int32_t i = 0; i < 32; i++) {
            if ((cur & 0x1) == 0) { // if free block found
	        return i;
	    }
	    cur = cur >> 1;
	}
	ASSERT(is_full());
	return -1; // no free blocks found
    }

    // sets bit at bit index i 
    // precondition: bit index i was previously zero
    inline void set_bit(uint32_t i) {
        ASSERT(i >= 0 && i < 32 && ((bitmap & (1 << i)) == 0));
        bitmap |= 1 << i; // <-- 2^i
        ASSERT((bitmap & (1 << i)) != 0); // make sure that it worked
    }

    // zeroes bit at bit index i
    // precondition: bit index i was previously one
    inline void free_bit(uint32_t i) {
        ASSERT(i >= 0 && i < 32 && ((bitmap & (1 << i)) != 0));
        bitmap &= ~(1 << i); // <-- 2^i complement
        ASSERT((bitmap & (1 << i)) == 0); // make sure that it worked
    }
    
    // finds the index of block that point p is pointing to (assuming p has to be start of the block)
    // returns -1 if p is not part of this slab-node i.e. p is not in any of the blocks
    inline int32_t find_index_of_block (void* p) {
	ASSERT(((int32_t)p) % 4 == 0);
        if (p < get_blocks_start() || p >= get_blocks_end()) {
	    return -1; 
	}
        int32_t result = (((int32_t) p) - ((int32_t)get_blocks_start())) / get_block_size();
        ASSERT(result >= 0 && result < 32); // make sure that the result is in range	
	return result;
    }
};


void* nolock_malloc(size_t bytes);
void nolock_free(void* p);

void print_heap() {
    Header* h = (Header*) heap_start;
    int i = 0;
    while (h < heap_end) {
	Debug::printf("*** (%p):  %d ", h, h->size_and_state);
        h = h -> get_right_header();
	i++;
	if (i % 10 == 9) Debug::printf("\n");
    }
    Debug::printf("*** \n");
}

void print_slab(Slab* s) {
    if (s == nullptr) return;
    Debug::printf("Slab: %x, is_full: %d, get_block_size: %d, get_blocks_start: %p, get_blocks_end: %p, Bitmap: %x, Next: %p,", s, s->is_full(), s->get_block_size(), s->get_blocks_start(), 
		    s->get_blocks_end(), s->bitmap, s->next);
/*
    for (int i = 0; i < 32; i++) {
        Debug::printf(" i: %d, %p, data: %x\n", i, s->get_a_block(i), *((uint32_t*) s->get_a_block(i)));
    }*/
    Debug::printf("\n");
}

void print_slab_lists() {
    for (int i = 0; i < 4; i++) { // max 4 slab lists for now
        Header* cur = slabs[i];
	while (cur != nullptr) {
	    Slab* s = (Slab*) cur;
	    print_slab(s);
	    cur = s->next;
	}
    }
}

void print_tree(Header* n) {
    if (n == nullptr) return;
    Debug::printf("Header: %x, Height: %d, Data: %d, Left: %x, Right: %x\n", n, n->height, n->get_block_size(), n->left_child, n->right_child);
    print_tree(n->left_child);
    print_tree(n->right_child);
}


int size_of_tree(Header* n) {
    if (n == nullptr) return 0;
    return 1 + size_of_tree(n->left_child) + size_of_tree(n->right_child);
}

int spaceUnallocated_() { // from heap's data
    int totSpace = 0;
    Header* current_node = (Header*) heap_start;

    while (current_node < heap_end) {
	if (!current_node->is_allocated()) totSpace += current_node->get_block_size(); // if free current node, add amount of free space to totSpace
        current_node = current_node->get_right_header(); // next node in the heap
    }
    return totSpace;
}

int spaceUnallocated_from_tree(Header* n); 

int spaceUnallocated() {
    return spaceUnallocated_from_tree(avail_list);
}

int spaceUnallocated_from_tree(Header* n) { // from tree's data
    if (n == nullptr) return 0;
    return n->get_block_size() + spaceUnallocated_from_tree(n->left_child) + spaceUnallocated_from_tree(n->right_child);
}

void sanity_checker() {
    Header* current_node = (Header*) heap_start;
    int x = 0;

    while (current_node < heap_end) {

	// error conditions
        if(current_node->size_and_state != current_node->get_footer()->size_and_state) {
	    print_heap();
	    print_tree(avail_list);
	    Debug::panic("Node at address %p has mismatched header(%d) and footer(%d)", current_node, current_node->size_and_state, current_node->get_footer()->size_and_state);
	}
        else if (((int32_t)current_node) % 4 != 0) {
	    print_heap();
	    print_tree(avail_list);
	    Debug::panic("Node has misaligned address at %p", current_node);
	}
	else if (current_node < heap_start || current_node >= heap_end) {
	    print_heap();
	    print_tree(avail_list);
	    Debug::panic("Node is outside heap bounds (%p)", current_node);
	}
	else if (current_node->get_right_header() <= current_node) {
	    print_heap();
	    print_tree(avail_list);
	    Debug::panic("Cycle found (%p)", current_node);
        }
        else if (current_node->size_and_state == 0 || current_node->size_and_state == 4 || current_node->size_and_state == -4 || current_node->size_and_state == 8 || current_node->size_and_state == -8) {
	    print_heap();
	    print_tree(avail_list);
            Debug::panic("Node with size_and_state = %d (this is not allowed) was found at (%p)!", current_node -> size_and_state, current_node);
	}
	if (!current_node->is_allocated()) x++;
	current_node = current_node->get_right_header(); // next node in the heap
    }
    int y = size_of_tree(avail_list);
    if (y != x) {
        print_heap();
        print_tree(avail_list);
        Debug::panic("Num free nodes = %d != size of tree = %d.", x, y);
    }
    int a = spaceUnallocated_from_tree(avail_list);
    int b = spaceUnallocated_();
    if (a != b) {
        print_heap();
        print_tree(avail_list);
        Debug::panic("space unallocated in heap = %d != space unallocated in heap according to tree = %d.", b, a);
    }
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
    // o.w. cur->get_block_size() == target, so we found a true best fit (note we can arbitraily break ties of multiple perfect fits by picking the first one found)
    return cur; // first found; perfect match
}

Header* insert_help(Header* cur, Header* node) {
    if (cur == nullptr) {
        return node;
    }
    if (node->get_block_size() < cur->get_block_size()) {
        cur->left_child = insert_help(cur->left_child, node); // recursive call
    } else if (node->get_block_size() > cur->get_block_size()) {
        cur->right_child = insert_help(cur->right_child, node); // recursive call
    }
    // o.w. node->get_block_size() == cur->get_block_size(), so use the tiebreaker of addresses ; this is for O(logn) complexity maintainance 
    else {
        if (cur == node) { // already in the tree...
        }
	else if (node > cur) {
	    cur->right_child = insert_help(cur->right_child, node); // recursive call
	}
	else { // node < cur
	    cur->left_child = insert_help(cur->left_child, node); // recursive call 
	}
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
	else if (node != cur) {
		if (node < cur) {
		    cur->left_child = remove_help(cur->left_child, node);
		} else { // node > cur
		    cur->right_child = remove_help(cur->right_child, node);
		}
	}
        else { // found it 
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
    node->left_child = nullptr;
    node->right_child = nullptr;
    node->height = 0;
    avail_list = insert_help(avail_list, node);
}

void remove_from_tree(Header* node) {
    avail_list = remove_help(avail_list, node);
}

void do_small_unit_tests() {
    print_heap();
    print_tree(avail_list);
    malloc(0);
    print_heap();
    print_tree(avail_list);
    void* p = malloc(4);
    print_heap();
    print_tree(avail_list);
    void* p2 = malloc(5);
    print_heap();
    print_tree(avail_list);
    free(p); 
    print_heap();
    print_tree(avail_list);
    malloc(1); // should go pick:  *** (5fffec):  -12 *** 
    free(p2);
    print_heap();
    print_tree(avail_list);
    Debug::panic("End. In do_small_unit_tests()\n");
}


void do_slab_unit_tests() {
    print_heap();
    print_slab((Slab*) slabs[0]);
    malloc(4);
    void* p = malloc(4);
    malloc(4);
    print_heap();
    print_slab((Slab*) slabs[0]);
    Debug::printf("p: %x\n", p);
    free(p);
    print_heap();
    print_slab((Slab*) slabs[0]);
    malloc(4);
    p = malloc(4);
    Debug::printf("p: %x\n", p);
    print_heap();
    print_slab((Slab*) slabs[0]);
    free(p);
    for (int i = 0; i < 28; i++) {
        malloc(4);
    }
    Debug::printf("for loop done.\n");
    print_heap();
    print_slab((Slab*) slabs[0]);
    p = malloc(4);
    print_heap();
    print_slab((Slab*) slabs[0]);
    free(p);
    print_heap();
    print_slab((Slab*) slabs[0]);

    /*
    for (int i = 0; i < 31; i++) {
        void* p2 = malloc(4);
        print_heap();
        print_slab((Slab*) slabs[0]);
        Debug::printf("in loop, i: %d, p2: %x\n", i, p2);
    }
    Debug::printf("\n\n");
    print_slab((Slab*) slabs[0]);
    print_slab((Slab*)(((Slab*) slabs[0]) -> next));
    */
    Debug::panic("End. In do_slab_unit_tests()\n");
}

Slab* make_slab (uint32_t block_size, uint32_t init_bitmap, Header* next_node) {
    int32_t total_slab_size = 32 * block_size + 8;
    void* temp = nolock_malloc(total_slab_size); // OH FUCK
    if (temp == nullptr) { // because the + 4 bytes
        return nullptr; // failed to make a slab node because heap didn't have the room for it. 
    }
    Slab* new_slab = (Slab*) ptr_add(temp, -4); // issue: malloc() returns pointer 4 away from header
    new_slab->bitmap = init_bitmap; // init new slab
    new_slab->next = next_node;
    return new_slab;
}

// do_slab() will allocate b bytes inside a slab node
// this is done either in a new slab node or an existing one 
// layout of a slab for n bytes:
// Header Bit-Map Header* 32-blocks Footer
// 4B     4B      4B      32*nB     4B
//
// so total-slab-size = 32*bytes + 16B
void* do_slab (size_t bytes) {
    ASSERT(bytes % 4 == 0 && bytes >= 4); // assert that bytes is in bounds
//    int32_t total_slab_size = 32 * bytes + 8; // 8 because 4 for bitmap and 4 for next
    
    for (int i = 0; i < 4; i++) { // max 4 slab lists for now
        Header* cur = slabs[i];
	Slab* current = (Slab*) cur;
        if (cur == nullptr || current->get_block_size() != bytes) continue;
	// so we found the slab list that works
        while (current != nullptr) {
	    if (!current->is_full()) {
	        int32_t j = current->get_free_block();
		current->set_bit(j);
		return (void*) current->get_a_block(j);
	    }
	    current = (Slab*) current->next;
	}
	// so all of the slabs were full. We need to make a new slab and we can put it at the front of the slab list
        Slab* new_slab = make_slab(bytes, 0x1, cur); // cur is slabs[i]
	if (new_slab == nullptr) {
		return nullptr; // failed to make a slab node because heap didn't have the room for it. 
	}
	new_slab->next = cur; // cur is slabs[i]
	slabs[i] = (Header*) new_slab; // update array with the new slab node at the front of it
        return (void*) new_slab->get_a_block(0); 	
    }
    // it all failed...
    return nullptr; 
}

// returns Header* for start of slab node if p is in a slab
// otherwise will just return nullptr
Header* is_in_slab (void* p) {
    
    for (int i = 0; i < 4; i++) {
	Header* cur = slabs[i];
        while (cur != nullptr) {
	    Slab* slb = (Slab*) cur;
	    if (p >= slb->get_blocks_start() && p < slb->get_blocks_end()) { // get_blocks_end() is exclusive
	        // found the slab that it belongs to
		return cur;
	    }
	    // o.w. keep going down the slab list
	    cur = slb->next;
	}	
    }
    return nullptr;
}

// preconditions: slab != nullptr, p is one of the 32 blocks inside slab
void undo_slab (Header* slab, void* p) {
    ASSERT(slab != nullptr);
    Slab* slb = (Slab*) slab;
    int32_t index = slb->find_index_of_block(p);
    ASSERT(index >= 0 && index < 32); 
    slb->free_bit(index); // adjust bitmap so that the block is effectively freed
    if (slb->bitmap == 0) { // if this is true, then we need to remove slb from its slablist

        for (int i = 0; i < 4; i++) {
            Header* cur = slabs[i];
            Slab* s = (Slab*) cur;
	    if (s == nullptr || s->get_block_size() != slb->get_block_size()) { // if it's an empty list or the wrong list then continue
	        continue; // not the right slab list so go to the next slab list
	    }
	    if (cur == slab) { // first node case
		// still need to do a free:
		slabs[i] = (((Slab*)slabs[i])->next);
		nolock_free(ptr_add(slab, 4)); // same thing as before with free issue maybe
	        return;
	    }
            while (((Slab*)cur)->next != slab) {
                cur = ((Slab*)cur)->next;
	    }
	    // so cur's next is slab
	    s = (Slab*) cur;
	    ASSERT(s->next == slab);
	    s->next = ((Slab*)(s->next))->next;
	    nolock_free(ptr_add(slab, 4)); // since the node is removed from the free list, we can just remove it from the heap by using free() <-- expects 4+ ptr
	    return; 
        }
    }
}

void start_slabbing(uint32_t amount) {
    if (num_slab_lists >= max_num_slab_lists) {
        return;
    }
    slabs[num_slab_lists++] = (Header*) make_slab(amount, 0x1, nullptr);
}

// TODO: impl stop_slabbing ??

void heapInit(void* base, size_t bytes) {
    heap_start = round_up_mult_four(base);
    heap_size = round_down_mult_four(bytes);
    //Debug::printf("heap_start: %d, heap_size: %d \n", heap_start, heap_size);
    heap_end = ptr_add((void*) heap_start, heap_size);
        Header* middle_node = (Header*)heap_start;
        middle_node->size_and_state = get_negative(heap_size - NODE_OVERHEAD);
        middle_node->get_footer()->size_and_state = get_negative(heap_size - NODE_OVERHEAD);
        add_to_tree(middle_node);

        start_slabbing(4);
        start_slabbing(8);
        start_slabbing(12);	
	start_slabbing(16);
    //print_heap();
    //sanity_checker();
    //do_small_unit_tests();
    //do_slab_unit_tests();
    heap_lock = new BlockingLock();
}

void* nolock_malloc(size_t bytes) { //using best fit policy
    bytes = round_up_mult_four(bytes); // extra bytes if mallocing an amount that is not a multiple of four
//    if (bytes >= 4 && bytes < MIN_BLOCK_SIZE) {
//        bytes = MIN_BLOCK_SIZE; // for the minimum block size, so round up 4 & 8 to 12.
//    }
    if (bytes == 0) {// malloc(0) special case
        return ptr_add(heap_end, 4); // returns out of bounds pointer.
    }
    if (bytes < MIN_BLOCK_SIZE) { // making slab nodes for malloc(4) and malloc(8) cases... for now
        return do_slab(bytes);
    }
    if (bytes > (heap_size - NODE_OVERHEAD)) { // because heap_size is actually an overestimation already (since header + footer)
        return nullptr;
    }
    //Header* current_node = avail_list;
    Header* best_fit_node = get_best_fit((int32_t) bytes);
    //Debug::printf("bytes: %d, got best fit: %p and it has footer: %p\n", bytes, best_fit_node, best_fit_node->get_footer());
    if (best_fit_node == nullptr) return nullptr;

    if (abs(best_fit_node->size_and_state) >= (ssize_t)(MIN_BLOCK_SIZE + bytes + 8)) { // leftover header & footer from old node + min_block_size needed for new free node + len(new allocated node)=8+bytes,
        int32_t leftover_bytes = abs(best_fit_node -> size_and_state) + 8 - bytes - 8 - 8;
        ASSERT(leftover_bytes >= 8); // leftover_bytes >= 8, still, by algebra
        //Debug::printf("leftover_bytes: %d\n", leftover_bytes);
        Header* new_header = (Header*)ptr_add(best_fit_node->get_footer(), get_negative(bytes + 4)); // this indicates we malloc from RHS
        new_header->size_and_state = bytes; // bytes > 0
        new_header->get_footer()->size_and_state = bytes;

        remove_from_tree(best_fit_node);
        best_fit_node->size_and_state = get_negative(leftover_bytes);
        best_fit_node->get_footer()->size_and_state = get_negative(leftover_bytes);
        add_to_tree(best_fit_node);
        //sanity_checker();
        //print_heap();
        return new_header->get_block(); // since new_header is pointing to the allocated portion
    }
    else { // case where exact fit or 4/8/12/16 extra bytes more than exact fit
        best_fit_node -> size_and_state *= -1; //indicate that it's now full
        best_fit_node -> get_footer() -> size_and_state *= -1;
        remove_from_tree(best_fit_node); // since it's no longer available
        //sanity_checker();
        //print_heap();
        return best_fit_node->get_block();
    }
}



void* malloc(size_t bytes) { //using best fit policy
    //Debug::printf("In malloc, bytes = %d \n", bytes);
    LockGuardP g{heap_lock};
    //sanity_checker();
    //print_heap();
    bytes = round_up_mult_four(bytes); // extra bytes if mallocing an amount that is not a multiple of four
    if (bytes == 0) {// malloc(0) special case
        return ptr_add(heap_end, 4); // returns out of bounds pointer.
    }
    if (bytes < MIN_BLOCK_SIZE || bytes == 12 || bytes == 16) { // making slab nodes for malloc(4), malloc(8), malloc(12), malloc(16)  cases... for now
        return do_slab(bytes);
    }
    if (bytes > (heap_size - NODE_OVERHEAD)) { // because heap_size is actually an overestimation already (since header + footer)
        return nullptr;
    }
    Header* best_fit_node = get_best_fit((int32_t) bytes);
    if (best_fit_node == nullptr) return nullptr;
    
    if (abs(best_fit_node->size_and_state) >= (ssize_t)(MIN_BLOCK_SIZE + bytes + 8)) { // leftover header & footer from old node + min_block_size needed for new free node + len(new allocated node)=8+bytes,
        int32_t leftover_bytes = abs(best_fit_node -> size_and_state) + 8 - bytes - 8 - 8;
	ASSERT(leftover_bytes >= 8); // leftover_bytes >= 8, still, by algebra
        Header* new_header = (Header*)ptr_add(best_fit_node->get_footer(), get_negative(bytes + 4)); // this indicates we malloc from RHS
        new_header->size_and_state = bytes; // bytes > 0
        new_header->get_footer()->size_and_state = bytes;
        
	remove_from_tree(best_fit_node);
        best_fit_node->size_and_state = get_negative(leftover_bytes); 
        best_fit_node->get_footer()->size_and_state = get_negative(leftover_bytes);
	add_to_tree(best_fit_node);
	//sanity_checker();
        //print_heap();
        return new_header->get_block(); // since new_header is pointing to the allocated portion
    }
    else { // case where exact fit or 4/8/12/16 extra bytes more than exact fit
	best_fit_node -> size_and_state *= -1; //indicate that it's now full
	best_fit_node -> get_footer() -> size_and_state *= -1;
	remove_from_tree(best_fit_node); // since it's no longer available
	//sanity_checker();
        //print_heap();
        return best_fit_node->get_block();
    }
}

void nolock_free(void* p) {
    Header* node = (Header*)ptr_add(p, -4); // because user gives the pointer that is the start of the block

    if (p < heap_start || p >= heap_end || ((int32_t)(p)) % 4 != 0) { // cases where free will do nothing
        return;
    }
    Header* p_slab = is_in_slab(p);

    if (p_slab != nullptr) { // if p is in some slab node
        undo_slab(p_slab, p);
        return;
    }

        if (!node->is_allocated()) {
        return; // double free...
    }

    Header* left_node = (node -> get_left_footer() < heap_start) ? nullptr : node->get_left_footer()->get_header();
    Header* right_node = (node -> get_right_header() >= heap_end) ? nullptr : node->get_right_header(); // heap_end is a multiple of 4, we can't have a right node start from there
    bool left_node_is_free = (left_node == nullptr) ? (false) : (!left_node->is_allocated()); // nullptr like guardnodes => allocated = not free
    bool right_node_is_free = (right_node == nullptr) ? (false) : (!right_node->is_allocated());

    if (left_node_is_free || right_node_is_free) {
        if (left_node_is_free && right_node_is_free) { // left and right nodes are free
            int32_t new_block_size = get_negative(abs(node->size_and_state) + abs(left_node->size_and_state) + 8 + abs(right_node->size_and_state) + 8);
            remove_from_tree(left_node);
            remove_from_tree(right_node);
            left_node->size_and_state = new_block_size;
            right_node->get_footer()->size_and_state = new_block_size;
            add_to_tree(left_node);
            //sanity_checker();
            return;
        }
        // o.w. we will merge two blocks instead of 3
        Header* leftmost = (left_node_is_free) ? left_node : node;
        Header* rightmost = (right_node_is_free) ? right_node : node;
        ASSERT(!(leftmost == left_node && rightmost == right_node)); // assert that we didn't miss the above (3 nodes) case
        ASSERT((leftmost == node || rightmost == node) && !(leftmost == node && rightmost == node)); //exactly one of them is node
        Header* not_node = (leftmost == node) ? (rightmost) : (leftmost); // the one that isn't node
        int32_t new_block_size = get_negative(abs(leftmost->size_and_state) + abs(rightmost->size_and_state) + 8); // negative since it's free
        remove_from_tree(not_node);
        leftmost->size_and_state = new_block_size;
        rightmost->get_footer()->size_and_state = new_block_size;
        add_to_tree(leftmost); // leftmost is the combined new free block
        //sanity_checker();
        return;
    }
    // o.w. right is allocated and left is allocated, so just free this block
    node->size_and_state *= -1; // negative number is free
    node->get_footer()->size_and_state *= -1;
    add_to_tree(node);
    //sanity_checker();
    //print_heap();
}

void free(void* p) {
    LockGuardP g{heap_lock}; 
//    Debug::printf("In free, p = %x \n", p);
    //sanity_checker();
    //print_heap();
    //print_tree(avail_list);
    Header* node = (Header*)ptr_add(p, -4); // because user gives the pointer that is the start of the block

    if (p < heap_start || p >= heap_end || ((int32_t)(p)) % 4 != 0) { // cases where free will do nothing 
        return;
    }
    Header* p_slab = is_in_slab(p); 

    if (p_slab != nullptr) { // if p is in some slab node
        undo_slab(p_slab, p);
	return;	
    }

    if (!node->is_allocated()) {
        return; // double free... 
    }

    Header* left_node = (node -> get_left_footer() < heap_start) ? nullptr : node->get_left_footer()->get_header();
    Header* right_node = (node -> get_right_header() >= heap_end) ? nullptr : node->get_right_header(); // heap_end is a multiple of 4, we can't have a right node start from there
    bool left_node_is_free = (left_node == nullptr) ? (false) : (!left_node->is_allocated()); // nullptr like guardnodes => allocated = not free
    bool right_node_is_free = (right_node == nullptr) ? (false) : (!right_node->is_allocated());
    
    if (left_node_is_free || right_node_is_free) {
        if (left_node_is_free && right_node_is_free) { // left and right nodes are free
	    int32_t new_block_size = get_negative(abs(node->size_and_state) + abs(left_node->size_and_state) + 8 + abs(right_node->size_and_state) + 8);
	    remove_from_tree(left_node);
	    remove_from_tree(right_node);
            left_node->size_and_state = new_block_size;
	    right_node->get_footer()->size_and_state = new_block_size;
	    add_to_tree(left_node);
	    //sanity_checker();
	    return;
	}
	// o.w. we will merge two blocks instead of 3
	Header* leftmost = (left_node_is_free) ? left_node : node;
	Header* rightmost = (right_node_is_free) ? right_node : node;
	ASSERT(!(leftmost == left_node && rightmost == right_node)); // assert that we didn't miss the above (3 nodes) case
	ASSERT((leftmost == node || rightmost == node) && !(leftmost == node && rightmost == node)); //exactly one of them is node
	Header* not_node = (leftmost == node) ? (rightmost) : (leftmost); // the one that isn't node
        int32_t new_block_size = get_negative(abs(leftmost->size_and_state) + abs(rightmost->size_and_state) + 8); // negative since it's free
        remove_from_tree(not_node);
        leftmost->size_and_state = new_block_size;
        rightmost->get_footer()->size_and_state = new_block_size;
        add_to_tree(leftmost); // leftmost is the combined new free block
	//sanity_checker();
        return;	
    }
    // o.w. right is allocated and left is allocated, so just free this block
    node->size_and_state *= -1; // negative number is free
    node->get_footer()->size_and_state *= -1;
    add_to_tree(node);
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


