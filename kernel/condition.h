#ifndef _kernel_condition_h_
#define _kernel_condition_h_

#include "queue.h"
#include "atomic.h"
#include "threads.h"
#include "shared.h"

// A Mesa-style condition variable
//    - protected by a mutual exclusion lock. The lock protocol is:
//          * lock()
//          * unlock()
//          * isMine(): returns true if the lock is held by the caller but:
//                            allows false positives. false => not held by the caller.
//                                                    true => may or may not be held by the caller
//                            makes no atomicity guarantees
//
//    - wait() can have false postivies: can return even though the conditioned hasn't been signalled
//             and doesn't make owner guarantees
//

class Condition : public Sharable<Condition> {

    InterruptSafeLock& the_lock;
    Queue<gheith::TCB,NoLock> queue;
    uint64_t epoch;   // protects against missed updates

public:

    Condition(InterruptSafeLock& the_lock) : the_lock(the_lock), queue(), epoch(0) {}

    Condition(const Condition&) = delete;

    // Pre-condition: holding the lock
    // Post-condition: holding the lock
    // Guarantee: no missed signals
    // Non guarantee: condition is met. The caller should re-check.
    //
    // Why do we allow false positives (return without being signalled and/or allow
    // the state to change after you've been signalled)?
    //
    //     -   We implement Mesa-style monitors in which the lock is not
    //         transfered from the signaller to the waiting thread. This
    //         means that when a thread wakes up in wait, it has to reacquire
    //         the lock. This gives a chance to other threads to enter the
    //         monitor and mess with the state.
    //
    //         We can implement lock transfer and tighten that condition but:
    //
    //             - we decided below not to hold a lock across the context switch
    //
    //             - lock transfer can lead to subtle programming errors when
    //               the caller of signal forgets that the lock is lost
    //
    //         This also makes us immune against software that generates false notifies.
    //

    void wait() {
        using namespace gheith;

        // check the pre-condition
        ASSERT(the_lock.isMine());
            
        using namespace gheith;

        auto old_epoch = epoch;

        the_lock.unlock();    // we can't hold an InterruptSafeLocks across context swithces
                     // we will release the lock then acquire again and use
                     // the epoch value to tell if we missed an event

        // We can handle false positives, let's not insist on blocking
        block(BlockOption::CanReturn,[this,old_epoch](TCB* me) {
            // idle threads have no business waiting for things
            ASSERT(!me->isIdle);

            // Things could've changed since we released the lock
            // Get the lock again and check the epoch
            the_lock.lock();

            auto new_epoch = epoch;

            ASSERT(new_epoch >= old_epoch);

            if (old_epoch == new_epoch) {
                // no new notifications, let's go on the queue
                queue.add(me);
                the_lock.unlock();
            } else {
                // we missed a notify, let's pretend we saw it
                the_lock.unlock();
                schedule(me);
            }
        });

        // We're back either because:
        //    - we switched context and missed an event
        //    - we switched context and blocked then someone notified us
        // In both cases we grab the lock again and retry
        the_lock.lock();

        // The post-condition is trivially satisfied but might as well be explicit about it
        ASSERT(the_lock.isMine());
    }

    // Pre-condition: has the lock
    // Post-condition: has the lock
    // Guarantee: will never release the lock
    void notify(const uint64_t limit) {
        using namespace gheith;

        ASSERT(the_lock.isMine());

        epoch += 1;

        for (uint64_t i=0; i<limit; i++) {
            auto next = queue.remove();
            if (next == nullptr) break;
            schedule(next);
        }

        ASSERT(the_lock.isMine());
    }

    // Pre-condition: has the lock
    // Post-condition: has the lock
    // Guarantee: will never release the lock   
    inline void notifyOne() {
        notify(1);
    }

    // Pre-condition: has the lock
    // Post-condition: has the lock
    // Guarantee: will never release the lock   
    inline void notifyAll() {
        notify(~((uint64_t)0));
    }

};


#endif
