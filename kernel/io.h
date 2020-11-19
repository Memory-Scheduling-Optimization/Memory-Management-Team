#ifndef _IO_H_
#define _IO_H_

#include "shared.h"

template<typename T>
class OutputStream : public Sharable<OutputStream<T>> {
public:
    virtual ~OutputStream() = default;
    virtual void put(T v) = 0;
};

template<typename T>
class InputStream : public Sharable<InputStream<T>> {    
public:
    virtual ~InputStream() = default;
    virtual T get() = 0;
};

#endif
