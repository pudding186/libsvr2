#include "./lib_svr_common_def.h"
#include "../include/smemory.hpp"
#include "../include/utility.hpp"
#include "../include/rb_tree.h"


extern "C"
{
	extern TLS_VAR HMEMORYUNIT def_avl_tree_unit;
	extern TLS_VAR HMEMORYUNIT def_avl_node_unit;

	extern TLS_VAR HMEMORYUNIT def_rb_tree_unit;
	extern TLS_VAR HMEMORYUNIT def_rb_node_unit;

	extern TLS_VAR HMEMORYMANAGER lib_svr_mem_mgr;

	extern TLS_VAR HMEMORYUNIT def_json_struct_unit;
	extern TLS_VAR HMEMORYUNIT def_json_node_unit;
}

extern TLS_VAR CFuncPerformanceMgr* def_func_perf_mgr;

static TLS_VAR HMEMORYMANAGER   def_mem_mgr = 0;

extern void check_log_proc(void);

namespace SMemory
{
    //////////////////////////////////////////////////////////////////////////
    class DefMemInit
    {
    public:
        DefMemInit()
        {
            init_ref = 0;

            if (!def_mem_mgr)
            {
                def_mem_mgr = create_memory_manager(8, 128, 65536, 4 * 1024, 2);
            }

            if (!lib_svr_mem_mgr)
            {
                lib_svr_mem_mgr = create_memory_manager(8, 128, 65536, 4 * 1024, 2);
            }

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
        }
        ~DefMemInit()
        {
            check_log_proc();

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

            if (lib_svr_mem_mgr)
            {
                destroy_memory_manager(lib_svr_mem_mgr);
                lib_svr_mem_mgr = 0;
            }

            if (def_mem_mgr)
            {
                destroy_memory_manager(def_mem_mgr);
                def_mem_mgr = 0;
            }
        }
        size_t init_ref;
    protected:
    private:
    };

    static thread_local DefMemInit g_def_mem_init;
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
        g_def_mem_init.init_ref++;
    }

    IClassMemory::~IClassMemory(void)
    {
        g_def_mem_init.init_ref--;
    }

    bool IClassMemory::IsValidPtr(void* ptr)
    {
        unsigned char* ptr_check = (unsigned char*)ptr - sizeof(IClassMemory**);

        if ((*(IClassMemory**)(ptr_check)) != this)
        {
            return false;
        }

        if (memory_unit_check(unit, ptr_check))
        {
            return true;
        }

        HMEMORYMANAGER check_mem_mgr = *(HMEMORYMANAGER*)(ptr_check - sizeof(HMEMORYMANAGER*));

        return memory_manager_check(check_mem_mgr, ptr_check - sizeof(HMEMORYMANAGER*) - sizeof(size_t));
    }

    void* IClassMemory::Alloc(size_t mem_size)
    {
        return memory_manager_alloc(def_mem_mgr, mem_size);
    }

    void* IClassMemory::Realloc(void* old_mem, size_t new_size)
    {
        return memory_manager_realloc(def_mem_mgr, old_mem, new_size);
    }

    void IClassMemory::Free(void* mem)
    {
        memory_manager_free(def_mem_mgr, mem);
    }
    
    bool IClassMemory::IsValidMem(void* mem)
    {
        return memory_manager_check(def_mem_mgr, mem);
    }

    void* IClassMemory::TraceAlloc(size_t mem_size, const char* file, int line)
    {
        void* mem = memory_manager_alloc(def_mem_mgr, mem_size);
        trace_alloc("alloc", file, line, mem, mem_size);

        return mem;
    }

    void* IClassMemory::TraceRealloc(void* old_mem, size_t new_size, const char* file, int line)
    {
        if (old_mem)
        {
            trace_free(old_mem);
        }
        void* new_mem = memory_manager_realloc(def_mem_mgr, old_mem, new_size);
        trace_alloc("realloc", file, line, new_mem, new_size);
        return new_mem;
    }

    void IClassMemory::TraceFree(void* mem)
    {
        trace_free(mem);
        memory_manager_free(def_mem_mgr, mem);
    }

    HMEMORYMANAGER IClassMemory::DefMemMgr(void)
    {
        return def_mem_mgr;
    }
}