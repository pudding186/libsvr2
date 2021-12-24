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

    if (t1->name < t2->name)
    {
        return -1;
    }
    else if (t1->name > t2->name)
    {
        return 1;
    }

    return (t1->stack - t2->stack);
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

ptrdiff_t trace_stack_cmp(const void* ptr1, const void* ptr2)
{
    mem_stack_info* t1 = (mem_stack_info*)ptr1;
    mem_stack_info* t2 = (mem_stack_info*)ptr2;

    if (t1->m_stack_size < t2->m_stack_size)
    {
        return -1;
    }
    else if (t1->m_stack_size > t2->m_stack_size)
    {
        return 1;
    }

    return memcmp(t1->m_stack, t2->m_stack, t1->m_stack_size * sizeof(CFuncPerformanceInfo*));
}

class mem_trace
{
public:
    CThreadLock m_lock;
    HRBTREE     m_trace_info_map;
    HRBTREE     m_trace_unit_map;
    HRBTREE     m_trace_mgr_map;
    HRBTREE     m_trace_stack_map;

    HRBTREE     m_dump_trace_info_map;

    HMEMORYUNIT m_rb_tree_unit;
    HMEMORYUNIT m_rb_node_unit;

    HMEMORYUNIT m_trace_info_unit;

    mem_trace()
    {
        m_rb_tree_unit = create_memory_unit(sizeof_rb_tree());
        
        m_rb_node_unit = create_memory_unit(sizeof_rb_node());

        m_trace_info_unit = create_memory_unit(sizeof(mem_trace_info));
        memory_unit_set_grow_count(m_trace_info_unit, 128);

        m_trace_info_map = create_rb_tree_ex(trace_info_cmp,
            m_rb_tree_unit, m_rb_node_unit);

        m_trace_unit_map = create_rb_tree_ex(0, m_rb_tree_unit, m_rb_node_unit);

        m_trace_mgr_map = create_rb_tree_ex(0, m_rb_tree_unit, m_rb_node_unit);

        m_trace_stack_map = create_rb_tree_ex(trace_stack_cmp,
            m_rb_tree_unit, m_rb_node_unit);

        m_dump_trace_info_map = create_rb_tree_ex(trace_info_cmp,
            m_rb_tree_unit, m_rb_node_unit);

        mem_trace_valid = true;
    }

    ~mem_trace()
    {
        mem_trace_valid = false;

        HRBNODE node = rb_first(m_trace_stack_map);
        while (node)
        {
            free((void*)rb_node_key_user(node));
            node = rb_next(node);
        }

        if (m_trace_info_unit)
        {
            destroy_memory_unit(m_trace_info_unit);
        }

        if (m_rb_tree_unit)
        {
            destroy_memory_unit(m_rb_tree_unit);
        }

        if (m_rb_node_unit)
        {
            destroy_memory_unit(m_rb_node_unit);
        }
    }

