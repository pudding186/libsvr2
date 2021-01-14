#pragma once
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <stdint.h>
#include <limits>
#include <cstdio>
#include <atomic>
#include "./lib_svr_def.h"
#include "./memory_pool.h"
#include "./memory_trace.hpp"
#ifdef  __cplusplus

#ifndef TRACE_MEMORY
#define TRACE_MEMORY
#endif

#ifdef TRACE_MEMORY
#ifdef _MSC_VER
#define S_NEW(type, size, ...) SMemory::TraceNew<type>(size, __FILE__, __LINE__, __VA_ARGS__)
#elif __GNUC__
#define S_NEW(type, size, ...) SMemory::TraceNew<type>(size, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#error "unknown compiler"
#endif

#define S_DELETE(ptr) SMemory::TraceDelete(ptr)
#define S_MALLOC(size) SMemory::TraceAlloc(size, u8"memory", __FILE__, __LINE__)
#define S_MALLOC_EX(size, type) SMemory::TraceAlloc(size, type, __FILE__, __LINE__)
#define S_REALLOC(mem, size) SMemory::TraceRealloc(mem, size, u8"memory", __FILE__, __LINE__)
#define S_REALLOC_EX(mem, size, type) SMemory::TraceRealloc(mem, size, type, __FILE__, __LINE__)
#define S_FREE(mem) SMemory::TraceFree(mem)
#else
#ifdef _MSC_VER
#define S_NEW(type, size, ...) SMemory::New<type>(size, __VA_ARGS__)
#elif __GNUC__
#define S_NEW(type, size, ...) SMemory::New<type>(size, ##__VA_ARGS__)
#else
#error "unknown compiler"
#endif

#define S_DELETE(ptr) SMemory::Delete(ptr)
#define S_MALLOC(size) SMemory::Alloc(size)
#define S_MALLOC_EX(size, type) SMemory::Alloc(size)
#define S_REALLOC(mem, size) SMemory::Realloc(mem, size)
#define S_REALLOC_EX(mem, size, type) SMemory::Realloc(mem, size)
#define S_FREE(mem) SMemory::Free(mem)
#endif

#define REP_DEL_SIG 0x19830116

namespace SMemory
{
    class CMemory
    {
    public:
        CMemory(void);
        ~CMemory(void);

        bool IsValidMem(void* mem);

        void* Alloc(size_t mem_size);

        void* Realloc(void* old_mem, size_t new_size);

        void Free(void* mem);

        void* TraceAlloc(size_t mem_size, const char* type, const char* file, int line);

        void* TraceRealloc(void* old_mem, size_t new_size, const char* type, const char* file, int line);

        void TraceFree(void* mem);

        void Release(void);

        size_t InUse(void);

        inline void SetDestroy(void) { m_can_destroy = true; }
    protected:
        HMEMORYMANAGER  m_memory_mgr;
        bool m_can_destroy;
        std::atomic<bool> m_be_destroy;
    private:
    };

    class CThreadMemory
    {
    public:
        CThreadMemory(void)
        {
            m_memory = new CMemory;
        }

        ~CThreadMemory(void)
        {
            m_memory->SetDestroy();

            if (!m_memory->InUse())
            {
                m_memory->Release();
            }
        }

        inline CMemory& GetMemory(void)
        {
            return *m_memory;
        }
    protected:
        CMemory* m_memory;
    private:
    };

    inline CMemory& get_memory(void)
    {
        static thread_local CThreadMemory thread_memory;

        return thread_memory.GetMemory();
    }

    class IClassMemory
    {
    public:

        IClassMemory(void);
        virtual ~IClassMemory(void);

        virtual void Delete(void* ptr) = 0;

        virtual void TraceDelete(void* ptr) = 0;

        inline HMEMORYUNIT GetUnit(void) { return unit; }

        inline size_t InUse(void) { return memory_unit_alloc_size(unit) + in_use + in_use_mt; }

        inline void SetDestroy(void) { can_destroy = true; }

