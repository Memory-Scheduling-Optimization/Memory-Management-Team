#ifndef _new_h_
#define _new_h_
#include "stdint.h"
inline void* operator new(size_t, void* p) { return p; }
#endif
