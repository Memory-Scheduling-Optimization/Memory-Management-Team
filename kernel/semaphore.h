#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include "stdint.h"
#include "atomic.h"
#include "queue.h"
#include "threads.h"

class Semaphore {
    uint64_t volatile count;
    InterruptSafeLock lock;
    Queue<gheith::TCB,NoLock> waiting;
public:
    Semaphore(const uint32_t count) : count(count), lock(), waiting() {}

    Semaphore(const Semaphore&) = delete;

    void down() {
        using namespace gheith;
        
        lock.lock();
            
        if (count > 0) {
            count--;
            lock.unlock();
            return;
        }
        
        // We release the lock then check again when we switch to the
        // target stack. This way we don't have to hold the lock across
        // the contextSwitch.
        //
        // There is no evidence that either solution is better. It all
        // depends on the usage pattern
        lock.unlock();

        // We have to block because we don't check again
        block(BlockOption::MustBlock,[this](TCB* me) {

            ASSERT(!me->isIdle);

            // Let's acquire the lock again and see if the world has changed since we looked
            lock.lock();

            if (count > 0) {
                // Things changed and now we can decrement the counter and continue
                // But it's too late to change our minds. Just go back on the ready queue
                count--;
                lock.unlock();
                schedule(me);
            } else {
                // semaphore count is still 0, we have to wait
                waiting.add(me);
                lock.unlock(); // we have to continue to hold the lock until we're
                               // comfortably in the queue
            }
        });
    }

    void up() {
        using namespace gheith;

        lock.lock();
        auto next = waiting.remove();
        if (next == nullptr) {
            count ++;
        }
        lock.unlock();
        
        if (next != nullptr) {
            schedule(next);    
        }
    }
    
};

#endif

