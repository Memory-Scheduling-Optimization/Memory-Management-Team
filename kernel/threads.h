#ifndef _threads_h_
#define _threads_h_

#include "atomic.h"
#include "queue.h"
#include "heap.h"
#include "debug.h"
#include "smp.h"
#include "shared.h"
#include "new.h"

#include "tss.h"
#include "pcb.h"

namespace gheith {

    constexpr static int STACK_BYTES = 8 * 1024;
    constexpr static int STACK_WORDS = STACK_BYTES / sizeof(uint32_t);

    struct TCB;

    struct SaveArea {
        uint32_t ebx;
        uint32_t esp;
        uint32_t ebp;
        uint32_t esi;
        uint32_t edi;
	uint32_t cr2;
        volatile uint32_t no_preempt;
        TCB* tcb;
    };
    
    struct TCB {
        static Atomic<uint32_t> next_id;
	
        const bool isIdle;
        const uint32_t id;

        TCB* next;
        SaveArea saveArea;
	Shared<PCB> pcb;
	
        TCB(bool isIdle) :
	    isIdle(isIdle),
	    id(next_id.fetch_add(1)) {
            saveArea.tcb = this;
        }

	TCB(Shared<PCB> pcb, bool isIdle) :
	    isIdle(isIdle),
	    id(next_id.fetch_add(1)),
	    pcb(pcb) {
	    saveArea.tcb = this;
	}

        virtual ~TCB() {}

        virtual void doYourThing() = 0;
    };

    extern "C" void gheith_contextSwitch(gheith::SaveArea *, gheith::SaveArea *, void* action, void* arg);

    extern TCB** activeThreads;
    extern TCB** idleThreads;

    extern TCB* current();
    extern Queue<TCB,InterruptSafeLock> readyQ;
    extern void entry();
    extern void schedule(TCB*);
    extern void delete_zombies();

    template <typename F>
    void caller(SaveArea* sa, F* f) {
        (*f)(sa->tcb);
    }
    
    // There is a bit of template/lambda voodoo here that allows us to
    // call block like this:
    //
    //   block([](TCB* previousTCB) {
    //        // do any cleanup work here but remember that you are:
    //        //     - running on the target stack
    //        //     - interrupts are disabled so you better be quick
    //   }

    enum class BlockOption {
        MustBlock,
        CanReturn
    };

    template <typename F>
    void block(BlockOption blockOption, const F& f) {

        uint32_t core_id;
        TCB* me;

        Interrupts::protect([&core_id,&me] {
            core_id = SMP::me();
            me = activeThreads[core_id];
            me->saveArea.no_preempt = 1;
        });
        
    again:
        readyQ.monitor_add();
        auto next_tcb = readyQ.remove();
        if (next_tcb == nullptr) {
            if (blockOption == BlockOption::CanReturn) {
		me->saveArea.no_preempt = 0;
		asm volatile ("pause");
		return;
	    }
            if (me->isIdle) {
                // Many students had problems with hopping idle threads
                ASSERT(core_id == SMP::me());
                ASSERT(!Interrupts::isDisabled());
                ASSERT(me == idleThreads[core_id]);
                ASSERT(me == activeThreads[core_id]);
                iAmStuckInALoop(true);
                goto again;
            }
            next_tcb = idleThreads[core_id];    
        }

        next_tcb->saveArea.no_preempt = 1;

        activeThreads[core_id] = next_tcb;  // Why is this safe?

	{
	    uint32_t old_cr3 = getCR3();
	    uint32_t new_cr3 = next_tcb->pcb->cr3;
	    if (old_cr3 != new_cr3)
		setCR3(new_cr3);
	    tss[core_id].esp0 = next_tcb->pcb->esp0;
	    
	}
	
        gheith_contextSwitch(&me->saveArea,&next_tcb->saveArea,(void *)caller<F>,(void*)&f);
    }

    struct TCBWithStack : public TCB {
        uint32_t *stack = new uint32_t[STACK_WORDS];
    
        TCBWithStack() : TCB(false) {
            stack[STACK_WORDS - 2] = 0x200;  // EFLAGS: IF
            stack[STACK_WORDS - 1] = (uint32_t) entry;
	        saveArea.no_preempt = 0;
            saveArea.esp = (uint32_t) &stack[STACK_WORDS-2];
        }

	TCBWithStack(Shared<PCB> pcb) : TCB(pcb, false) {
	    stack[STACK_WORDS - 2] = 0x200;  // EFLAGS: IF
            stack[STACK_WORDS - 1] = (uint32_t) entry;
	        saveArea.no_preempt = 0;
            saveArea.esp = (uint32_t) &stack[STACK_WORDS-2];
	    pcb->esp0 = saveArea.esp;
	}
	
        ~TCBWithStack() {
            if (stack) {
                delete[] stack;
                stack = nullptr;
            }
        }
    };
    

    template <typename T>
    struct TCBImpl : public TCBWithStack {
        T work;

        TCBImpl(T work) : TCBWithStack(), work(work) {}
	
	TCBImpl(Shared<PCB> pcb, T work) : TCBWithStack(pcb), work(work) {}
	
        ~TCBImpl() {}

        void doYourThing() override {
            work();
        }
    };

    
};

extern void threadsInit();

extern void stop();
extern void yield();


template <typename T>
void thread(T work) {
    using namespace gheith;

    delete_zombies();
    
    auto tcb = new TCBImpl<T>(kProc, work);
    schedule(tcb);
}

template <typename T>
void thread(Shared<PCB> pcb, T work) {
    using namespace gheith;

    delete_zombies();

    auto tcb = new TCBImpl<T>(pcb, work);
    schedule(tcb);
}




#endif
