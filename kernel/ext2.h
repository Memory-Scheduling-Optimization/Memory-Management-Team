#ifndef _ext2_h_
#define _ext2_h_

#include "ide.h"
#include "shared.h"
#include "atomic.h"
#include "debug.h"
#include "heap.h"

// A wrapper around an i-node
class Node : public BlockIO, public Sharable<Node> { // we implement BlockIO because we
                                                     // represent data
    
    Shared<Ide> ide;     // device driver
    uint16_t mode;       // [ 4b type | 12b permissions ]
    uint16_t link_count; // number of hard links
    uint32_t size;       // size
    uint32_t data[15];   // store block numbers
    uint32_t entries = 0;
    char* cache = nullptr;
    uint32_t cache_num = ~0;
    
public:

    // i-number of this node
    const uint32_t number;

    Node(uint32_t block_size, uint32_t number) : BlockIO(block_size), number(number) {}

    virtual ~Node() {
	if (cache) delete cache;
    }

    // How many bytes does this i-node represent
    //    - for a file, the size of the file
    //    - for a directory, implementation dependent
    //    - for a symbolic link, the length of the name
    uint32_t size_in_bytes() override {
        return size;
    }

    // read the given block (panics if the block number is not valid)
    // remember that block size is defined by the file system not the device
    void read_block(uint32_t number, char* buffer) override;

    // returns the ext2 type of the node
    uint32_t get_type() {
        return mode >> 12;
    }

    // true if this node is a directory
    bool is_dir() {
        return get_type() == 0x4;
    }

    // true if this node is a file
    bool is_file() {
        return get_type() == 0x8;
    }

    // true if this node is a symbolic link
    bool is_symlink() {
        return get_type() == 0xa;
    }

    // If this node is a symbolic link, fill the buffer with
    // the name the link referes to.
    //
    // Panics if the node is not a symbolic link
    //
    // The buffer needs to be at least as big as the the value
    // returned by size_in_byte()
    void get_symbol(char* buffer) {
	ASSERT(is_symlink());
	if (size < 60) {
	    memcpy(buffer, &data[0], size);
	} else {
	    read_all(0, size, buffer);
	}
    }

    // Returns the number of hard links to this node
    uint32_t n_links() {
        return link_count;
    }

    // Returns the number of entries in a directory node
    //
    // Panics if not a directory
    uint32_t entry_count();

    friend class Ext2;
};


// This class encapsulates the implementation of the Ext2 file system
class Ext2 : public Sharable<Ext2> {
    // The device on which the file system resides
    Shared<Ide> ide;   

    uint32_t inode_count;
    uint32_t block_count;
    uint32_t block_size;
    uint32_t block_count_per_group;
    uint32_t inode_count_per_group;
    uint32_t block_group_count;
    
    struct BGD {
//	uint32_t block_usage_bitmap;
//	uint32_t inode_usage_bitmap;
	uint32_t inode_table;
//	uint16_t unallocated_block_count;
//	uint16_t unallocated_inode_count;
//	uint16_t directory_count;
//	uint8_t unused[14];
    }* group_table;

    Shared<Node> getInode(uint32_t index);

public:
    // The root directory for this file system
    Shared<Node> root;
public:
    // Mount an existing file system residing on the given device
    // Panics if the file system is invalid
    Ext2(Shared<Ide> ide);
    ~Ext2() {
	delete[] group_table;
    }

    // Returns the block size of the file system. Doesn't have
    // to match that of the underlying device
    uint32_t get_block_size() {
        return block_size;
    }

    // Returns the actual size of an i-node. Ext2 specifies that
    // an i-node will have a minimum size of 128B but could have
    // more bytes for extended attributes
    uint32_t get_inode_size() {
        return 128;
    }

    // If the given node is a directory, return a reference to the
    // node linked to that name in the directory.
    //
    // Returns a null reference if "name" doesn't exist in the directory
    //
    // Panics if "dir" is not a directory
    Shared<Node> find(Shared<Node> dir, const char* name);

    Shared<Node> open(Shared<Node> dir, const char* name);
    
    friend class Node;
};

#endif
