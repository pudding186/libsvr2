#pragma once
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <stdint.h>
#include <limits>
#include <cstdio>
#include "./lib_svr_def.h"
#include "../include/memory_pool.h"
#include "../include/memory_trace.hpp"
#ifdef  __cplusplus

#ifdef _DEBUG
#ifdef _MSC_VER
#define S_NEW(type, size, ...) SMemory::TraceNew<type>(size, __FILE__, __LINE__, __VA_ARGS__)
#elif __GNUC__
#define S_NEW(type, size, ...) SMemory::TraceNew<type>(size, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#error "unknown compiler"
#endif

#define S_DELETE(ptr) SMemory::TraceDelete(ptr)
#define S_MALLOC(size) SMemory::IClassMemory::TraceAlloc(size, __FILE__, __LINE__)
#define S_REALLOC(mem, size) SMemory::IClassMemory::TraceRealloc(mem, size, __FILE__, __LINE__)
#define S_FREE(mem) SMemory::IClassMemory::TraceFree(mem)
#else
#ifdef _MSC_VER
#define S_NEW(type, size, ...) SMemory::New<type>(size, __VA_ARGS__)
#elif __GNUC__
#define S_NEW(type, size, ...) SMemory::New<type>(size, ##__VA_ARGS__)
#else
#error "unknown compiler"
#endif

#define S_DELETE(ptr) SMemory::Delete(ptr)
#define S_MALLOC(size) SMemory::IClassMemory::Alloc(size)
#define S_REALLOC(mem, size) SMemory::IClassMemory::Realloc(mem, size)
#define S_FREE(mem) SMemory::IClassMemory::Free(mem)
#endif

#define REP_DEL_SIG 0xa1b2c3d4
#define THD_DEL_SIG 0xaabbccdd

namespace SMemory
{
    class IClassMemory
    {
    public:

        IClassMemory(void);
        virtual ~IClassMemory(void);

        virtual void Delete(void* ptr) = 0;

        bool IsValidPtr(void* ptr);

        //////////////////////////////////////////////////////////////////////////

        static void* Alloc(size_t mem_size);

        static void* Realloc(void* old_mem, size_t new_size);

        static void Free(void* mem);

        static bool IsValidMem(void* mem);

        static void* TraceAlloc(size_t mem_size, const char* file, int line);

        static void* TraceRealloc(void* old_mem, size_t new_size, const char* file, int line);

        static void TraceFree(void* mem);

        static HMEMORYMANAGER DefMemMgr(void);

    protected:
        HMEMORYUNIT                     unit;
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
            unit = create_memory_unit(sizeof(HMEMORYMANAGER*) + sizeof(IClassMemory**) + sizeof(T));
        }

        ~CClassMemory<T, false>(void)
        {
            destroy_memory_unit(unit);
        }

        template <typename... Args>
        T* New(size_t size, Args&&... args)
        {
            if (size == 1)
            {
                void* ptr = memory_unit_alloc(unit);
                *(HMEMORYMANAGER*)ptr = DefMemMgr();
                *(IClassMemory**)((unsigned char*)ptr + sizeof(HMEMORYMANAGER*)) = this;

                return new((unsigned char*)ptr + sizeof(HMEMORYMANAGER*) + sizeof(IClassMemory**))T(std::forward<Args>(args)...);
            }
            else if (size > 1)
            {
                void* ptr = memory_manager_alloc(DefMemMgr(), sizeof(size_t) + sizeof(HMEMORYMANAGER*) + sizeof(IClassMemory**) + sizeof(T)*size);
                *(size_t*)ptr = size;
                *(HMEMORYMANAGER*)((unsigned char*)ptr + sizeof(size_t)) = DefMemMgr();
                *(IClassMemory**)((unsigned char*)ptr + sizeof(size_t) + sizeof(HMEMORYMANAGER*)) = this;
                T* obj = (T*)((unsigned char*)ptr + sizeof(size_t) + sizeof(HMEMORYMANAGER*) + sizeof(IClassMemory**));

                while (size)
                {
                    new(obj)T(std::forward<Args>(args)...);
                    ++obj;
                    size--;
                }

                return (T*)((unsigned char*)ptr + sizeof(size_t) + sizeof(HMEMORYMANAGER*) + sizeof(IClassMemory**));
            }

            return 0;
        }

