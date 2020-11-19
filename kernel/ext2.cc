#include "ext2.h"
#include "libk.h"

// Node

/* Read a block from the file
 * TODO caching
 */
void Node::read_block(uint32_t number, char* buffer) {
    if (number == cache_num) {
    	memcpy(buffer, cache, block_size);
    	return;
    }
    if (!cache) cache = new char[block_size];
    cache_num = number;
    if (number < 12) {
	// direct block
	uint32_t offset = data[number] * block_size;
	ide->read_all(offset, block_size, cache);
	memcpy(buffer, cache, block_size);
	return;
    }
    number -= 12;
    const uint32_t pc1 = block_size / sizeof(uint32_t);
    if (number < pc1) {
	// singly indirect block
	uint32_t offset = (data[12] * block_size) + (number * sizeof(uint32_t));
	ide->read_all(offset, sizeof(uint32_t), (char*) &offset);
	offset *= block_size;
	ide->read_all(offset, block_size, cache);
	memcpy(buffer, cache, block_size);
	return;
    }
    number -= pc1;
    const uint32_t pc2 = pc1 * pc1;
    if (number < pc2) {
	// doubly indirect block
	uint32_t offset = (data[13] * block_size) + (number / pc1 * sizeof(uint32_t));
	ide->read_all(offset, sizeof(uint32_t), (char*) &offset);
	offset *= block_size;
	offset += number % pc1;
	ide->read_all(offset, sizeof(uint32_t), (char*) &offset);
	offset *= block_size;
	ide->read_all(offset, block_size, cache);
	memcpy(buffer, cache, block_size);
	return;
    }
    number -= pc2;
    {
	// trebly indirect block
	uint32_t offset = (data[14] * block_size) + (number / pc2 * sizeof(uint32_t));
	ide->read_all(offset, sizeof(uint32_t), (char*) &offset);
	offset *= block_size;
	offset += (number % pc2) / pc1;
	ide->read_all(offset, sizeof(uint32_t), (char*) &offset);
	offset *= block_size;
	offset += number % pc1;
	ide->read_all(offset, sizeof(uint32_t), (char*) &offset);
	offset *= block_size;
	ide->read_all(offset, block_size, cache);
	memcpy(buffer, cache, block_size);
	return;
    }
}

// structure of directory entry
struct DirectoryEntry {
    uint32_t inode;
    uint16_t rec_len;
    uint16_t name_len;
    uint32_t placeholder;
    const char* getName() {
	return (const char*) &placeholder;
    }
};

uint32_t Node::entry_count() {
    if (entries == 0) {
	auto buffer = new char[block_size];
	uint32_t total_togo = size;
	for (uint32_t i = 0; total_togo > 0; i++, total_togo -= block_size) {
	    read_block(i, buffer);
	    uint32_t togo = block_size;
	    DirectoryEntry* entry = (DirectoryEntry*) &buffer[0];
	    while (togo > 0) {
		if (entry->inode != 0) entries++;
		togo -= entry->rec_len;
		entry = (DirectoryEntry*) ((uintptr_t) entry + entry->rec_len);
	    }
	}
	delete[] buffer;
    }
    return entries;
}

// Ext2

// unused
struct BlockGroupDescriptor {
    uint32_t block_usage_bitmap;      // 4 bytes
    uint32_t inode_usage_bitmap;      // 8 bytes
    uint32_t inode_table;             // 12 bytes
    uint16_t unallocated_block_count; // 14 bytes
    uint16_t unallocated_inode_count; // 16 bytes
    uint16_t directory_count;         // 18 bytes
    uint16_t unused[7];               // 32 bytes
};

Shared<Node> Ext2::getInode(uint32_t inode) {
    
    // extract raw data into buffer
    uint32_t block_group = (inode-1) / inode_count_per_group;
    ASSERT(block_group < block_group_count);
    
    auto bgd = group_table[block_group];
    uint32_t table_offset = bgd.inode_table * block_size;
    uint32_t index = (inode-1) % inode_count_per_group;
    uint32_t inode_offset = table_offset + (index * get_inode_size());
    auto buffer = new char[get_inode_size()];
    auto cnt = ide->read_all(inode_offset, get_inode_size(), buffer);
    ASSERT(cnt >= 0);
    
    // read relevent inode fields
    auto node = Shared<Node>::make(block_size, inode);
    node->ide = ide;
    node->mode = *((uint16_t*) &buffer[0]);
    node->size = *((uint32_t*) &buffer[4]);
    node->link_count = *((uint16_t*) &buffer[26]);
    memcpy(&node->data[0], &buffer[40], sizeof(uint32_t[15]));
    delete[] buffer;
    return node;
}

