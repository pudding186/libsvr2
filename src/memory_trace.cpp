#include "./lib_svr_common_def.h"
#include "../include/rb_tree.h"
#include "../include/memory_pool.h"
#include "../include/memory_trace.hpp"

bool mem_trace_valid = false;

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
    HRBTREE     m_trace_unit_map;
    HRBTREE     m_trace_mgr_map;
    //HRBTREE     m_trace_ptr_map;
    HMEMORYUNIT m_trace_info_unit;
    //HMEMORYUNIT m_trace_ptr_unit;

    mem_trace()
    {
        m_trace_info_map = create_rb_tree_ex(trace_info_cmp,
            create_memory_unit(sizeof_rb_tree()), create_memory_unit(sizeof_rb_node()));

        m_trace_unit_map = create_rb_tree_ex(0, 
            create_memory_unit(sizeof_rb_tree()), create_memory_unit(sizeof_rb_node()));

        m_trace_mgr_map = create_rb_tree_ex(0,
            create_memory_unit(sizeof_rb_tree()), create_memory_unit(sizeof_rb_node()));

        //m_trace_ptr_map = create_rb_tree_ex(trace_ptr_cmp,
        //    create_memory_unit(sizeof_rb_tree()), create_memory_unit(sizeof_rb_node()));

        m_trace_info_unit = create_memory_unit(sizeof(mem_trace_info));
        memory_unit_set_grow_count(m_trace_info_unit, 128);

        mem_trace_valid = true;
        //m_trace_ptr_unit = create_memory_unit(sizeof(ptr_info));
        //memory_unit_set_grow_count(m_trace_ptr_unit, 8192);
    }

    ~mem_trace()
    {
        mem_trace_valid = false;

        if (m_trace_mgr_map)
        {
            destroy_memory_unit(rb_node_unit(m_trace_mgr_map));
            destroy_memory_unit(rb_tree_unit(m_trace_mgr_map));
        }

        if (m_trace_unit_map)
        {
            destroy_memory_unit(rb_node_unit(m_trace_unit_map));
            destroy_memory_unit(rb_tree_unit(m_trace_unit_map));
        }

        if (m_trace_info_map)
        {
            destroy_memory_unit(rb_node_unit(m_trace_info_map));
            destroy_memory_unit(rb_tree_unit(m_trace_info_map));
        }

        //if (m_trace_ptr_map)
        //{
        //    destroy_memory_unit(rb_node_unit(m_trace_ptr_map));
        //    destroy_memory_unit(rb_tree_unit(m_trace_ptr_map));
        //}

        if (m_trace_info_unit)
        {
            destroy_memory_unit(m_trace_info_unit);
        }

        //if (m_trace_ptr_unit)
        //{
        //    destroy_memory_unit(m_trace_ptr_unit);
        //}
    }
};

mem_trace g_mem_trace;

void _trace_memory(const char* name, const char* file, int line, trace_sign* sign)
{
    if (mem_trace_valid)
    {
        HRBNODE node;

        mem_trace_info find_info;

        find_info.file = file;
        find_info.line = line;
        find_info.name = name;

        node = rb_tree_find_user(g_mem_trace.m_trace_info_map, &find_info);

        if (!node)
        {
            sign->m_info = (mem_trace_info*)memory_unit_alloc(g_mem_trace.m_trace_info_unit);
            sign->m_info->file = file;
            sign->m_info->line = line;
            sign->m_info->name = name;
            sign->m_info->size = sign->m_size;
            sign->m_info->alloc = 1;
            sign->m_info->free = 0;

            g_mem_trace.m_lock.Lock();
            bool ret = rb_tree_try_insert_user(g_mem_trace.m_trace_info_map, sign->m_info, 0, &node);
            g_mem_trace.m_lock.UnLock();

            if (ret)
            {
                return;
            }
            else
            {
                memory_unit_free(g_mem_trace.m_trace_info_unit, sign->m_info);
                sign->m_info = (mem_trace_info*)rb_node_key_user(node);
            }
        }
        else
        {
            sign->m_info = (mem_trace_info*)rb_node_key_user(node);
        }

#ifdef _MSC_VER
        InterlockedAdd64((LONG64*)(&sign->m_info->size), sign->m_size);
        InterlockedIncrement64((LONG64*)(&sign->m_info->alloc));
#elif __GNUC__
        __atomic_add_fetch(&(sign->m_info->size), sign->m_size, __ATOMIC_SEQ_CST);
        __atomic_add_fetch(&(sign->m_info->alloc), 1, __ATOMIC_SEQ_CST);
#else
#error "unknown compiler"
#endif
    }
}

