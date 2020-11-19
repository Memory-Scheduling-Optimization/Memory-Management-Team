#ifndef _SMP_H
#define _SMP_H

#include "config.h"
#include "stdint.h"
#include "atomic.h"

class SMP {
private:
    static constexpr uint32_t ENABLE = 1 << 11;
    static constexpr uint32_t ISBSP = 1 << 8;
    static constexpr uint32_t MSR = 0x1B;
    static AtomicPtr<uint32_t> id;
    static AtomicPtr<uint32_t> spurious;
    static AtomicPtr<uint32_t> icr_low;
    static AtomicPtr<uint32_t> icr_high;

public:
    static AtomicPtr<uint32_t> eoi_reg;
    static AtomicPtr<uint32_t> apit_lvt_timer;
    static AtomicPtr<uint32_t> apit_initial_count;
    static AtomicPtr<uint32_t> apit_current_count;
    static AtomicPtr<uint32_t> apit_divide;
    static const char* names[MAX_PROCS];
public:
    static void init(bool isFirst);
    static uint32_t me() { return (id.get() >> 24); }
    static const char* name() { return names[me()]; }
    static void eoi() { eoi_reg = 0; }

    static void ipi(uint32_t id, uint32_t num) {
        icr_high = id << 24;
        icr_low = num;
        while (icr_low.get() & (1 << 12));
    }

    static Atomic<uint32_t> running;
};


template<class T>
class PerCPU {
private:
    T data[MAX_PROCS];
public:
    inline T& forCPU(int id) {
        return data[id];
    }

    inline T& mine() {
        return forCPU(SMP::me());
    }
};

#endif // _SMP_H
