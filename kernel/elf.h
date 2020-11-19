#ifndef _elf_h_
#define _elf_h_

#include "ext2.h"

namespace ELF {
    
    struct Header {
	char ident[16];
	uint16_t type;
	uint16_t machine;
	uint32_t verison;
	uint32_t entry;
	uint32_t phoff;
	uint32_t shoff;
	uint32_t flags;
	uint16_t ehsize;
	uint16_t phentsize;
	uint16_t phentnum;
	uint16_t shentsize;
	uint16_t shentnum;
	uint16_t shstrndx;
    };

    bool read_header(Shared<Node>, Header&);
    uint32_t load(Shared<Node>, Header&);
    
    int load(Shared<Node>, uint32_t&);
}

#endif
