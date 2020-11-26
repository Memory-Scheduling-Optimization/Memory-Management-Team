/*
 * heap
 */

#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "atomic.h"
#include "blocking_lock.h"
#include "avl.h"

struct Header;
struct Footer;
class HeaderRecord;

static void* heap_start = nullptr;
static void* heap_end = nullptr;
static size_t heap_size = 0;
static BlockingLock *heap_lock = nullptr;
static AVL<HeaderRecord>* avl_tree = nullptr;
//static Header* avail_list = nullptr;
static int const MIN_NODE_SIZE = 12; //= 16;
static int const MIN_BLOCK_SIZE 4; //= 8;
static int const NODE_OVERHEAD = 8;

class HeaderRecord {
    private:
        uint32_t data;
        Header* location_ptr;
    
    public:
         bool operator <(const HeaderRecord& h) {
             return data < h.data;
         }

	 bool operator <=(const HeaderRecord& h) {
             return data <= h.data;
         }

	 bool operator >(const HeaderRecord& h) {
             return data > h.data;
         }

         bool operator >=(const HeaderRecord& h) {
             return data >= h.data;
         }

	 bool operator ==(const HeaderRecord& h) {
             return data == h.data && location_ptr == h.location_ptr; 
         }
};


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
    Header* next_avail;
    Header* prev_avail;

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
    int num_free_nodes = 0; // number of free nodes in the heap

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
        else if (current_node -> size_and_state == 0 || current_node -> size_and_state == 4 || current_node -> size_and_state == -4) {
	    print_heap();
            Debug::panic("Node with size_and_state = %d (this is not allowed) was found at (%p)!", current_node -> size_and_state, current_node);	    
	}
        
	if (! current_node -> is_allocated()) { // if the current node is free
	    num_free_nodes++;
	    Header* next_node = current_node -> next_avail;
	    Header* prev_node = current_node -> prev_avail;
            if (next_node < heap_start || prev_node < heap_start || next_node >= heap_end || prev_node >= heap_end) {
	        Debug::panic("Out of bounds next_node %p or prev_node %p found from node at (%p).", next_node, prev_node, current_node);    
	    }	    
	}
	current_node = current_node -> get_right_header(); // next node in the heap 
    }
    
    current_node = avail_list;
    if (current_node == nullptr) {
	if (num_free_nodes != 0) {
            Debug::panic("Heap has %d free nodes but the avail list has 0 free nodes in it.", num_free_nodes);
	}    
    }
    else {
	bool first_time = true;
        int num_nodes_in_avail_list = 0;

        while (first_time || current_node != avail_list) {
	    if (first_time) {
	        first_time = false;	    
	    }
            num_nodes_in_avail_list++;
            current_node = current_node -> next_avail;	    
	}
        if (num_nodes_in_avail_list != num_free_nodes) {
	    Debug::panic("Heap has %d free nodes but the avail list has %d free nodes in it.", num_free_nodes, num_nodes_in_avail_list);	
	}	
    }
}

void heapInit(void* start, size_t bytes) { // ex: called with start = 0x100000 and bytes = 1<<20
    heap_lock = theLock = new BlockingLock(); 
    heap_start = round_up_mult_four(start);
    heap_size = round_down_mult_four(bytes);
    //Debug::printf("heap_start: %d, heap_size: %d \n", heap_start, heap_size);
    heap_end = ptr_add((void*) heap_start, heap_size);

    if (heap_size >= MIN_NODE_SIZE) { // if there is room for anything in the heap
        Header* middle_node = (Header*)heap_start;
        middle_node->size_and_state = get_negative(heap_size - NODE_OVERHEAD); 
        middle_node->get_footer()->size_and_state = get_negative(heap_size - NODE_OVERHEAD);
        middle_node->next_avail = middle_node; // circular doubly linked list
	middle_node->prev_avail = middle_node; 
	avail_list = middle_node; // set the available list global variable as needed
    }
    //sanity_checker();
}

//TODO this and remove_from_avail_list
void add_to_avail_list(Header* node) { // add node to the avail linked list
    if (avail_list == nullptr) {
        avail_list = node;
        node -> next_avail = node;
        node -> prev_avail = node;	
    }
    else {
	Header* other_node = avail_list;
	Header* other_prev_node = other_node -> prev_avail;
        node -> next_avail = other_node;
        node -> prev_avail = other_prev_node;
        other_prev_node -> next_avail = node;
	other_node -> prev_avail = node;
    }
}