Ext2::Ext2(Shared<Ide> ide) : ide{ide} {

    // Debug::printf("reading ext2 fs\n");
    
    // read superblock (sector 2)
    auto buffer = new char[ide->block_size];
    ide->read_block(2, buffer);

    // extract superblock information    
    inode_count = *((uint32_t*) &buffer[0]);
    block_count = *((uint32_t*) &buffer[4]);
    
    block_size = 1024 << *((uint32_t*) &buffer[24]);

    block_count_per_group = *((uint32_t*) &buffer[32]);
    inode_count_per_group = *((uint32_t*) &buffer[40]);

    // number of block groups
    block_group_count = ((inode_count-1) / inode_count_per_group) + 1;
    ASSERT(block_group_count == ((block_count-1) / block_count_per_group) + 1);
    
    // Debug::printf("inode count: %d\n", inode_count);
    // Debug::printf("block count: %d\n", block_count);
    // Debug::printf("block count per group: %d\n", block_count_per_group);
    // Debug::printf("inode count per group: %d\n", inode_count_per_group);

    delete[] buffer;
    
    // read block group descriptor table
    uint32_t bgdt_offset = block_size * (block_size == 1024 ? 2 : 1);
    uint32_t bgdt_size = block_group_count * sizeof(BlockGroupDescriptor);

    buffer = new char[bgdt_size];
    ide->read_all(bgdt_offset, bgdt_size, buffer);

    // Debug::printf("block group count = %d\n", block_group_count);
    
    // initialize group table in memory
    group_table = new BGD[block_group_count];
    for (uint32_t i = 0; i < block_group_count; i++) {
	auto bgd = &group_table[i];
	bgd->inode_table = *((uint32_t*) &buffer[(sizeof(BlockGroupDescriptor) * i) + 8]);
    }
    delete[] buffer;

    // Debug::printf("ide->block_size = %d\n", ide->block_size);
    // read the root node
    root = getInode(2);    
    // Debug::printf("root size = %d\n", root->size);
    // Debug::printf("initialized ext2\n");
}

// static bool streq(const char* lhs, const char* rhs, uint32_t len) {
//     uint32_t i = 0;
//     for (; i < len; i++)
// 	if (lhs[i] != rhs[i]) return false;
//     return rhs[i] == 0;
// }

Shared<Node> Ext2::find(Shared<Node> dir, const char* name) {
    ASSERT(dir->is_dir());
    auto buffer = new char[block_size];
    uint32_t total_togo = dir->size;
    for (uint32_t i = 0; total_togo > 0; i++, total_togo -= block_size) {
	dir->read_block(i, buffer);
	uint32_t togo = block_size;
	DirectoryEntry* entry = (DirectoryEntry*) &buffer[0];
	while (togo > 0) {
	    const char* lhs = entry->getName();
	    uint32_t i = 0;
	    for (; i < entry->name_len; i++)
		if (lhs[i] != name[i]) goto next;
	    {
		auto node = getInode(entry->inode);
		if (name[i] == 0) { delete[] buffer; return node; }
		if (name[i] == '/') { delete[] buffer; return find(node, &name[i+1]); }
	    }
	next:
	    togo -= entry->rec_len;
	    entry = (DirectoryEntry*) ((uintptr_t) entry + entry->rec_len);
	}
    }
    delete[] buffer;
    return Shared<Node>{};
}

static constexpr uint32_t MAX_DEPTH = 10;

Shared<Node> Ext2::open(Shared<Node> dir, const char* name) {
    ASSERT(dir->is_dir());
    
    Shared<Node> cd = dir;
    Shared<Node> file;
    char* path;
    char* base;
    uint32_t depth = MAX_DEPTH;
    auto buffer = new char[block_size];
    
    {
	int sz = K::strlen(name);
	base = new char[sz+1];
	memcpy(base, name, sz+1);
    }

follow_path:

    if (depth == 0)
	goto fail;

    path = base;
    if (path[0] == '/') {
	cd = root;
	path++;
    }

next_link:

    // path is invalid
    if (!cd->is_dir())
	goto fail;

    // iterate through entries
    for (uint32_t b = 0; b < (cd->size_in_bytes() / block_size); b++) {
	cd->read_block(b, buffer);
	uint32_t togo = block_size;
	DirectoryEntry* entry = (DirectoryEntry*) buffer;

	while (togo > 0) {
	    const char* lhs = entry->getName();
	    uint32_t i = 0;
	    for (; i < entry->name_len; i++)
		if (lhs[i] != path[i]) goto next_entry;
	    
	    if (path[i] == 0) {
		file = getInode(entry->inode);
		goto parse_file;
	    } else if (path[i] == '/') {
		cd = getInode(entry->inode);
		path = &path[i+1];
		goto next_link;
	    }
	    
	next_entry:
	    togo -= entry->rec_len;
	    entry = (DirectoryEntry*) ((uintptr_t) entry + entry->rec_len);
	}
    }

    goto fail;

parse_file:    
    
    delete[] base;
    if (file->is_symlink()) {
	uint32_t sz = file->size_in_bytes();
	base = new char[sz+1];
	base[sz] = 0;
	file->get_symbol(base);
	depth--;
	goto follow_path;
    } else if (file->is_dir()) {
	file = Shared<Node>{};
    }
    
    delete[] buffer;
    return file;
    
fail:
    
    delete[] buffer;
    delete[] base;
    return Shared<Node>{};
}
