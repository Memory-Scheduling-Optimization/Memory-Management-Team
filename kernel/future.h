#ifndef _future_h_
#define _future_h_

#include "atomic.h"
#include "semaphore.h"
#include "threads.h"
#include "shared.h"

template <typename T>
class Future : public Sharable<Future<T>> {
    Semaphore sem;
    T value;
public:
    Future() : sem{0} {}
    Future(const Future&) = delete;    
    void set(T v) {
	value = v;
	sem.up();
    }
    T get() {
	sem.down();
	sem.up();
	return value;
    }
};

template <typename Out, typename T>
Shared<Future<Out>> future(T work) {
    auto f = Shared<Future<Out>>::make();
    thread([=] { f->set(work()); });
    return f;
}

#endif

