#pragma once
#include <cstdlib>
#include <cstdio>

#if defined(LX_DEBUG)
    #define LX_ASSERT(condition, message)                          \
        do {                                                        \
            if (!(condition)) {                                     \
                ::fprintf(stderr,                                   \
                    "Assertion failed: %s\n"                        \
                    "Message:   %s\n"                               \
                    "File:      %s\n"                               \
                    "Line:      %d\n",                              \
                    #condition, message, __FILE__, __LINE__);       \
                ::abort();                                          \
            }                                                       \
        } while(0)
#else
    #define LX_ASSERT(condition, message) ((void)0)
#endif