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
        if (!ptr)
        {
            return;
        }
        IClassMemory** pool = (IClassMemory**)((unsigned char*)ptr - sizeof(IClassMemory**));
        (*pool)->TraceDelete(ptr);
    }

    bool IsValidMem(void* mem)
    {
        CMemory* memory = *(CMemory**)((unsigned char*)mem - sizeof(CMemory**));

        return memory->IsValidMem((unsigned char*)mem - sizeof(CMemory**));
    }

    void* Alloc(size_t mem_size)
    {
        void* ptr = get_memory().Alloc(sizeof(CMemory**) + mem_size);

        *(CMemory**)(ptr) = &get_memory();

        return ((unsigned char*)ptr + sizeof(CMemory**));
    }

    void* Realloc(void* old_mem, size_t new_size)
    {
        if (old_mem)
        {
            CMemory* memory = *(CMemory**)((unsigned char*)old_mem - sizeof(CMemory**));

            void* ptr = memory->Realloc((unsigned char*)old_mem - sizeof(CMemory**), new_size + sizeof(CMemory**));

            return ((unsigned char*)ptr + sizeof(CMemory**));
        }
        else
        {
            return Alloc(new_size);
        }
    }

    void Free(void* mem)
    {
        CMemory* memory = *(CMemory**)((unsigned char*)mem - sizeof(CMemory**));

        memory->Free((unsigned char*)mem - sizeof(CMemory**));
    }

    void* TraceAlloc(size_t mem_size, const char* type, const char* file, int line)
    {
        void* ptr = get_memory().TraceAlloc(sizeof(CMemory**) + mem_size, type, file, line);

        *(CMemory**)(ptr) = &get_memory();

        return ((unsigned char*)ptr + sizeof(CMemory**));
    }

    void* TraceRealloc(void* old_mem, size_t new_size, const char* type, const char* file, int line)
    {
        if (old_mem)
        {
            CMemory* memory = *(CMemory**)((unsigned char*)old_mem - sizeof(CMemory**));

            void* ptr = memory->TraceRealloc((unsigned char*)old_mem - sizeof(CMemory**), new_size + sizeof(CMemory**), type, file, line);

            return ((unsigned char*)ptr + sizeof(CMemory**));
        }
        else
        {
            return TraceAlloc(new_size, type, file, line);
        }
    }

    void TraceFree(void* mem)
    {
        if (mem)
        {
            CMemory* memory = *(CMemory**)((unsigned char*)mem - sizeof(CMemory**));

            memory->TraceFree((unsigned char*)mem - sizeof(CMemory**));
        }
    }

    IClassMemory::IClassMemory(void)
    {
        //g_def_mem_init.init_ref++;
    }

    IClassMemory::~IClassMemory(void)
    {
        //g_def_mem_init.init_ref--;
    }

    void IClassMemory::Release(void)
    {
        bool exp = false;
        if (be_destroy.compare_exchange_strong(exp, true))
        {
            delete this;
        }
    }

    CMemory::CMemory(void)
    {
        m_memory_mgr = create_memory_manager(8, 128, 1024, 4 * 1024, 2);
        m_can_destroy = false;
        m_be_destroy = false;
    }

    CMemory::~CMemory()
    {
        destroy_memory_manager(m_memory_mgr);
    }

    bool CMemory::IsValidMem(void* mem)
    {
        return memory_manager_check(m_memory_mgr, mem);
    }

    void* CMemory::Alloc(size_t mem_size)
    {
        return memory_manager_alloc(m_memory_mgr, mem_size);
    }

    void* CMemory::Realloc(void* old_mem, size_t new_size)
    {
        return memory_manager_realloc(m_memory_mgr, old_mem, new_size);
    }

    void CMemory::Free(void* mem)
    {
        memory_manager_free(m_memory_mgr, mem);

        if (m_can_destroy)
        {
            if (!InUse())
            {
                Release();
            }
        }
    }

    void* CMemory::TraceAlloc(size_t mem_size, const char* type, const char* file, int line)
    {
        void* mem = memory_manager_alloc(m_memory_mgr, mem_size + sizeof(trace_sign));

        trace_sign* sign = (trace_sign*)mem;
        sign->m_size = mem_size;

        _trace_memory(type, file, line, sign);

        return ((unsigned char*)mem + sizeof(trace_sign));
    }

    void* CMemory::TraceRealloc(void* old_mem, size_t new_size, const char* type, const char* file, int line)
    {
        if (old_mem)
        {
            trace_sign* sign = (trace_sign*)((unsigned char*)old_mem - sizeof(trace_sign));
            old_mem = sign;

            if (!IsValidMem(sign))
            {
                CRUSH_CODE();
            }

            _check_memory(sign);
        }
        void* new_mem = memory_manager_realloc(m_memory_mgr, old_mem, new_size + sizeof(trace_sign));

        trace_sign* sign = (trace_sign*)new_mem;
        sign->m_size = new_size;

        _trace_memory(type, file, line, sign);

        return ((unsigned char*)new_mem + sizeof(trace_sign));
    }

    void CMemory::TraceFree(void* mem)
    {
        if (!mem)
        {
            return;
        }
        trace_sign* sign = (trace_sign*)((unsigned char*)mem - sizeof(trace_sign));

        if (!IsValidMem(sign))
        {
            CRUSH_CODE();
        }

        _check_memory(sign);
        memory_manager_free(m_memory_mgr, sign);

        if (m_can_destroy)
        {
            if (!InUse())
            {
                Release();
            }
        }
    }

    void CMemory::Release(void)
    {
        bool exp = false;
        if (m_be_destroy.compare_exchange_strong(exp, true))
        {
            delete this;
        }
    }

    size_t CMemory::InUse(void)
    {
        return memory_manager_alloc_size(m_memory_mgr);
    }
}

#ifdef  __cplusplus
extern "C" {
#endif

void* default_alloc_rb_node(void)
{
    return SMemory::New<rb_node>(1);
}

void default_free_rb_node(void* node)
{
    SMemory::Delete(node);
}

void* default_alloc_rb_tree(void)
{
    return SMemory::New<rb_tree>(1);
}

void default_free_rb_tree(void* tree)
{
    SMemory::Delete(tree);
}

void* default_alloc_avl_node(void)
{
    return SMemory::New<avl_node>(1);
}

void default_free_avl_node(void* node)
{
    SMemory::Delete(node);
}

void* default_alloc_avl_tree(void)
{
    return SMemory::New<avl_tree>(1);
}

void default_free_avl_tree(void* tree)
{
    SMemory::Delete(tree);
}

void* default_alloc_json_struct(void)
{
    return SMemory::New<json_struct>(1);
}

void default_free_json_struct(void* js)
{
    SMemory::Delete(js);
}

void* default_alloc_json_node(void)
{
    return SMemory::New<json_node>(1);
}

void default_free_json_node(void* jn)
{
    SMemory::Delete(jn);
}

void* default_alloc(size_t mem_size)
{
    return SMemory::Alloc(mem_size);
}

void* default_realloc(void* old_mem, size_t mem_size)
{
    return SMemory::Realloc(old_mem, mem_size);
}

void default_free(void* mem)
{
    SMemory::Free(mem);
}

#ifdef  __cplusplus
}
#endif

