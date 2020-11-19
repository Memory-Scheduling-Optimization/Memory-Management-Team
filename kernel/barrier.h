#ifndef _barrier_h_
#define _barrier_h_

#include "condition.h"
#include "semaphore.h"
#include "shared.h"
#include "threads.h"

class Barrier : public Sharable<Barrier> {
    InterruptSafeLock mutex;
    Condition cv;
    volatile uint32_t count;
public:
    Barrier(uint32_t count) :
	mutex{},
	cv{mutex},
	count{count} {}
    Barrier(const Barrier&) = delete;
    Barrier& operator=(const Barrier&) const = delete;
    void sync() {
	{
	    LockGuard g{mutex};
	    if (--count == 0) goto release;
	}
	{
	    LockGuard g{mutex};
	    while (count) cv.wait();
	    return;
	}       
release:
	{
	    LockGuard g{mutex};
	    cv.notifyAll();
	}
    }
};

class ReusableBarrier : public Sharable<ReusableBarrier> {
    Semaphore mut;
    Semaphore sem;
    volatile uint32_t count = 0;
    const uint32_t limit;
public:
    ReusableBarrier(uint32_t count) :
	mut{1},
	sem{0},
	limit{count} {}
    ReusableBarrier(const ReusableBarrier&) = delete;
    ReusableBarrier& operator=(const ReusableBarrier&) const = delete;
    void sync() {
	mut.down();
	if (++count == limit) {
	    sem.up();
	} else
	    mut.up();
	sem.down();
	if (--count == 0) {
	    mut.up();
	} else
	    sem.up();
    }
};
        
#endif

