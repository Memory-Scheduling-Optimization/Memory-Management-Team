#ifndef _blocking_lock_h_
#define _blocking_lock_h_

#include "debug.h"
#include "semaphore.h"

class BlockingLock : public Sharable<BlockingLock> {
    Semaphore mut;
public:
    inline BlockingLock() : mut{1} {}
    inline void lock() { mut.down(); }
    inline void unlock() { mut.up(); }
    inline bool isMine() { return true; }
};



#endif

