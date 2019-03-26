#include "./lib_svr_common_def.h"
#include "../include/rb_tree.h"
#include "../include/memory_pool.h"
#include "../include/memory_trace.hpp"

ptrdiff_t trace_info_cmp(void* info1, void* info2)
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

ptrdiff_t trace_ptr_cmp(void* ptr1, void* ptr2)
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

TLS_VAR HRBTREE trace_info_map = 0;
TLS_VAR HRBTREE trace_ptr_map = 0;

TLS_VAR HMEMORYUNIT trace_info_unit = 0;
TLS_VAR HMEMORYUNIT trace_ptr_unit = 0;

void trace_alloc(const char* name, const char* file, int line, void* ptr, size_t size)
{
    HRBNODE node;
    mem_trace_info info;
    mem_trace_info* exist_info;
    ptr_info* _ptr_info;

    info.file = file;
    info.line = line;
    info.name = name;

    node = rb_tree_find_user(trace_info_map, &info);

    if (node)
    {
        exist_info = (mem_trace_info*)rb_node_key_user(node);
        exist_info->size += size;
    }
    else
    {
        exist_info = (mem_trace_info*)memory_unit_alloc(trace_info_unit);
        exist_info->file = file;
        exist_info->line = line;
        exist_info->name = name;
        exist_info->size = size;

        if (!rb_tree_try_insert_user(trace_info_map, exist_info, 0, &node))
        {
            CRUSH_CODE();
        }
    }

    _ptr_info = (ptr_info*)memory_unit_alloc(trace_ptr_unit);

    _ptr_info->info = exist_info;
    _ptr_info->size = size;

    if (!rb_tree_try_insert_user(trace_ptr_map, ptr, _ptr_info, &node))
    {
        CRUSH_CODE();
    }
}

void trace_free(void* ptr)
{
    ptr_info* _ptr_info;

    HRBNODE node = rb_tree_find_user(trace_ptr_map, ptr);

    if (node)
    {
        _ptr_info = (ptr_info*)rb_node_value_user(node);

        if (_ptr_info->info->size < _ptr_info->size)
        {
            CRUSH_CODE();
        }

        _ptr_info->info->size -= _ptr_info->size;

        rb_tree_erase(trace_ptr_map, node);

        memory_unit_free(trace_ptr_unit, _ptr_info);
    }
    else
    {
        CRUSH_CODE();
    }
}

HRBNODE mem_trace_info_head(void)
{
    return rb_first(trace_info_map);
}