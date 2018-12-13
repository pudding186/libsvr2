#pragma once
#include <tchar.h>
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern void* shm_alloc(const TCHAR* shm_name, unsigned int size);
extern void shm_free(void* mem);

#ifdef  __cplusplus
}
#endif