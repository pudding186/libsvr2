#pragma once
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern void* shm_alloc(int key, unsigned int size);
extern void shm_free(void* mem);

#ifdef  __cplusplus
}
#endif