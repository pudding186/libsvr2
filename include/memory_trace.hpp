#pragma once
#include "./lib_svr_def.h"
#ifdef  __cplusplus

extern void trace_alloc(const char* name, const char* file, int line, void* ptr, size_t size);
extern void trace_free(void* ptr);

extern HRBNODE mem_trace_info_head(void);
#endif