    void _trace_memory(const char* name, const char* file, int line, trace_sign* sign)
    {
        HRBNODE node;

        mem_trace_info find_info;

        find_info.file = file;
        find_info.line = line;
        find_info.name = name;
        find_info.stack = nullptr;

        mem_stack_info stack_info;
        stack_info.m_stack_size = GetFuncStackTop(DefFuncPerf()) - 1;
        stack_info.m_stack = GetFuncStack(DefFuncPerf());

        if (stack_info.m_stack_size > 0)
        {
            node = rb_tree_find_user(m_trace_stack_map, &stack_info);

            if (!node)
            {
                unsigned char* ptr = (unsigned char*)malloc(sizeof(mem_stack_info) + sizeof(CFuncPerformanceInfo*) * stack_info.m_stack_size);
                mem_stack_info* info = (mem_stack_info*)ptr;
                info->m_stack_size = stack_info.m_stack_size;
                info->m_stack = (CFuncPerformanceInfo**)(ptr + sizeof(mem_stack_info));
                memcpy(info->m_stack, GetFuncStack(DefFuncPerf()), stack_info.m_stack_size * sizeof(CFuncPerformanceInfo*));

                m_lock.Lock();
                bool ret = rb_tree_try_insert_user(m_trace_stack_map, info, 0, &node);
                m_lock.UnLock();

                if (!ret)
                {
                    free(ptr);
                }
                find_info.stack = (mem_stack_info*)rb_node_key_user(node);
            }
            else
            {
                find_info.stack = (mem_stack_info*)rb_node_key_user(node);
            }
        }

        node = rb_tree_find_user(m_trace_info_map, &find_info);

        if (!node)
        {
            sign->m_info = (mem_trace_info*)memory_unit_alloc(m_trace_info_unit);
            sign->m_info->file = file;
            sign->m_info->line = line;
            sign->m_info->name = name;
            sign->m_info->size = sign->m_size;
            sign->m_info->alloc = 1;
            sign->m_info->free = 0;
            sign->m_info->stack = find_info.stack;

            m_lock.Lock();
            bool ret = rb_tree_try_insert_user(m_trace_info_map, sign->m_info, 0, &node);
            m_lock.UnLock();

            if (ret)
            {
                return;
            }
            else
            {
                memory_unit_free(m_trace_info_unit, sign->m_info);
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

    void _trace_unit(HMEMORYUNIT unit)
    {
        m_lock.Lock();
        HRBNODE node;
        rb_tree_try_insert_user(m_trace_unit_map, unit, unit, &node);

        m_lock.UnLock();
    }

    void _untrace_unit(HMEMORYUNIT unit)
    {
        m_lock.Lock();

        HRBNODE node = rb_tree_find_user(m_trace_unit_map, unit);

        if (node)
        {
            rb_tree_erase(m_trace_unit_map, node);
        }

        m_lock.UnLock();
    }

    void _trace_manager(HMEMORYMANAGER mgr)
    {
        m_lock.Lock();
        HRBNODE node;
        rb_tree_try_insert_user(m_trace_mgr_map, mgr, mgr, &node);

        m_lock.UnLock();
    }

    void _untrace_manager(HMEMORYMANAGER mgr)
    {
        m_lock.Lock();

        HRBNODE node = rb_tree_find_user(m_trace_mgr_map, mgr);

        if (node)
        {
            rb_tree_erase(m_trace_mgr_map, node);
        }

        m_lock.UnLock();
    }

    HRBNODE mem_trace_info_head(void)
    {
        return rb_first(m_trace_info_map);
    }

    size_t memory_alloc_size(void)
    {
        size_t alloc_size = 0;
        HRBNODE node = rb_first(m_trace_unit_map);

        while (node)
        {
            alloc_size += memory_unit_alloc_size((HMEMORYUNIT)rb_node_key_user(node));
            node = rb_next(node);
        }

        node = rb_first(m_trace_mgr_map);
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

        HRBNODE node = rb_first(m_trace_unit_map);

        while (node)
        {
            total_size += memory_unit_total_size((HMEMORYUNIT)rb_node_key_user(node));
            node = rb_next(node);
        }

        node = rb_first(m_trace_mgr_map);
        while (node)
        {
            total_size += memory_manager_total_size((HMEMORYMANAGER)rb_node_key_user(node));
            node = rb_next(node);
        }

        return total_size;
    }

    void memory_tidy(void)
    {
        HRBNODE node = rb_first(m_trace_unit_map);

        while (node)
        {
            memory_unit_tidy((HMEMORYUNIT)rb_node_key_user(node));
            node = rb_next(node);
        }

        node = rb_first(m_trace_mgr_map);
        while (node)
        {
            memory_manager_tidy((HMEMORYMANAGER)rb_node_key_user(node));
            node = rb_next(node);
        }
    }

    static void _check_memory(trace_sign* sign)
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

    void _dump_trace_info_map()
    {
        HRBNODE mem_info_node = rb_first(m_trace_info_map);
        while (mem_info_node)
        {
            mem_trace_info* trace_info = (mem_trace_info*)rb_node_key_user(mem_info_node);
            mem_info_node = rb_next(mem_info_node);

            HRBNODE dump_info_node = rb_tree_find_user(m_dump_trace_info_map, trace_info);

            if (dump_info_node)
            {
                mem_trace_info* dump_trace_info = (mem_trace_info*)rb_node_key_user(dump_info_node);
                
                dump_trace_info->alloc = trace_info->alloc;
                dump_trace_info->free = trace_info->free;
                dump_trace_info->size = trace_info->size;
            }
            else
            {
                mem_trace_info* info = (mem_trace_info*)memory_unit_alloc(m_trace_info_unit);
                info->file = trace_info->file;
                info->line = trace_info->line;
                info->name = trace_info->name;
                info->size = trace_info->size;
                info->alloc = trace_info->alloc;
                info->free = trace_info->free;
                info->stack = trace_info->stack;

                rb_tree_insert_user(m_dump_trace_info_map, info, 0);
            }
        }
    }

    mem_trace_info* _get_dump_trace_info(mem_trace_info* info)
    {
        HRBNODE dump_info_node = rb_tree_find_user(m_dump_trace_info_map, info);

        if (dump_info_node)
        {
            return (mem_trace_info*)rb_node_key_user(dump_info_node);
        }

        return nullptr;
    }
};

mem_trace g_mem_trace;

void _trace_memory(const char* name, const char* file, int line, trace_sign* sign)
{
    if (mem_trace_valid)
    {
        g_mem_trace._trace_memory(name, file, line, sign);
    }
}

void _check_memory(trace_sign* sign)
{
    mem_trace::_check_memory(sign);
}

void _trace_unit(HMEMORYUNIT unit)
{
    if (mem_trace_valid)
    {
        g_mem_trace._trace_unit(unit);
    }
}

void _untrace_unit(HMEMORYUNIT unit)
{
    if (mem_trace_valid)
    {
        g_mem_trace._untrace_unit(unit);
    }
}

void _trace_manager(HMEMORYMANAGER mgr)
{
    if (mem_trace_valid)
    {
        g_mem_trace._trace_manager(mgr);
    }
}

void _untrace_manager(HMEMORYMANAGER mgr)
{
    if (mem_trace_valid)
    {
        g_mem_trace._untrace_manager(mgr);
    }
}

HRBNODE mem_trace_info_head(void)
{
    return g_mem_trace.mem_trace_info_head();
}

size_t memory_alloc_size(void)
{
    return g_mem_trace.memory_alloc_size();
}

size_t memory_total_size(void)
{
    return g_mem_trace.memory_total_size();
}

void memory_tidy(void)
{
    g_mem_trace.memory_tidy();
}

void dump_mem_trace_info(void)
{
    g_mem_trace._dump_trace_info_map();
}

mem_trace_info* get_dump_mem_trace_info(mem_trace_info* info)
{
    return g_mem_trace._get_dump_trace_info(info);
}

