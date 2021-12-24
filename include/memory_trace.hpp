#pragma once
#include "./lib_svr_def.h"
#include "./utility.hpp"
#ifdef  __cplusplus

typedef struct st_mem_stack_info
{
    int                     m_stack_size;
    CFuncPerformanceInfo**  m_stack;
}mem_stack_info;

typedef struct st_trace_sign
{
    size_t          m_size;
    mem_trace_info* m_info;
}trace_sign;

extern void _trace_memory(const char* name, const char* file, int line, trace_sign* sign);
extern void _check_memory(trace_sign* sign);

extern void _trace_unit(HMEMORYUNIT unit);
extern void _untrace_unit(HMEMORYUNIT unit);

extern void _trace_manager(HMEMORYMANAGER mgr);
extern void _untrace_manager(HMEMORYMANAGER mgr);

extern HRBNODE mem_trace_info_head(void);

extern size_t memory_alloc_size(void);
extern size_t memory_total_size(void);

extern void memory_tidy(void);

extern void dump_mem_trace_info(void);
extern mem_trace_info* get_dump_mem_trace_info(mem_trace_info* info);

#endif