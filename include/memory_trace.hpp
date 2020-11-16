#pragma once
#include "./lib_svr_def.h"
#include <utility>
#ifdef  __cplusplus

typedef struct st_trace_sign
{
    size_t          m_size;
    mem_trace_info* m_info;
}trace_sign;

template<typename T>
class TraceMemory
{
public:
    trace_sign      m_sign;
    T               m_obj;

    template<typename... Args>
    TraceMemory(Args&&... args)
        :m_obj(std::forward<Args>(args)...)
    {
        m_sign.m_size = 0;
        m_sign.m_info = nullptr;
    }
    ~TraceMemory(){}
protected:
private:
};

extern void _trace_memory(const char* name, const char* file, int line, trace_sign* sign);
extern void _check_memory(trace_sign* sign);


//extern void trace_alloc(const char* name, const char* file, int line, void* ptr, size_t size);
//extern void trace_free(void* ptr);

extern HRBNODE mem_trace_info_head(void);
#endif