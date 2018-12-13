#include "./lib_svr_common_def.h"
#include "../include/rb_tree.h"
#include "../include/memory_pool.h"
#include "../include/memory_trace.hpp"

ptrdiff_t trace_info_cmp(void* info1, void* info2)
{
    MemTraceInfo* t1 = (MemTraceInfo*)info1;
    MemTraceInfo* t2 = (MemTraceInfo*)info2;

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

__declspec(thread) HRBTREE trace_info_map = 0;
__declspec(thread) HRBTREE trace_ptr_map = 0;

__declspec(thread) HMEMORYUNIT trace_info_unit = 0;
__declspec(thread) HMEMORYUNIT trace_ptr_unit = 0;

void trace_alloc(const char* name, const char* file, int line, void* ptr, size_t size)
{
    HRBNODE node;
    MemTraceInfo info;
    MemTraceInfo* exist_info;
    PtrInfo* ptr_info;

    info.file = file;
    info.line = line;
    info.name = name;

    node = rb_tree_find_user(trace_info_map, &info);

    if (node)
    {
        exist_info = (MemTraceInfo*)rb_node_key_user(node);
        exist_info->size += size;
    }
    else
    {
        exist_info = (MemTraceInfo*)memory_unit_alloc(trace_info_unit, 1024);
        exist_info->file = file;
        exist_info->line = line;
        exist_info->name = name;
        exist_info->size = size;

        if (!rb_tree_try_insert_user(trace_info_map, exist_info, 0, &node))
        {
            CRUSH_CODE();
        }
    }

    ptr_info = (PtrInfo*)memory_unit_alloc(trace_ptr_unit, 40960);

    ptr_info->info = exist_info;
    ptr_info->size = size;

    if (!rb_tree_try_insert_user(trace_ptr_map, ptr, ptr_info, &node))
    {
        CRUSH_CODE();
    }
}

void trace_free(void* ptr)
{
    PtrInfo* ptr_info;

    HRBNODE node = rb_tree_find_user(trace_ptr_map, ptr);

    if (node)
    {
        ptr_info = (PtrInfo*)rb_node_value_user(node);

        if (ptr_info->info->size < ptr_info->size)
        {
            CRUSH_CODE();
        }

        ptr_info->info->size -= ptr_info->size;

        rb_tree_erase(trace_ptr_map, node);

        memory_unit_free(trace_ptr_unit, ptr_info);
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