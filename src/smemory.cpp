#include "./lib_svr_common_def.h"
#include "../include/smemory.hpp"
#include "../include/utility.hpp"
#include "../include/rb_tree.h"

namespace SMemory
{
    //////////////////////////////////////////////////////////////////////////
    void Delete(void* ptr)
    {
        if (!ptr)
        {
            return;
        }
        IClassMemory** pool = (IClassMemory**)((unsigned char*)ptr - sizeof(IClassMemory**));
        (*pool)->Delete(ptr);
    }

    void TraceDelete(void* ptr)
    {
        trace_free(ptr);
        Delete(ptr);
    }

    IClassMemory::IClassMemory(void)
    {

    }

    IClassMemory::~IClassMemory(void)
    {

    }

    __declspec(thread) HMEMORYMANAGER IClassMemory::def_mem_mgr = 0;

    bool IClassMemory::IsValidPtr(void* ptr)
    {
        if (*(IClassMemory**)((unsigned char*)ptr - sizeof(IClassMemory**)) != this)
        {
            return false;
        }

        if (*(HMEMORYMANAGER*)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(HMEMORYMANAGER*)) != def_mem_mgr)
        {
            return false;
        }

        if (memory_unit_check(unit, (unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(HMEMORYMANAGER*)))
        {
            return true;
        }

        return memory_manager_check(def_mem_mgr, (unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(HMEMORYMANAGER*) - sizeof(size_t));
    }
}

class DefMemInit
{
public:
    DefMemInit()
    {
        if (!lib_svr_mem_mgr)
        {
            lib_svr_mem_mgr = create_memory_manager(8, 128, 65536, 4 * 1024, 2);
        }

        SMemory::IClassMemory::Init();

        if (!def_rb_tree_unit)
        {
            def_rb_tree_unit = create_memory_unit(sizeof(rb_tree));
        }

        if (!def_rb_node_unit)
        {
            def_rb_node_unit = create_memory_unit(sizeof(rb_node));
        }

        if (!def_avl_tree_unit)
        {
            def_avl_tree_unit = create_memory_unit(sizeof(avl_tree));
        }

        if (!def_avl_node_unit)
        {
            def_avl_node_unit = create_memory_unit(sizeof(avl_node));
        }

        if (!def_json_struct_unit)
        {
            def_json_struct_unit = create_memory_unit(sizeof(json_struct));
        }

        if (!def_json_node_unit)
        {
            def_json_node_unit = create_memory_unit(sizeof(json_node));
        }

        if (!def_func_perf_mgr)
        {
            def_func_perf_mgr = CreateFuncPerfMgr();
        }

#ifdef _DEBUG

        if (!trace_info_map)
        {
            trace_info_map = create_rb_tree(trace_info_cmp);
        }

        if (!trace_ptr_map)
        {
            trace_ptr_map = create_rb_tree(trace_ptr_cmp);
        }

        if (!trace_info_unit)
        {
            trace_info_unit = create_memory_unit(sizeof(MemTraceInfo));
        }

        if (!trace_ptr_unit)
        {
            trace_ptr_unit = create_memory_unit(sizeof(PtrInfo));
        }
#endif
    }
    ~DefMemInit()
    {
#ifdef _DEBUG
        if (trace_info_map)
        {
            destroy_rb_tree(trace_info_map);
        }

        if (trace_ptr_map)
        {
            destroy_rb_tree(trace_ptr_map);
        }

        if (trace_info_unit)
        {
            destroy_memory_unit(trace_info_unit);
        }

        if (trace_ptr_unit)
        {
            destroy_memory_unit(trace_ptr_unit);
        }
#endif
        if (def_func_perf_mgr)
        {
            DestroyFuncPerfMgr(def_func_perf_mgr);
            def_func_perf_mgr = 0;
        }

        if (def_json_node_unit)
        {
            destroy_memory_unit(def_json_node_unit);
            def_json_node_unit = 0;
        }

        if (def_json_struct_unit)
        {
            destroy_memory_unit(def_json_struct_unit);
            def_json_struct_unit = 0;
        }

        if (def_avl_node_unit)
        {
            destroy_memory_unit(def_avl_node_unit);
            def_avl_node_unit = 0;
        }

        if (def_avl_tree_unit)
        {
            destroy_memory_unit(def_avl_tree_unit);
            def_avl_tree_unit = 0;
        }

        if (def_rb_node_unit)
        {
            destroy_memory_unit(def_rb_node_unit);
            def_rb_node_unit = 0;
        }

        if (def_rb_tree_unit)
        {
            destroy_memory_unit(def_rb_tree_unit);
            def_rb_tree_unit = 0;
        }

        SMemory::IClassMemory::UnInit();

        if (lib_svr_mem_mgr)
        {
            destroy_memory_manager(lib_svr_mem_mgr);
            lib_svr_mem_mgr = 0;
        }
    }
protected:
private:
};

__declspec(thread) static DefMemInit g_def_mem_init;