void remove_from_avail_list(Header* node) {
	if (avail_list == nullptr) // empty list case. Note, this should never happen
	    return; 
	if (node -> next_avail == node) { // one node case
	    avail_list = nullptr;
	    return;
	}

	if (node == avail_list) { // to make sure that avail_list is point to an actually free nide 
            avail_list = avail_list -> next_avail;
	}
	Header* next = node -> next_avail;
	Header* prev = node -> prev_avail;
        next -> prev_avail = prev;
	prev -> next_avail = next;
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
    if (avail_list == nullptr) { // case where we know that there are no free nodes
	return nullptr; 
    }
    if (bytes >= heap_size) { // because heap_size is actually an overestimation already (since header + footer)
        return nullptr;    
    }
    Header* current_node = avail_list;
    Header* best_fit_node = nullptr;
    int32_t best_size = __INT_MAX__; // since it's lowest size found
    bool found_fit = false;
    bool first_time = true;

    while (first_time || current_node != avail_list) {
	if (first_time)
	    first_time = false;

        if (abs(current_node->size_and_state) < best_size && abs(current_node -> size_and_state) >= (ssize_t)bytes) {
	    found_fit = true;
	    best_size = abs(current_node->size_and_state);
            best_fit_node = current_node;
            if (abs(current_node -> size_and_state) == (ssize_t)bytes) { // Perfect fit
	        break;    
	    }	    
	}
	current_node = current_node -> next_avail;
    }

    if (found_fit && abs(best_fit_node -> size_and_state) >= (ssize_t)(MIN_BLOCK_SIZE + bytes + 8)) { // leftover header & footer from old node + min_block_size needed for new free node + len(new allocated node)=8+bytes, 
        int32_t leftover_bytes = abs(best_fit_node -> size_and_state) + 8 - bytes - 8 - 8; // leftover_bytes >= 8
        Header* new_header = (Header*)ptr_add(best_fit_node->get_footer(), get_negative(bytes + 4)); // this indicates we malloc from RHS => no need to remove from available list
        new_header->size_and_state = bytes; // bytes > 0
        new_header->get_footer()->size_and_state = bytes; 
            
        best_fit_node->size_and_state = get_negative(leftover_bytes); // <-- v <= -4, so this node is still in the avail list so this works. 
        best_fit_node->get_footer()->size_and_state = get_negative(leftover_bytes);	   
        //print_heap();
        return new_header->get_block(); // since new_header is pointing to the allocated portion 
    }
    else if (found_fit) { // case where exact fit or 4/8/12 extra bytes more than exact fit
	best_fit_node -> size_and_state *= -1; //indicate that it's now full
	best_fit_node -> get_footer() -> size_and_state *= -1;
	remove_from_avail_list(best_fit_node); // since it's no longer available
        //print_heap();	
        return best_fit_node->get_block();	    
    }
    //sanity_checker();
    //print_heap();
    return nullptr; // otherwise, no free nodes found i.e. no free space in the heap so malloc failed
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
    bool is_first_case = false;
    bool is_second_case = false;

    if (right_node != nullptr && !right_node -> is_allocated()) { // if right node is free 
        is_first_case = true;
	int32_t new_block_size = get_negative(abs(node -> size_and_state) + abs(right_node -> size_and_state) + 8);
	node -> size_and_state = new_block_size; // +8 because of the now removed header and footer
	add_to_avail_list(node);
	remove_from_avail_list(right_node);
	right_node -> get_footer() -> size_and_state = new_block_size;
    }

    if (left_node != nullptr && !left_node -> is_allocated()) { // if left node is free
	is_second_case = true;   
        if (is_first_case) { // if this is true => node has been added to the avail_list and hence must be removed
	    remove_from_avail_list(node);
	}
        // since left_node is free, => it's already in the avail_list.
	int32_t new_block_size = get_negative(abs(node -> size_and_state) + abs(left_node -> size_and_state) + 8); 	
        left_node -> size_and_state = new_block_size; // +8 because of the now removed header and footer
        node -> get_footer() -> size_and_state = new_block_size;
    } 

    if (!is_first_case && !is_second_case) { // if left node was not free and right node was not free
        add_to_avail_list(node);
        node -> size_and_state *= -1;
        node -> get_footer() -> size_and_state *= -1;	
    }
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


