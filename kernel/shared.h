#ifndef _shared_h_
#define _shared_h_

#include "atomic.h"

template <typename T>
class Shared {

    T* ptr;
    
    void reset() {
	if (ptr && ptr->ref_count.add_fetch(-1) == 0)
	    delete ptr;
	ptr = nullptr;
    }
    
public:

    explicit Shared(T* it) : ptr{it} {
	if (ptr) ptr->ref_count.add_fetch(1);
    }

    //
    // Shared<Thing> a{};
    //
    Shared() : ptr{nullptr} {}

    //
    // Shared<Thing> b { a };
    // Shared<Thing> c = b;
    // f(b);
    // return c;
    //
    Shared(const Shared& rhs) : ptr{rhs.ptr} {
	if (ptr) ptr->ref_count.add_fetch(1);
    }

    //
    // Shared<Thing> d = g();
    //
    Shared(Shared&& rhs) : ptr{rhs.ptr} {
	rhs.ptr = nullptr;
    }

    ~Shared() {
	reset();
    }

    // d->m();
    T* operator -> () const {
        return ptr;
    }

    // d = nullptr;
    // d = new Thing{};
    Shared<T>& operator=(T* rhs) {
	reset();
	if ((ptr = rhs)) ptr->ref_count.add_fetch(1);
        return *this;
    }

    // d = a;
    // d = Thing{};
    Shared<T>& operator=(const Shared<T>& rhs) {
	if (&rhs == this) return *this;
	reset();
	if ((ptr = rhs.ptr)) ptr->ref_count.add_fetch(1);
        return *this;
    }

    // d = g();
    Shared<T>& operator=(Shared<T>&& rhs) {
	reset();
	ptr = rhs.ptr;
	rhs.ptr = nullptr;	
        return *this;
    }

    bool operator==(const Shared<T>& rhs) const {
        return ptr == rhs.ptr;
    }

    bool operator!=(const Shared<T>& rhs) const {
        return !(*this == rhs);
    }

    bool operator==(T* rhs) {
	return ptr == rhs;
    }

    bool operator!=(T* rhs) {
        return !(*this == rhs);;
    }

    uint32_t use_count() {
	return ptr->ref_count.get();
    }
    
    // e = Shared<Thing>::make(1,2,3);
    template <typename... Args>
    static Shared<T> make(Args... args) {
        return Shared<T>{new T(args...)};
    }
};

template<class Derived>
class Sharable {
    friend class Shared<Derived>;
    Atomic<uint32_t> ref_count;
protected:
    Sharable() : ref_count{0} {}
};

#endif