        virtual void Delete(void* ptr)
        {
            unsigned char* pTmp = (unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(HMEMORYMANAGER*);

            if (*(void**)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(HMEMORYMANAGER*)) != DefMemMgr())
            {
                char* p = (char*)((intptr_t)THD_DEL_SIG);
                *p = 'a';
            }

            //void** check_data = memory_unit_check_data(pTmp);
            void** check_data = memory_unit_get_info(pTmp);

            //if (*check_data == unit)
            if (memory_unit_info_unit(check_data) == unit)
            {
                T* obj = (T*)ptr;
                obj->~T();
                *(IClassMemory**)(pTmp + sizeof(HMEMORYMANAGER*)) = (IClassMemory*)((intptr_t)REP_DEL_SIG);
                memory_unit_quick_free(unit, check_data);
            }
            else
            {
                size_t size = *(size_t*)(pTmp - sizeof(size_t));

                T* obj = (T*)ptr;

                while (size)
                {
                    obj->~T();
                    ++obj;
                    size--;
                }

                *(IClassMemory**)(pTmp + sizeof(HMEMORYMANAGER*)) = (IClassMemory*)((intptr_t)REP_DEL_SIG);
                memory_manager_free(DefMemMgr(), pTmp - sizeof(size_t));
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
            unit = create_memory_unit(sizeof(HMEMORYMANAGER*) + sizeof(IClassMemory**) + sizeof(T));
        }

        ~CClassMemory<T, true>(void)
        {
            destroy_memory_unit(unit);
        }

        T* New(size_t size)
        {
            if (size == 1)
            {
                void* ptr = memory_unit_alloc(unit);
                *(HMEMORYMANAGER*)ptr = DefMemMgr();
                *(IClassMemory**)((unsigned char*)ptr + sizeof(HMEMORYMANAGER*)) = this;
                return (T*)((unsigned char*)ptr + sizeof(HMEMORYMANAGER*) + sizeof(IClassMemory**));
            }
            else if (size > 1)
            {
                void* ptr = memory_manager_alloc(DefMemMgr(), sizeof(size_t) + sizeof(HMEMORYMANAGER*) + sizeof(IClassMemory**) + sizeof(T)*size);
                *(size_t*)ptr = size;
                *(HMEMORYMANAGER*)((unsigned char*)ptr + sizeof(size_t)) = DefMemMgr();
                *(IClassMemory**)((unsigned char*)ptr + sizeof(size_t) + sizeof(HMEMORYMANAGER*)) = this;
                return (T*)((unsigned char*)ptr + sizeof(size_t) + sizeof(HMEMORYMANAGER*) + sizeof(IClassMemory**));
            }

            return 0;
        }

        virtual void Delete(void* ptr)
        {
            unsigned char* pTmp = (unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(HMEMORYMANAGER*);

            if (*(void**)((unsigned char*)ptr - sizeof(IClassMemory**) - sizeof(HMEMORYMANAGER*)) != DefMemMgr())
            {
                char* p = (char*)((intptr_t)THD_DEL_SIG);
                *p = 'a';
            }

            //void** check_data = memory_unit_check_data(pTmp);
            void** check_data = memory_unit_get_info(pTmp);

            //if (memory_unit_check(unit, pTmp))
            //if (*check_data == unit)
            if (memory_unit_info_unit(check_data) == unit)
            {
                *(IClassMemory**)(pTmp + sizeof(HMEMORYMANAGER*)) = (IClassMemory*)((intptr_t)REP_DEL_SIG);
                memory_unit_quick_free(unit, check_data);
            }
            else
            {
                *(IClassMemory**)(pTmp + sizeof(HMEMORYMANAGER*)) = (IClassMemory*)((intptr_t)REP_DEL_SIG);
                memory_manager_free(DefMemMgr(), pTmp - sizeof(size_t));
            }
        }
    };

    template <typename T>
    inline CClassMemory<T>& get_class_memory(void)
    {
        static thread_local CClassMemory<T> class_memory;
        return class_memory;
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
        T* obj = get_class_memory<T>().New(size, std::forward<Args>(args)...);
        trace_alloc(typeid(T).name(), file, line, obj, size * sizeof(T));

        return obj;
    }

    extern void TraceDelete(void* ptr);

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
            return (pointer)S_MALLOC(sizeof(value_type)*num);
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