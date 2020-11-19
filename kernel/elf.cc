#include "elf.h"
#include "shared.h"
#include "ext2.h"
#include <stdarg.h>
#include "libk.h"
#include "config.h"
#include "vmm.h"
#include "threads.h"
#include "process.h"

namespace ELF {    

    constexpr uint32_t MAGIC = 0x464c457f;
    constexpr uint32_t PT_LOAD = 1;
    
    struct ProgramHeader {
	uint32_t type;
	uint32_t offset;
	uint32_t vaddr;
	uint32_t paddr;
	uint32_t filesize;
	uint32_t memsize;
	uint32_t flags;
	uint32_t align;	
    };

    bool read_header(Shared<Node> file, Header& eh) {
	
	if (file->size_in_bytes() < sizeof(Header))
	    return false;
	
	file->read(0, eh);

	if (*((uint32_t*) &eh.ident[0]) != MAGIC) {
	    Debug::printf("Not an ELF file!\n");
	    return false;
	}

	if (eh.ident[4] != 1) {
	    Debug::printf("Not 32-bit!\n");
	    return false;	    
	}

	if (eh.ident[5] != 1) {
	    Debug::printf("Not little endian!\n");
	    return false;
	}

	if (eh.ident[7] != 0) {
	    Debug::printf("ABI is not System V\n");
	    return false;
	}

	if (eh.type != 2) {
	    Debug::printf("Not executable!\n");
	    return false;
	}

	if (eh.machine != 3) {
	    Debug::printf("Not x86!\n");
	    return false;
	}

	if (eh.entry < 0x80000000) {
	    return false;
	}
	
	return true;
    }

    uint32_t load(Shared<Node> file, Header& eh) {
	for (uint32_t i = 0; i < eh.phentnum; i++) {
	    ProgramHeader phent;
	    file->read(eh.phoff+(i*eh.phentsize), phent);
	    if (phent.type != PT_LOAD) continue;
	    uint32_t vaddr = phent.vaddr;
	    uint32_t filesize = phent.filesize;
	    file->read_all(phent.offset, filesize, (char*) vaddr);
	    bzero((void*) (vaddr+filesize), phent.memsize-filesize);
	}
	return eh.entry;
    }
}
