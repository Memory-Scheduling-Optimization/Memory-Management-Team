#ifndef _LIBK_H_
#define _LIBK_H_

#include <stdarg.h>
#include "io.h"

class K {
public:
    static void snprintf (OutputStream<char>& sink, long maxlen, const char *fmt, ...);
    static void vsnprintf (OutputStream<char>& sink, long maxlen, const char *fmt, va_list arg);
    static long strlen(const char* str);
    static int isdigit(int c);
    static bool streq(const char* left, const char* right);

    template <typename T>
    static T min(T v) {
        return v;
    }

    template <typename T, typename... More>
    static T min(T a, More... more) {
        auto rest = min(more...); 
        return (a < rest) ? a : rest;
    }


};

#endif