        void Release(void);

    protected:
        HMEMORYUNIT                     unit;
        size_t                          in_use;
        std::atomic<size_t>             in_use_mt;
        std::atomic<bool>               be_destroy;
        bool                            can_destroy;
    };

    template <typename T, bool is_pod = std::is_pod<T>::value>
    class CClassMemory
        :public IClassMemory
    {
    public:

        virtual void Delete(void* ptr) {};
    };

    template <typename T>
    class CClassMemory<T, false>
        :public IClassMemory
    {
    public:
        CClassMemory<T, false>(void)
        {
#ifdef TRACE_MEMORY
            unit = create_memory_unit(sizeof(trace_sign) + sizeof(IClassMemory**) + sizeof(T));
            _trace_unit(unit);
#else
            unit = create_memory_unit(sizeof(IClassMemory**) + sizeof(T));
#endif
            in_use = 0;
            in_use_mt = 0;
            be_destroy = false;
            can_destroy = false;
        }

        ~CClassMemory<T, false>(void)
        {
#ifdef TRACE_MEMORY
            _untrace_unit(unit);
#endif
            destroy_memory_unit(unit);
        }

        template <typename... Args>
        T* TraceNew(size_t size, const char* name, const char* file, int line, Args&&... args)
        {
            if (size == 1)
            {
                void* ptr = memory_unit_alloc(unit);
                trace_sign* sign = (trace_sign*)ptr;
                sign->m_size = sizeof(T);
                _trace_memory(name, file, line, sign);
                * (IClassMemory**)((unsigned char*)ptr + sizeof(trace_sign)) = this;

                return new((unsigned char*)ptr + sizeof(IClassMemory**) + sizeof(trace_sign))T(std::forward<Args>(args)...);
            }
            else if (size > 1)
            {
                void* ptr = get_memory().Alloc(sizeof(trace_sign) + sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**) + sizeof(T) * size);
                trace_sign* sign = (trace_sign*)ptr;
                sign->m_size = sizeof(T) * size;
                _trace_memory(name, file, line, sign);
                *(size_t*)((unsigned char*)ptr + sizeof(trace_sign)) = size;
                *(CMemory**)((unsigned char*)ptr + sizeof(trace_sign) + sizeof(size_t)) = &get_memory();
                *(IClassMemory**)((unsigned char*)ptr + sizeof(trace_sign) + sizeof(size_t) + sizeof(CMemory**)) = this;
                T* obj = (T*)((unsigned char*)ptr + sizeof(trace_sign) + sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**));

                while (size)
                {
                    new(obj)T(std::forward<Args>(args)...);
                    ++obj;
                    size--;
                }

                if (memory_unit_in_create_thread(unit))
                {
                    in_use += sizeof(T) * size;
                }
                else
                {
                    in_use_mt.fetch_add(sizeof(T) * size);
                }

                return (T*)((unsigned char*)ptr + sizeof(trace_sign) + sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**));
            }

            return 0;
        }

        template <typename... Args>
        T* New(size_t size, Args&&... args)
        {
            if (size == 1)
            {
                void* ptr = memory_unit_alloc(unit);
                * (IClassMemory**)ptr = this;

                return new((unsigned char*)ptr + sizeof(IClassMemory**))T(std::forward<Args>(args)...);
            }
            else if (size > 1)
            {
                void* ptr = get_memory().Alloc(sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**) + sizeof(T) * size);
                *(size_t*)ptr = size;
                *(CMemory**)((unsigned char*)ptr + sizeof(size_t)) = &get_memory();
                *(IClassMemory**)((unsigned char*)ptr + sizeof(size_t) + sizeof(CMemory**)) = this;
                T* obj = (T*)((unsigned char*)ptr + sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**));

                while (size)
                {
                    new(obj)T(std::forward<Args>(args)...);
                    ++obj;
                    size--;
                }

                if (memory_unit_in_create_thread(unit))
                {
                    in_use += sizeof(T) * size;
                }
                else
                {
                    in_use_mt.fetch_add(sizeof(T) * size);
                }

                return (T*)((unsigned char*)ptr + sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**));
            }

            return 0;
        }

        virtual void TraceDelete(void* ptr)
        {
            unsigned char* pTmp = (unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(trace_sign);

            void** check_data = memory_unit_get_sign(pTmp);

            if (memory_unit_sign_to_unit(check_data) == unit)
            {
                T* obj = (T*)ptr;
                obj->~T();
                *(IClassMemory**)(pTmp + sizeof(trace_sign)) = (IClassMemory*)REP_DEL_SIG;
                _check_memory((trace_sign*)pTmp);
                memory_unit_quick_free(unit, check_data);
            }
            else
            {
                CMemory* mem = *(CMemory**)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**));

                size_t size = *(size_t*)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t));

                T* obj = (T*)ptr;

                while (size)
                {
                    obj->~T();
                    ++obj;
                    size--;
                }

                *(IClassMemory**)((unsigned char*)pTmp + sizeof(trace_sign)) = (IClassMemory*)REP_DEL_SIG;
                _check_memory((trace_sign*)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t) - sizeof(trace_sign)));
                mem->Free((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t) - sizeof(trace_sign));

                if (memory_unit_in_create_thread(unit))
                {
                    in_use -= sizeof(T) * size;
                }
                else
                {
                    in_use_mt.fetch_sub(sizeof(T) * size);
                }
            }

            if (can_destroy)
            {
                if (!InUse())
                {
                    Release();
                }
            }
        }

        virtual void Delete(void* ptr)
        {
            unsigned char* pTmp = (unsigned char*)ptr - sizeof(IClassMemory**);

            void** check_data = memory_unit_get_sign(pTmp);

            if (memory_unit_sign_to_unit(check_data) == unit)
            {
                T* obj = (T*)ptr;
                obj->~T();
                *(IClassMemory**)(pTmp) = (IClassMemory*)REP_DEL_SIG;
                memory_unit_quick_free(unit, check_data);
            }
            else
            {
                CMemory* mem = *(CMemory**)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**));

                size_t size = *(size_t*)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t));

                T* obj = (T*)ptr;

                while (size)
                {
                    obj->~T();
                    ++obj;
                    size--;
                }

                *(IClassMemory**)(pTmp) = (IClassMemory*)REP_DEL_SIG;
                mem->Free((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t));

                if (memory_unit_in_create_thread(unit))
                {
                    in_use -= sizeof(T) * size;
                }
                else
                {
                    in_use_mt.fetch_sub(sizeof(T) * size);
                }

            }

            if (can_destroy)
            {
                if (!InUse())
                {
                    Release();
                }
            }
        }
    };

    template <typename T>
    class CClassMemory<T, true>
        :public IClassMemory
    {
    public:

        CClassMemory<T, true>(void)
        {
#ifdef TRACE_MEMORY
            unit = create_memory_unit(sizeof(trace_sign) + sizeof(IClassMemory**) + sizeof(T));
            _trace_unit(unit);
#else
            unit = create_memory_unit(sizeof(IClassMemory**) + sizeof(T));
#endif
            in_use = 0;
            in_use_mt = 0;
            be_destroy = false;
            can_destroy = false;
        }

        ~CClassMemory<T, true>(void)
        {
#ifdef TRACE_MEMORY
            _untrace_unit(unit);
#endif
            destroy_memory_unit(unit);
        }

        T* TraceNew(size_t size, const char* name, const char* file, int line)
        {
            if (size == 1)
            {
                void* ptr = memory_unit_alloc(unit);
                trace_sign* sign = (trace_sign*)ptr;
                sign->m_size = sizeof(T);
                _trace_memory(name, file, line, sign);
                *(IClassMemory**)((unsigned char*)ptr + sizeof(trace_sign)) = this;
                return (T*)((unsigned char*)ptr + sizeof(IClassMemory**) + sizeof(trace_sign));
            }
            else if (size > 1)
            {
                void* ptr = get_memory().Alloc(sizeof(trace_sign) + sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**) + sizeof(T) * size);
                trace_sign* sign = (trace_sign*)ptr;
                sign->m_size = sizeof(T) * size;
                _trace_memory(name, file, line, sign);
                *(size_t*)((unsigned char*)ptr + sizeof(trace_sign)) = size;

                if (memory_unit_in_create_thread(unit))
                {
                    in_use += sizeof(T) * size;
                }
                else
                {
                    in_use_mt.fetch_add(sizeof(T) * size);
                }
                *(CMemory**)((unsigned char*)ptr + sizeof(trace_sign) + sizeof(size_t)) = &get_memory();
                *(IClassMemory**)((unsigned char*)ptr + sizeof(trace_sign) + sizeof(size_t) + sizeof(CMemory**)) = this;
                return (T*)((unsigned char*)ptr + sizeof(trace_sign) + sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**));
            }

            return 0;
        }

        T* New(size_t size)
        {
            if (size == 1)
            {
                void* ptr = memory_unit_alloc(unit);
                * (IClassMemory**)ptr = this;
                return (T*)((unsigned char*)ptr + sizeof(IClassMemory**));
            }
            else if (size > 1)
            {
                void* ptr = get_memory().Alloc(sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**) + sizeof(T) * size);
                *(size_t*)ptr = size;
                *(CMemory**)((unsigned char*)ptr + sizeof(size_t)) = &get_memory();
                *(IClassMemory**)((unsigned char*)ptr + sizeof(size_t) + sizeof(CMemory**)) = this;

                if (memory_unit_in_create_thread(unit))
                {
                    in_use += sizeof(T) * size;
                }
                else
                {
                    in_use_mt.fetch_add(sizeof(T) * size);
                }

                return (T*)((unsigned char*)ptr + sizeof(size_t) + sizeof(CMemory**) + sizeof(IClassMemory**));
            }

            return 0;
        }

        virtual void TraceDelete(void* ptr)
        {
            unsigned char* pTmp = (unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(trace_sign);

            void** check_data = memory_unit_get_sign(pTmp);

            if (memory_unit_sign_to_unit(check_data) == unit)
            {
                *(IClassMemory**)((unsigned char*)pTmp + sizeof(trace_sign)) = (IClassMemory*)REP_DEL_SIG;
                _check_memory((trace_sign*)pTmp);
                memory_unit_quick_free(unit, check_data);
            }
            else
            {
                CMemory* mem = *(CMemory**)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**));
                size_t size = *(size_t*)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t));

                *(IClassMemory**)((unsigned char*)pTmp + sizeof(trace_sign)) = (IClassMemory*)REP_DEL_SIG;
                _check_memory((trace_sign*)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t) - sizeof(trace_sign)));
                mem->Free((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t) - sizeof(trace_sign));

                if (memory_unit_in_create_thread(unit))
                {
                    in_use -= sizeof(T) * size;
                }
                else
                {
                    in_use_mt.fetch_sub(sizeof(T)* size);
                }
            }

            if (can_destroy)
            {
                if (!InUse())
                {
                    Release();
                }
            }
        }

        virtual void Delete(void* ptr)
        {
            unsigned char* pTmp = (unsigned char*)ptr - sizeof(IClassMemory**);

            void** check_data = memory_unit_get_sign(pTmp);

            if (memory_unit_sign_to_unit(check_data) == unit)
            {
                *(IClassMemory**)(pTmp) = (IClassMemory*)REP_DEL_SIG;
                memory_unit_quick_free(unit, check_data);
            }
            else
            {
                CMemory* mem = *(CMemory**)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**));

                size_t size = *(size_t*)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t));

                *(IClassMemory**)(pTmp) = (IClassMemory*)REP_DEL_SIG;
                mem->Free((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(CMemory**) - sizeof(size_t));


                if (memory_unit_in_create_thread(unit))
                {
                    in_use -= sizeof(T) * size;
                }
                else
                {
                    in_use_mt.fetch_sub(sizeof(T) * size);
                }
            }

            if (can_destroy)
            {
                if (!InUse())
                {
                    Release();
                }
            }
        }
    };

    template<typename T>
    class CThreadClassMemory
    {
    public:
        CThreadClassMemory(void)
        {
            m_class_memory = new CClassMemory<T>();
        }

        ~CThreadClassMemory(void)
        {
            m_class_memory->SetDestroy();


            if (!m_class_memory->InUse())
            {
                m_class_memory->Release();
            }
        }

        inline CClassMemory<T>& GetClassMemory(void)
        {
            return *m_class_memory;
        }
    protected:
        CClassMemory<T>* m_class_memory;
    private:
    };

    template <typename T>
    inline CClassMemory<T>& get_class_memory(void)
    {
        static thread_local CThreadClassMemory<T> thread_class_memory;

        return thread_class_memory.GetClassMemory();
    }

    template <typename T, typename... Args>
    T* New(size_t size, Args&&... args)
    {
        return get_class_memory<T>().New(size, std::forward<Args>(args)...);
    }

    template <typename T>
    bool IsValidPtr(void* ptr)
    {
        return get_class_memory<T>().IsValidPtr(ptr);
    }

    extern void Delete(void* ptr);

    template <typename T, typename... Args>
    T* TraceNew(size_t size, const char* file, int line, Args&&... args)
    {
        static const char* name = typeid(T).name();

        return get_class_memory<T>().TraceNew(size, name, file, line, std::forward<Args>(args)...);
    }

    extern void TraceDelete(void* ptr);

    //////////////////////////////////////////////////////////////////////////

    extern bool IsValidMem(void* mem);

    extern void* Alloc(size_t mem_size);

    extern void* Realloc(void* old_mem, size_t new_size);

    extern void Free(void* mem);

    extern void* TraceAlloc(size_t mem_size, const char* type, const char* file, int line);

    extern void* TraceRealloc(void* old_mem, size_t new_size, const char* type, const char* file, int line);

    extern void TraceFree(void* mem);

    //////////////////////////////////////////////////////////////////////////


    template<typename T>
    struct Allocator_base
    {
        typedef T value_type;
    };

    template<typename T>
    struct Allocator_base<const T>
    {
        typedef T value_type;
    };

    template<typename T>
    class Allocator
        :public Allocator_base<T>
    {
    public:
        typedef typename std::size_t size_type;
        typedef typename std::ptrdiff_t difference_type;
        typedef T* pointer;
        typedef const T* const_pointer;
        typedef T& reference;
        typedef const T& const_reference;
        typedef Allocator_base<T> _Mybase;
        typedef typename _Mybase::value_type value_type;

        template<typename _other>
        struct rebind
        {
            typedef Allocator<_other> other;
        };

        pointer address(reference value)const {
            return &value;
        }
        const_pointer address(const_reference value)const {
            return (const_pointer)&value;
        }

        Allocator() throw() {}
        Allocator(const Allocator &)throw() {}
        template<class _otherAll>
        Allocator(const Allocator<_otherAll> &)throw() {}
        ~Allocator()throw() {}

        size_type max_size()const throw()
        {
            return (std::numeric_limits<size_type>::max)() / sizeof(T);
        }

        pointer allocate(size_type num, const void* hint = 0)
        {
            static const char* name = typeid(T).name();

            return (pointer)S_MALLOC_EX(sizeof(value_type)*num, name);
        }

        void construct(pointer p, const_reference value)
        {
            new(p) T(value);
        }

        void destroy(pointer p)
        {
            p->~T();
        }

        void deallocate(pointer p, size_type size)
        {
            S_FREE(p);
        }

        bool operator==(Allocator const& a) const
        {
            return true;
        }

        bool operator!=(Allocator const& a) const
        {
            return !operator==(a);
        }
    };
}

#endif