extern void _check_memory(trace_sign* sign)
{
    if (mem_trace_valid)
    {
#ifdef _MSC_VER
        LONG64 s_size = sign->m_size;
        InterlockedAdd64((LONG64*)(&sign->m_info->size), -s_size);
        InterlockedIncrement64((LONG64*)(&sign->m_info->free));
#elif __GNUC__
        __atomic_sub_fetch(&(sign->m_info->size), sign->m_size, __ATOMIC_SEQ_CST);
        __atomic_add_fetch(&(sign->m_info->free), 1, __ATOMIC_SEQ_CST);
#else
#error "unknown compiler"
#endif
    }
}

extern void _trace_unit(HMEMORYUNIT unit)
{
    if (mem_trace_valid)
    {
        g_mem_trace.m_lock.Lock();
        HRBNODE node;
        rb_tree_try_insert_user(g_mem_trace.m_trace_unit_map, unit, unit, &node);

        g_mem_trace.m_lock.UnLock();
    }
}

extern void _untrace_unit(HMEMORYUNIT unit)
{
    if (mem_trace_valid)
    {
        g_mem_trace.m_lock.Lock();

        HRBNODE node = rb_tree_find_user(g_mem_trace.m_trace_unit_map, unit);

        if (node)
        {
            rb_tree_erase(g_mem_trace.m_trace_unit_map, node);
        }

        g_mem_trace.m_lock.UnLock();
    }
}

extern void _trace_manager(HMEMORYMANAGER mgr)
{
    if (mem_trace_valid)
    {
        g_mem_trace.m_lock.Lock();
        HRBNODE node;
        rb_tree_try_insert_user(g_mem_trace.m_trace_mgr_map, mgr, mgr, &node);

        g_mem_trace.m_lock.UnLock();
    }
}

extern void _untrace_manager(HMEMORYMANAGER mgr)
{
    if (mem_trace_valid)
    {
        g_mem_trace.m_lock.Lock();

        HRBNODE node = rb_tree_find_user(g_mem_trace.m_trace_mgr_map, mgr);

        if (node)
        {
            rb_tree_erase(g_mem_trace.m_trace_mgr_map, node);
        }

        g_mem_trace.m_lock.UnLock();
    }
}

HRBNODE mem_trace_info_head(void)
{
    return rb_first(g_mem_trace.m_trace_info_map);
}

size_t memory_alloc_size(void)
{
    size_t alloc_size = 0;
    HRBNODE node = rb_first(g_mem_trace.m_trace_unit_map);

    while (node)
    {
        alloc_size += memory_unit_alloc_size((HMEMORYUNIT)rb_node_key_user(node));
        node = rb_next(node);
    }

    node = rb_first(g_mem_trace.m_trace_mgr_map);
    while (node)
    {
        alloc_size += memory_manager_alloc_size((HMEMORYMANAGER)rb_node_key_user(node));
        node = rb_next(node);
    }

    return alloc_size;
}

size_t memory_total_size(void)
{
    size_t total_size = 0;

    HRBNODE node = rb_first(g_mem_trace.m_trace_unit_map);

    while (node)
    {
        total_size += memory_unit_total_size((HMEMORYUNIT)rb_node_key_user(node));
        node = rb_next(node);
    }

    node = rb_first(g_mem_trace.m_trace_mgr_map);
    while (node)
    {
        total_size += memory_manager_total_size((HMEMORYMANAGER)rb_node_key_user(node));
        node = rb_next(node);
    }

    return total_size;
}

extern void memory_tidy(void)
{
    HRBNODE node = rb_first(g_mem_trace.m_trace_unit_map);

    while (node)
    {
        memory_unit_tidy((HMEMORYUNIT)rb_node_key_user(node));
        node = rb_next(node);
    }

    node = rb_first(g_mem_trace.m_trace_mgr_map);
    while (node)
    {
        memory_manager_tidy((HMEMORYMANAGER)rb_node_key_user(node));
        node = rb_next(node);
    }
}
