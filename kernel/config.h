#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "stdint.h"

struct ApicInfo {
    uint8_t processorId;
    uint8_t apicId;
    uint32_t flags;
} __attribute__((packed));

typedef struct ApicInfo ApicInfo;

#define MAX_PROCS 16

struct Config {
    uint32_t memSize;
    uint32_t nOtherProcs;
    uint32_t totalProcs;
    uint32_t localAPIC;
    uint32_t madtFlags;
    uint32_t ioAPIC;

    ApicInfo apicInfo[MAX_PROCS];
    char oemid[7];
};

typedef struct Config Config;
extern Config kConfig;

extern void configInit(Config* config);


#endif
