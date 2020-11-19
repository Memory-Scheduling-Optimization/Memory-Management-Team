#ifndef _TSS_H_
#define _TSS_H_

struct TSS {
    uint32_t unused1;
    uint32_t esp0;        // %ESP when CPL changes to 0
    uint32_t ss0;         // %ESP when CPI changes to 0
    uint32_t unused2[23];
};

extern TSS tss[16];

#endif
