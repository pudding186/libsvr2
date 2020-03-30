#include "./lib_svr_common_def.h"
#include "../include/rb_tree.h"
#include "../include/memory_pool.h"
#include "../include/memory_trace.hpp"

#ifdef _MSC_VER
#include <Windows.h>
class CThreadLock
{
public:
    CThreadLock(void)
    {
        InitializeCriticalSection(&m_stCritical_section);
    }
public:
    ~CThreadLock(void)
    {
        DeleteCriticalSection(&m_stCritical_section);
    }

    inline void Lock(void)
    {
        EnterCriticalSection(&m_stCritical_section);
    }

    inline void UnLock(void)
    {
        LeaveCriticalSection(&m_stCritical_section);
    }
protected:
    CRITICAL_SECTION    m_stCritical_section;
};
#elif __GNUC__
#include <pthread.h>
class CThreadLock
{
public:
    CThreadLock(void)
    {
        pthread_mutex_init(&m_stpthread_mutex, NULL);
    }
    ~CThreadLock(void)
    {
        pthread_mutex_destroy(&m_stpthread_mutex);
    }

    inline void Lock(void)
    {
        pthread_mutex_lock(&m_stpthread_mutex);
    }

    inline void UnLock(void)
    {
        pthread_mutex_unlock(&m_stpthread_mutex);
    }
protected:
    pthread_mutex_t m_stpthread_mutex;
};
#else
#error "unknown compiler"
#endif


ptrdiff_t trace_info_cmp(const void* info1, const void* info2)
{
    mem_trace_info* t1 = (mem_trace_info*)info1;
    mem_trace_info* t2 = (mem_trace_info*)info2;

    if (t1->line < t2->line)
    {
        return -1;
    }
    else if (t1->line > t2->line)
    {
        return 1;
    }

    if (t1->file < t2->file)
    {
        return -1;
    }
    else if (t1->file > t2->file)
    {
        return 1;
    }

    return (t1->name - t2->name);
}

ptrdiff_t trace_ptr_cmp(const void* ptr1, const void* ptr2)
{
    if (ptr1 < ptr2)
    {
        return -1;
    }
    else if (ptr1 > ptr2)
    {
        return 1;
    }

    return 0;
}

//TLS_VAR HRBTREE trace_info_map = 0;
//TLS_VAR HRBTREE trace_ptr_map = 0;
//
//TLS_VAR HMEMORYUNIT trace_info_unit = 0;
//TLS_VAR HMEMORYUNIT trace_ptr_unit = 0;

class mem_trace
{
public:
    CThreadLock m_lock;
    HRBTREE     m_trace_info_map;
    HRBTREE     m_trace_ptr_map;
    HMEMORYUNIT m_trace_info_unit;
    HMEMORYUNIT m_trace_ptr_unit;

    mem_trace()
    {
        m_trace_info_map = create_rb_tree_ex(trace_info_cmp,
            create_memory_unit(sizeof_rb_tree()), create_memory_unit(sizeof_rb_node()));

        m_trace_ptr_map = create_rb_tree_ex(trace_ptr_cmp,
            create_memory_unit(sizeof_rb_tree()), create_memory_unit(sizeof_rb_node()));

        m_trace_info_unit = create_memory_unit(sizeof(mem_trace_info));
        memory_unit_set_grow_count(m_trace_info_unit, 128);
        m_trace_ptr_unit = create_memory_unit(sizeof(ptr_info));
        memory_unit_set_grow_count(m_trace_ptr_unit, 8192);
    }

    ~mem_trace()
    {
        if (m_trace_info_map)
        {
            destroy_memory_unit(rb_node_unit(m_trace_info_map));
            destroy_memory_unit(rb_tree_unit(m_trace_info_map));
        }

        if (m_trace_ptr_map)
        {
            destroy_memory_unit(rb_node_unit(m_trace_ptr_map));
            destroy_memory_unit(rb_tree_unit(m_trace_ptr_map));
        }

        if (m_trace_info_unit)
        {
            destroy_memory_unit(m_trace_info_unit);
        }

        if (m_trace_ptr_unit)
        {
            destroy_memory_unit(m_trace_ptr_unit);
        }
    }
};

mem_trace g_mem_trace;

void trace_alloc(const char* name, const char* file, int line, void* ptr, size_t size)
{
    HRBNODE node;
    mem_trace_info info;
    mem_trace_info* exist_info;
    ptr_info* _ptr_info;

    info.file = file;
    info.line = line;
    info.name = name;

    g_mem_trace.m_lock.Lock();

    node = rb_tree_find_user(g_mem_trace.m_trace_info_map, &info);

    if (node)
    {
        exist_info = (mem_trace_info*)rb_node_key_user(node);
        exist_info->size += size;
        exist_info->alloc++;
    }
    else
    {
        exist_info = (mem_trace_info*)memory_unit_alloc(g_mem_trace.m_trace_info_unit);
        exist_info->file = file;
        exist_info->line = line;
        exist_info->name = name;
        exist_info->size = size;
        exist_info->alloc = 1;
        exist_info->free = 0;

        if (!rb_tree_try_insert_user(g_mem_trace.m_trace_info_map, exist_info, 0, &node))
        {
            CRUSH_CODE();
        }
    }

    _ptr_info = (ptr_info*)memory_unit_alloc(g_mem_trace.m_trace_ptr_unit);

    _ptr_info->info = exist_info;
    _ptr_info->size = size;

    if (!rb_tree_try_insert_user(g_mem_trace.m_trace_ptr_map, ptr, _ptr_info, &node))
    {
        CRUSH_CODE();
    }
    g_mem_trace.m_lock.UnLock();
}

void trace_free(void* ptr)
{
    ptr_info* _ptr_info;

    g_mem_trace.m_lock.Lock();

    HRBNODE node = rb_tree_find_user(g_mem_trace.m_trace_ptr_map, ptr);

    if (node)
    {
        _ptr_info = (ptr_info*)rb_node_value_user(node);

        if (_ptr_info->info->size < _ptr_info->size)
        {
            CRUSH_CODE();
        }

        _ptr_info->info->size -= _ptr_info->size;
        _ptr_info->info->free++;

        rb_tree_erase(g_mem_trace.m_trace_ptr_map, node);

        memory_unit_free(g_mem_trace.m_trace_ptr_unit, _ptr_info);
    }
    else
    {
        CRUSH_CODE();
    }

    g_mem_trace.m_lock.UnLock();
}

HRBNODE mem_trace_info_head(void)
{
    return rb_first(g_mem_trace.m_trace_info_map);
}