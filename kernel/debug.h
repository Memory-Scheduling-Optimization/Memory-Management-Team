#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdarg.h>
#include "io.h"
#include "stdint.h"
#include "atomic.h"

class Debug {
    const char* what;
    bool flag;
    static OutputStream<char> *sink;
public:
    static Atomic<uint32_t> checks;
    static bool shutdown_called;
    static bool debugAll;
    static void vprintf(const char* fmt, va_list ap);
    static void printf(const char* fmt, ...);
    static void vpanic(const char* fmt, va_list ap);
    static void panic(const char* fmt, ...);
    static void missing(const char* file, int line);
    inline static void check(bool invariant, const char* invariant_text, const char* file, int line) {
        checks.fetch_add(1);
        if (!invariant) {
            panic("*** Check [%s] failed at %s:%d\n",invariant_text,file,line);
        }
    }
    inline static void assert(bool invariant, const char* invariant_text, const char* file, int line) {
        if (!invariant) {
            panic("*** Assertion [%s] failed at %s:%d\n",invariant_text,file,line);
        }
    }
    static void vsay(const char* fmt, va_list ap);
    static void say(const char* fmt, ...);
    static void shutdown();

    static void init(OutputStream<char> *sink);

    Debug(const char* what) : what(what), flag(false) {
    }
    
    void vdebug(const char* fmt, va_list ap);
    void debug(const char* fmt, ...);

    void on() { flag = true; }
    void off() { flag = false; }
};

#define MISSING() Debug::missing(__FILE__,__LINE__)

//inline void ASSERT(bool) {}
#define ASSERT(invariant) Debug::assert(invariant,#invariant,__FILE__,__LINE__)

// used for testing, please don't change
#define CHECK(invariant) Debug::check(invariant,#invariant,__FILE__,__LINE__)

#define PANIC(msg) Debug::panic("panic at %s:%d [%s]",__FILE__,__LINE__,(msg))

#endif
