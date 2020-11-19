#ifndef _SYS_H_
#define _SYS_H_

#include "pcb.h"
#include "ext2.h"

namespace SYS {
    void init(void);
    int exec(Shared<PCB>&, Shared<Node>&, const char**);
    int exec(Shared<Node>&, const char*, ...);
    int exit(int status);
};

#endif
