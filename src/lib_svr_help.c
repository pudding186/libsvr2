#include "./lib_svr_common_def.h"
unsigned long long BKDRHash64(const char* str)
{
    unsigned long long hash = 0;
    while (*str)
    {
        hash = hash * 131 + (*str++);
    }

    return hash;
}