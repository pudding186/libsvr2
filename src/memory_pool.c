#include <string.h>
#include <stdlib.h>
#include "./lib_svr_common_def.h"
#include "../include/memory_pool.h"
#include "../include/avl_tree.h"

#define MAIN_THREAD_ALLOC  1
#define MULTI_THREAD_ALLOC 2

typedef union u_sign_data
{
    unsigned long long  ull;
    HMEMORYUNIT         unit;
}sign_data;

//TLS_VAR HMEMORYMANAGER lib_svr_mem_mgr = 0;

TLS_VAR int create_thread_sign = 0;

void** memory_unit_get_sign(void* mem)
{
    return (void**)((unsigned char*)mem - sizeof(void*));
}

HMEMORYUNIT memory_unit_sign_to_unit(void** info)
{
    unsigned long long magic_unit = (unsigned long long)(*info);
    magic_unit &= ~((unsigned long long)1 << 63);
    return (HMEMORYUNIT)(magic_unit);
}

size_t memory_unit_alloc_count(mem_unit* unit)
{
    return unit->alloc_count +
        unit->unit_free_head_cache.u_data.tp.tag[0] -
        unit->unit_free_head_cache.u_data.tp.tag[1] +
        unit->unit_free_head_mt.u_data.tp.tag[0] -
        unit->unit_free_head_mt.u_data.tp.tag[1];
}

//size_t memory_unit_total_count(mem_unit* unit)
//{
//    return unit->total_count + 
//        unit->mem_block_head_mt.u_data.tp.tag[0] - 
//        unit->mem_block_head_mt.u_data.tp.tag[1];
//}

size_t memory_unit_alloc_size(mem_unit* unit)
{
    return memory_unit_alloc_count(unit) * (sizeof(void*) + unit->unit_size);
}

size_t memory_unit_total_size(mem_unit* unit)
{
    return unit->blocks_size + unit->mt_blocks_size;
}

bool memory_unit_in_create_thread(mem_unit* unit)
{
    return (unit->unit_create_thread == &create_thread_sign);
}

int memory_unit_check_sign(mem_unit* unit, void** info)
{
    if (*info == unit)
    {
        return MAIN_THREAD_ALLOC;
    }
    else 
    {

        sign_data data;

        data.unit = unit;
        data.ull |= ((unsigned long long)1 << 63);

        if (*info == data.unit)
        {
            return MULTI_THREAD_ALLOC;
        }
    }

    return 0;
}

bool _tidy_memory_block(mem_unit* unit, mem_block* block, void** head, void** tail)
{
    unsigned char* ptr = (unsigned char*)block + sizeof(mem_block);

    bool can_free = true;

    while (ptr < (unsigned char*)block->end)
    {
        if ((*((void**)ptr)) == unit)
        {
            can_free = false;
        }
        else
        {
            if ((*tail))
            {
                *((void**)(*tail)) = ptr;
                *tail = ptr;
            }
            else
            {
                *tail = ptr;
                *head = ptr;
            }
        }

        ptr += sizeof(void*) + unit->unit_size;
    }

    return can_free;
}

void _tidy_memory_unit(mem_unit* unit)
{
    unit->unit_free_head = 0;

    for (mem_block** cur = &(unit->mem_block_head); *cur;)
    {
        mem_block* entry = *cur;
        void* head = 0;
        void* tail = 0;

        if (_tidy_memory_block(unit, entry, &head, &tail))
        {
            *cur = entry->next;

            size_t block_size = (unsigned char*)entry->end - (unsigned char*)entry;
            free(entry);

            unit->blocks_size -= block_size;
        }
        else
        {
            cur = &entry->next;
            if (head)
            {
                *((void**)tail) = unit->unit_free_head;
                unit->unit_free_head = head;
            }
        }
    }
}

void memory_unit_tidy(mem_unit* unit)
{
    if (unit->unit_create_thread == &create_thread_sign)
    {
        _tidy_memory_unit(unit);
    }
    else
    {

    }
}

mem_block* _create_memory_block(mem_unit* unit, size_t unit_count)
{
    unsigned char* ptr;
    size_t i;
    mem_block* block;
    size_t block_size = sizeof(mem_block) + unit_count * (sizeof(void*) + unit->unit_size);

    block = (mem_block*)malloc(block_size);

    block->next = unit->mem_block_head;
    unit->mem_block_head = block;

    ptr = (unsigned char*)block + sizeof(mem_block);

    for (i = 0; i < unit_count - 1; i++)
    {
        *((void**)ptr) = ptr + sizeof(void*) + unit->unit_size;
        ptr += sizeof(void*) + unit->unit_size;
    }

    block->end = ptr + sizeof(void*) + unit->unit_size;

    *((void**)ptr) = unit->unit_free_head;
    unit->unit_free_head = (unsigned char*)block + sizeof(mem_block);

    //unit->total_count += unit_count;
    unit->blocks_size += block_size;

    return block;
}

mem_block* _create_memory_block_mt(mem_unit* unit, size_t unit_count)
{
    unsigned char* ptr;
    size_t i;
    mem_block* block;
    size_t block_size = sizeof(mem_block) + unit_count * (sizeof(void*) + unit->unit_size);

    block = (mem_block*)malloc(block_size);

    tag_pointer tp_next;
    tp_next.u_data.tp.ptr = block;

    tag_pointer tp;
    tp.u_data.tp.ptr = (void*)ULLONG_MAX;

#ifdef _MSC_VER
    while (!InterlockedCompareExchange128(
        unit->mem_block_head_mt.u_data.bit128.Int,
        tp_next.u_data.bit128.Int[1],
        tp_next.u_data.bit128.Int[0],
        tp.u_data.bit128.Int))
#elif __GNUC__

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored"-Watomic-alignment"
#endif

    while (!__atomic_compare_exchange(
        &unit->mem_block_head_mt.u_data.bit128,
        &tp.u_data.bit128,
        &tp_next.u_data.bit128,
        false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#else
#error "unknown compiler"
#endif
    {
        block->next = tp.u_data.tp.ptr;

        tp_next.u_data.tp.tag[0] = tp.u_data.tp.tag[0];
        tp_next.u_data.tp.tag[1] = tp.u_data.tp.tag[1] + (unsigned int)unit_count;
    }

    ptr = (unsigned char*)block + sizeof(mem_block);

    for (i = 0; i < unit_count - 1; i++)
    {
        *((void**)ptr) = ptr + sizeof(void*) + unit->unit_size;
        ptr += sizeof(void*) + unit->unit_size;
    }

    block->end = ptr + sizeof(void*) + unit->unit_size;

    tp_next.u_data.tp.ptr = (unsigned char*)block + sizeof(mem_block);

    tp.u_data.tp.ptr = (void*)ULLONG_MAX;

#ifdef _MSC_VER
    while (!InterlockedCompareExchange128(
        unit->unit_free_head_mt.u_data.bit128.Int,
        tp_next.u_data.bit128.Int[1],
        tp_next.u_data.bit128.Int[0],
        tp.u_data.bit128.Int))
#elif __GNUC__

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored"-Watomic-alignment"
#endif

    while (!__atomic_compare_exchange(
        &unit->unit_free_head_mt.u_data.bit128,
        &tp.u_data.bit128,
        &tp_next.u_data.bit128,
        false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#else
#error "unknown compiler"
#endif
    {
        *((void**)ptr) = tp.u_data.tp.ptr;
        tp_next.u_data.tp.tag[0] = tp.u_data.tp.tag[0] + 1;
        tp_next.u_data.tp.tag[1] = tp.u_data.tp.tag[1] + 1;
    }

#ifdef _MSC_VER
    InterlockedAdd64((LONG64*)(&unit->mt_blocks_size), block_size);
#elif __GNUC__
    __atomic_add_fetch(&unit->mt_blocks_size, block_size, __ATOMIC_SEQ_CST);
#else
#error "unknown compiler"
#endif

    return block;
}

void memory_unit_set_grow_bytes(HMEMORYUNIT unit, size_t grow_bytes)
{
    if (grow_bytes)
    {
        size_t real_unit_size = sizeof(void*) + unit->unit_size;

        if (grow_bytes > (sizeof(mem_block) + real_unit_size))
        {
            unit->grow_count = (grow_bytes - sizeof(mem_block)) / real_unit_size;
        }
        else
        {
            unit->grow_count = 1;
        }
    }
    else
    {
        unit->grow_count = 0;
    }
}

void memory_unit_set_grow_count(HMEMORYUNIT unit, size_t grow_count)
{
    unit->grow_count = grow_count;
}

mem_unit* create_memory_unit(size_t unit_size)
{
    mem_unit* unit = (mem_unit*)malloc(sizeof(mem_unit));
#ifdef _MSC_VER
    unit->unit_free_head_mt.u_data.bit128.Int[0] = 0;
    unit->unit_free_head_mt.u_data.bit128.Int[1] = 0;
    unit->unit_free_head_cache.u_data.bit128.Int[0] = 0;
    unit->unit_free_head_cache.u_data.bit128.Int[1] = 0;
    unit->mem_block_head_mt.u_data.bit128.Int[0] = 0;
    unit->mem_block_head_mt.u_data.bit128.Int[1] = 0;
#elif __GNUC__
    unit->unit_free_head_mt.u_data.bit128 = 0;
    unit->unit_free_head_cache.u_data.bit128 = 0;
    unit->mem_block_head_mt.u_data.bit128 = 0;
#else
#error "unknown compiler"
#endif
    unit->unit_create_thread = &create_thread_sign;
    unit->mem_block_head = 0;
    unit->unit_free_head = 0;
    unit->unit_size = unit_size;
    unit->alloc_count = 0;
    //unit->total_count = 0;
    unit->blocks_size = sizeof(mem_unit);
    unit->mt_blocks_size = 0;

    
    memory_unit_set_grow_bytes(unit, 4 * 1024);

    return unit;
}

void destroy_memory_unit(mem_unit* unit)
{
    while (unit->mem_block_head)
    {
        mem_block* block = unit->mem_block_head;
        unit->mem_block_head = block->next;

        free(block);
    }

    while (unit->mem_block_head_mt.u_data.tp.ptr)
    {
        mem_block* block = (mem_block*)unit->mem_block_head_mt.u_data.tp.ptr;
        unit->mem_block_head_mt.u_data.tp.ptr = block->next;

        free(block);
    }

    free(unit);
}


void _check_mutli_free_cache(mem_unit* unit)
{
    tag_pointer tp_next;
    tag_pointer tp;

    tp.u_data.tp.ptr = (void*)ULLONG_MAX;

    tp_next.u_data.tp.ptr = 0;

#ifdef _MSC_VER
    while (!InterlockedCompareExchange128(
        unit->unit_free_head_cache.u_data.bit128.Int,
        tp_next.u_data.bit128.Int[1],
        tp_next.u_data.bit128.Int[0],
        tp.u_data.bit128.Int))
#elif __GNUC__

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored"-Watomic-alignment"
#endif

    while (!__atomic_compare_exchange(
        &unit->unit_free_head_cache.u_data.bit128,
        &tp.u_data.bit128,
        &tp_next.u_data.bit128,
        false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#else
#error "unknown compiler"
#endif
    {
        tp_next.u_data.tp.tag[0] = tp.u_data.tp.tag[1] + 1;
        tp_next.u_data.tp.tag[1] = tp.u_data.tp.tag[1] + 1;
    }

    unit->unit_free_head = tp.u_data.tp.ptr;
    unit->alloc_count = unit->alloc_count - (tp.u_data.tp.tag[1] - tp.u_data.tp.tag[0]);
}

void* _main_thread_alloc(mem_unit* unit)
{
    if (!unit->unit_free_head)
    {
        if (!unit->grow_count)
        {
            return 0;
        }

        _check_mutli_free_cache(unit);

        if (!unit->unit_free_head)
        {
            _create_memory_block(unit, unit->grow_count);
        }
    }

    void* ptr = unit->unit_free_head;
    unit->unit_free_head = *(void**)ptr;

    *(void**)ptr = unit;

    unit->alloc_count++;

    return (unsigned char*)ptr + sizeof(void*);
}

void* _multi_thread_alloc(mem_unit* unit)
{
    tag_pointer alloc_tp;

    tag_pointer alloc_pt_next;

    if (!unit->unit_free_head_mt.u_data.tp.ptr)
    {
        if (!unit->grow_count)
        {
            return 0;
        }
    }

    alloc_tp.u_data.tp.ptr = (void*)ULLONG_MAX;

#ifdef _MSC_VER
    while (!InterlockedCompareExchange128(
        unit->unit_free_head_mt.u_data.bit128.Int,
        alloc_pt_next.u_data.bit128.Int[1],
        alloc_pt_next.u_data.bit128.Int[0],
        alloc_tp.u_data.bit128.Int))
#elif __GNUC__

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored"-Watomic-alignment"
#endif

    while (!__atomic_compare_exchange(
        &unit->unit_free_head_mt.u_data.bit128,
        &alloc_tp.u_data.bit128,
        &alloc_pt_next.u_data.bit128,
        false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#else
#error "unknown compiler"
#endif
    {
        if (!alloc_tp.u_data.tp.ptr)
        {
            alloc_tp.u_data.tp.ptr = (void*)ULLONG_MAX;

            _create_memory_block_mt(unit, unit->grow_count);
            continue;
        }

        alloc_pt_next.u_data.tp.ptr = *(void**)alloc_tp.u_data.tp.ptr;
        alloc_pt_next.u_data.tp.tag[0] = alloc_tp.u_data.tp.tag[0] + 1;
        alloc_pt_next.u_data.tp.tag[1] = alloc_tp.u_data.tp.tag[1];
    }

    sign_data data;

    data.unit = unit;
    data.ull |= ((unsigned long long)1 << 63);

    *(void**)alloc_tp.u_data.tp.ptr = data.unit;

    return (unsigned char*)alloc_tp.u_data.tp.ptr + sizeof(void*);
}

void* memory_unit_alloc(HMEMORYUNIT unit)
{
    if (unit->unit_create_thread == &create_thread_sign)
    {
        return _main_thread_alloc(unit);
    }
    else
    {
        return _multi_thread_alloc(unit);
    }
}

void _main_thread_free(mem_unit* unit, void** ptr_mem_unit)
{
    *ptr_mem_unit = unit->unit_free_head;
    unit->unit_free_head = ptr_mem_unit;

    unit->alloc_count--;
}

void _multi_thread_free(mem_unit* unit, void** ptr_mem_unit)
{
    tag_pointer tp_next;
    tp_next.u_data.tp.ptr = ptr_mem_unit;

    tag_pointer tp;

    tp.u_data.tp.ptr = (void*)ULLONG_MAX;

#ifdef _MSC_VER
    while (!InterlockedCompareExchange128(
        unit->unit_free_head_mt.u_data.bit128.Int,
        tp_next.u_data.bit128.Int[1],
        tp_next.u_data.bit128.Int[0],
        tp.u_data.bit128.Int))
#elif __GNUC__

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored"-Watomic-alignment"
#endif

    while (!__atomic_compare_exchange(
        &unit->unit_free_head_mt.u_data.bit128,
        &tp.u_data.bit128,
        &tp_next.u_data.bit128,
        false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#else
#error "unknown compiler"
#endif

    {
        *ptr_mem_unit = tp.u_data.tp.ptr;
        tp_next.u_data.tp.tag[0] = tp.u_data.tp.tag[0];
        tp_next.u_data.tp.tag[1] = tp.u_data.tp.tag[1] + 1;
    }
}

void _multi_thread_free_cache(mem_unit* unit, void** ptr_mem_unit)
{
    tag_pointer tp_next;
    tp_next.u_data.tp.ptr = ptr_mem_unit;

    tag_pointer tp;

    tp.u_data.tp.ptr = (void*)ULLONG_MAX;

#ifdef _MSC_VER
    while (!InterlockedCompareExchange128(
        unit->unit_free_head_cache.u_data.bit128.Int,
        tp_next.u_data.bit128.Int[1],
        tp_next.u_data.bit128.Int[0],
        tp.u_data.bit128.Int))
#elif __GNUC__

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored"-Watomic-alignment"
#endif

    while (!__atomic_compare_exchange(
        &unit->unit_free_head_cache.u_data.bit128,
        &tp.u_data.bit128,
        &tp_next.u_data.bit128,
        false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#else
#error "unknown compiler"
#endif
    {
        *ptr_mem_unit = tp.u_data.tp.ptr;
        tp_next.u_data.tp.tag[0] = tp.u_data.tp.tag[0];
        tp_next.u_data.tp.tag[1] = tp.u_data.tp.tag[1] + 1;
    }
}

void memory_unit_quick_free(mem_unit* unit, void** ptr_mem_unit)
{
    int alloc_thread = memory_unit_check_sign(unit, ptr_mem_unit);
    if (unit->unit_create_thread == &create_thread_sign)
    {
        if (alloc_thread == MAIN_THREAD_ALLOC)
        {
            _main_thread_free(unit, ptr_mem_unit);
        }
        else if (alloc_thread == MULTI_THREAD_ALLOC)
        {
            _multi_thread_free(unit, ptr_mem_unit);
        }
        else
        {
            CRUSH_CODE();
        }
    }
    else
    {
        if (alloc_thread == MAIN_THREAD_ALLOC)
        {
            _multi_thread_free_cache(unit, ptr_mem_unit);
        }
        else if (alloc_thread == MULTI_THREAD_ALLOC)
        {
            _multi_thread_free(unit, ptr_mem_unit);
        }
        else
        {
            CRUSH_CODE();
        }
    }
}

void memory_unit_free(mem_unit* unit, void* mem)
{
    memory_unit_quick_free(unit, memory_unit_get_sign(mem));
}

bool memory_unit_check(mem_unit* unit, void* mem)
{
    if (memory_unit_check_sign(unit, memory_unit_get_sign(mem)))
    {
        return true;
    }

    return false;
}

mem_pool* create_memory_pool(size_t align, size_t min_mem_size, size_t max_mem_size, size_t grow_size)
{
    size_t mod = 0;
    size_t k = align;
    mem_pool* pool = (mem_pool*)malloc(sizeof(mem_pool));

    if (min_mem_size < sizeof(size_t))
    {
        min_mem_size = sizeof(size_t);
    }

    if (max_mem_size < min_mem_size)
    {
        return 0;
    }

    if (align)
    {
        mod = align % sizeof(size_t);
        if (mod)
        {
            pool->align = align + sizeof(size_t) - mod;
        }
        else
            pool->align = align;
    }
    else
        return 0;

    mod = max_mem_size % pool->align;
    if (mod)
    {
        pool->max_mem_size = max_mem_size + pool->align - mod;
    }
    else
        pool->max_mem_size = max_mem_size;

    mod = min_mem_size % pool->align;
    if (mod)
    {
        pool->min_mem_size = min_mem_size + pool->align - mod;
    }
    else
        pool->min_mem_size = min_mem_size;

    pool->shift = 0;
    for (; !(k & 1); k >>= 1, ++pool->shift);             // mod MAX_MEM_ALIGN align

    pool->unit_size = (pool->max_mem_size - pool->min_mem_size) / pool->align + 1;

    pool->units = (mem_unit**)malloc(pool->unit_size * sizeof(mem_unit*));

    for (k = 0; k < pool->unit_size; k++)
    {
        pool->units[k] = 0;
    }

    pool->grow = 4 * 1024;
    if (grow_size > pool->grow)
    {
        pool->grow = grow_size;
    }

    pool->system_alloc_size = 0;
    pool->system_free_size = 0;
    pool->mt_system_alloc_size = 0;
    pool->mt_system_free_size = 0;
    pool->pool_create_thread = &create_thread_sign;

    return pool;
}

void destroy_memory_pool(mem_pool* pool)
{
    size_t i;

    for (i = 0; i < pool->unit_size; ++i)
    {
        if (pool->units[i])
        {
            destroy_memory_unit(pool->units[i]);
        }
    }

    free(pool->units);

    free(pool);
}

void* memory_pool_alloc(mem_pool* pool, size_t mem_size)
{
    size_t i;

    if (!mem_size)
    {
        return 0;
    }

    if (mem_size > pool->max_mem_size)
    {
        unsigned char* mem = (unsigned char*)malloc(sizeof(size_t) + sizeof(void*) + mem_size);
        *(size_t*)mem = mem_size + sizeof(size_t) + sizeof(void*);
        *(void**)(mem + sizeof(size_t)) = pool;

        if (pool->pool_create_thread == &create_thread_sign)
        {
            pool->system_alloc_size += sizeof(size_t) + sizeof(void*) + mem_size;
        }
        else
        {
#ifdef _MSC_VER
            InterlockedAdd64((LONG64*)(&pool->mt_system_alloc_size), sizeof(size_t) + sizeof(void*) + mem_size);
#elif __GNUC__
            __atomic_add_fetch(&pool->mt_system_alloc_size, sizeof(size_t) + sizeof(void*) + mem_size, __ATOMIC_SEQ_CST);
#else
#error "unknown compiler"
#endif
        }

        return mem + sizeof(size_t) + sizeof(void*);
    }

    if (mem_size <= pool->min_mem_size)
    {
        i = 0;
    }
    else
    {
        mem_size -= pool->min_mem_size;

        i = (mem_size & (pool->align - 1)) ?
            (mem_size >> pool->shift) + 1 : (mem_size >> pool->shift);
    }

    if (pool->units[i])
    {
        return memory_unit_alloc(pool->units[i]);
    }
    else
    {
        HMEMORYUNIT zero_unit = 0;

        HMEMORYUNIT unit = create_memory_unit(pool->min_mem_size + i * pool->align);
        unit->unit_create_thread = pool->pool_create_thread;
        memory_unit_set_grow_bytes(unit, pool->grow);

#ifdef _MSC_VER
        void* volatile *ptr_unit = (void* volatile*)&(pool->units[i]);
        if (InterlockedCompareExchangePointer(ptr_unit, unit, zero_unit))
#elif __GNUC__
        if (!__atomic_compare_exchange(&(pool->units[i]), &zero_unit, &unit,
            false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#else
#error "unknown compiler"
#endif
        {
            destroy_memory_unit(unit);
        }
        return memory_unit_alloc(pool->units[i]);
    }
}

void* memory_pool_realloc(mem_pool* pool, void* old_mem, size_t mem_size)
{
    void* new_mem = 0;

    if (!old_mem)
    {
        return memory_pool_alloc(pool, mem_size);
    }

    void** check_data = memory_unit_get_sign(old_mem);

    if ((*check_data) != pool)
    {
        mem_unit* unit = memory_unit_sign_to_unit(check_data);

        if (unit == pool->units[(unit->unit_size - pool->min_mem_size) >> pool->shift])
        {
            if (unit->unit_size >= mem_size)
                return old_mem;

            new_mem = memory_pool_alloc(pool, mem_size);

            memcpy(new_mem, old_mem, unit->unit_size);

            memory_unit_quick_free(unit, check_data);

            return new_mem;
        }

        CRUSH_CODE();
        return 0;
    }
    else
    {
        size_t old_mem_size = *(size_t*)((unsigned char*)old_mem - sizeof(void*) - sizeof(size_t));
        unsigned char* new_mem_ptr = (unsigned char*)realloc((unsigned char*)old_mem - sizeof(void*) - sizeof(size_t), mem_size + sizeof(size_t) + sizeof(void*));

        if (new_mem_ptr)
        {
            if (pool->pool_create_thread == &create_thread_sign)
            {
                pool->system_free_size += old_mem_size;
                pool->system_alloc_size += mem_size + sizeof(void*) + sizeof(size_t);
            }
            else
            {
#ifdef _MSC_VER
                InterlockedAdd64((LONG64*)(&pool->mt_system_free_size), (LONG64)old_mem_size);
                InterlockedAdd64((LONG64*)(&pool->mt_system_alloc_size), mem_size + sizeof(void*) + sizeof(size_t));
#elif __GNUC__
                __atomic_add_fetch(&pool->mt_system_free_size, old_mem_size, __ATOMIC_SEQ_CST);
                __atomic_add_fetch(&pool->mt_system_alloc_size, mem_size + sizeof(void*) + sizeof(size_t), __ATOMIC_SEQ_CST);
#else
#error "unknown compiler"
#endif
            }

            *(size_t*)new_mem_ptr = mem_size + sizeof(size_t) + sizeof(void*);
            return new_mem_ptr + sizeof(size_t) + sizeof(void*);
        }

        return 0;
    }

}

void memory_pool_free(mem_pool* pool, void* mem)
{
    void** check_data = memory_unit_get_sign(mem);
    mem_unit* unit = memory_unit_sign_to_unit(check_data);

    if (*(mem_pool**)check_data == pool)
    {
        size_t mem_size = *(size_t*)((unsigned char*)mem - sizeof(void*) - sizeof(size_t));

        if (pool->pool_create_thread == &create_thread_sign)
        {
            pool->system_free_size += mem_size;
        }
        else
        {
#ifdef _MSC_VER
            InterlockedAdd64((LONG64*)(&pool->mt_system_free_size), (LONG64)(mem_size));
#elif __GNUC__
            __atomic_add_fetch(&pool->mt_system_free_size, mem_size, __ATOMIC_SEQ_CST);
#else
#error "unknown compiler"
#endif
        }

        free((unsigned char*)mem - sizeof(void*) - sizeof(size_t));
    }
    else if (unit == pool->units[
        (unit->unit_size - pool->min_mem_size) >> pool->shift])
    {
        memory_unit_quick_free(unit, check_data);
    }
    else
    {
        CRUSH_CODE();
    }
}

bool memory_pool_check(mem_pool* pool, void* mem)
{
    void** check_data = memory_unit_get_sign(mem);
    mem_unit* unit = memory_unit_sign_to_unit(check_data);

    if (unit ==
        pool->units[(unit->unit_size - pool->min_mem_size) >> pool->shift])
    {
        return true;
    }

    if (*(mem_pool**)check_data == pool)
    {
        return true;
    }

    return false;
}

size_t memory_pool_alloc_size(mem_pool* pool)
{
    size_t alloc_size = 0;
    size_t i;

    for (i = 0; i < pool->unit_size; ++i)
    {
        mem_unit* unit = pool->units[i];

        if (unit)
        {
            alloc_size += memory_unit_alloc_size(unit);
        }
    }

    alloc_size += (pool->system_alloc_size - pool->system_free_size + pool->mt_system_alloc_size - pool->mt_system_free_size);

    return alloc_size;
}

size_t memory_pool_total_size(mem_pool* pool)
{
    size_t total_size = sizeof(mem_pool) + pool->unit_size * sizeof(mem_unit*);
    size_t i;

    for (i = 0; i < pool->unit_size; ++i)
    {
        mem_unit* unit = pool->units[i];

        if (unit)
        {
            total_size += memory_unit_total_size(unit);
        }
    }

    total_size += (pool->system_alloc_size - pool->system_free_size + pool->mt_system_alloc_size - pool->mt_system_free_size);

    return total_size;
}

void memory_pool_tidy(mem_pool* pool)
{
    size_t i;
    for (i = 0; i < pool->unit_size; ++i)
    {
        mem_unit* unit = pool->units[i];

        if (unit)
        {
            memory_unit_tidy(unit);
        }
    }
}

HMEMORYMANAGER create_memory_manager(size_t align, size_t start_size, size_t max_size, size_t grow_size, size_t grow_power)
{
    size_t last_start_size = sizeof(size_t);

    HAVLTREE mgr = (HAVLTREE)malloc(sizeof_avl_tree());

    mgr->root = 0;
    mgr->size = 0;
    mgr->head = 0;
    mgr->tail = 0;

    mgr->key_cmp = 0;
    mgr->tree_unit = 0;
    mgr->node_unit = create_memory_unit(sizeof(avl_node));
    memory_unit_set_grow_count(mgr->node_unit, 32);

    while (start_size <= max_size)
    {
        HMEMORYPOOL pool = create_memory_pool(align, last_start_size, start_size, grow_size);

        avl_tree_insert_integer(mgr, pool->max_mem_size, pool);
        align *= grow_power;
        last_start_size = start_size + 1;
        start_size *= grow_power;
    }

    return mgr;
}

void destroy_memory_manager(HMEMORYMANAGER mgr)
{
    HAVLNODE pool_node = avl_first(mgr);
    while (pool_node)
    {
        destroy_memory_pool((HMEMORYPOOL)avl_node_value_user(pool_node));
        pool_node = avl_next(pool_node);
    }
    destroy_memory_unit(mgr->node_unit);
    free(mgr);
}

HAVLNODE avl_tree_find_integer_nearby(HAVLTREE tree, size_t key)
{
    HAVLNODE node = tree->root;
    HAVLNODE nearby_node = node;

    while (node)
    {
        nearby_node = node;

        if (key < node->key.v_integer)
        {
            node = node->avl_child[0];
        }
        else if (key > node->key.v_integer)
        {
            node = node->avl_child[1];
        }
        else
            return node;
    }

    return nearby_node;
}

void* memory_manager_alloc(HMEMORYMANAGER mgr, size_t mem_size)
{
    HAVLNODE pool_node = avl_tree_find_integer_nearby(mgr, mem_size);

    if ((size_t)avl_node_key_integer(pool_node) < mem_size)
    {
        if (avl_next(pool_node))
        {
            pool_node = avl_next(pool_node);
        }
    }

    return memory_pool_alloc((HMEMORYPOOL)avl_node_value_user(pool_node), mem_size);
}

void* memory_manager_realloc(HMEMORYMANAGER mgr, void* old_mem, size_t mem_size)
{
    HMEMORYUNIT unit = 0;
    void* new_mem = 0;
    HAVLNODE old_node = 0;
    HAVLNODE new_node = 0;

    if (!old_mem)
    {
        return memory_manager_alloc(mgr, mem_size);
    }

    void** check_data = memory_unit_get_sign(old_mem);

    unit = memory_unit_sign_to_unit(check_data);

    if ((*check_data) == avl_node_value_user(avl_last(mgr)))
    {
        return memory_pool_realloc((HMEMORYPOOL)(*check_data), old_mem, mem_size);
    }

    if (mem_size <= unit->unit_size)
    {
        return old_mem;
    }

    old_node = avl_tree_find_integer_nearby(mgr, unit->unit_size);
    if (avl_node_key_integer(old_node) < unit->unit_size)
    {
        if (avl_next(old_node))
        {
            old_node = avl_next(old_node);
        }
    }

    new_node = avl_tree_find_integer_nearby(mgr, mem_size);
    if (avl_node_key_integer(new_node) < mem_size)
    {
        if (avl_next(new_node))
        {
            new_node = avl_next(new_node);
        }
    }

    if (old_node == new_node)
    {
        return memory_pool_realloc((HMEMORYPOOL)avl_node_value_user(old_node), old_mem, mem_size);
    }

    new_mem = memory_pool_alloc((HMEMORYPOOL)avl_node_value_user(new_node), mem_size);
    memcpy(new_mem, old_mem, unit->unit_size);
    memory_pool_free((HMEMORYPOOL)avl_node_value_user(old_node), old_mem);
    return new_mem;
}

void memory_manager_free(HMEMORYMANAGER mgr, void* mem)
{
    HMEMORYUNIT unit = 0;
    HAVLNODE pool_node = 0;
    HMEMORYPOOL pool = 0;

    if (!mem)
        return;

    void** check_data = memory_unit_get_sign(mem);
    unit = memory_unit_sign_to_unit(check_data);

    if ((*check_data) == avl_node_value_user(avl_last(mgr)))
    {
        memory_pool_free((HMEMORYPOOL)(*check_data), mem);
        return;
    }

    pool_node = avl_tree_find_integer_nearby(mgr, unit->unit_size);
    if (avl_node_key_integer(pool_node) < unit->unit_size)
    {
        if (avl_next(pool_node))
        {
            pool_node = avl_next(pool_node);
        }
    }

    if (!pool_node)
    {
        CRUSH_CODE();
    }

    pool = (HMEMORYPOOL)avl_node_value_user(pool_node);

    if (pool->units[(unit->unit_size - pool->min_mem_size) >> pool->shift] == unit)
    {
        memory_unit_quick_free(unit, check_data);
    }
    else
    {
        CRUSH_CODE();
    }
}

bool memory_manager_check(HMEMORYMANAGER mgr, void* mem)
{
    HMEMORYUNIT unit = 0;
    HAVLNODE pool_node = 0;
    HMEMORYPOOL pool = 0;

    void** check_data = memory_unit_get_sign(mem);
    unit = memory_unit_sign_to_unit(check_data);

    if ((*check_data) == avl_node_value_user(avl_last(mgr)))
    {
        return true;
    }

    pool_node = avl_tree_find_integer_nearby(mgr, unit->unit_size);
    if (avl_node_key_integer(pool_node) < unit->unit_size)
    {
        if (avl_next(pool_node))
        {
            pool_node = avl_next(pool_node);
        }
    }

    if (!pool_node)
    {
        CRUSH_CODE();
    }

    pool = (HMEMORYPOOL)avl_node_value_user(pool_node);

    if (pool->units[(unit->unit_size - pool->min_mem_size) >> pool->shift] == unit)
    {
        return true;
    }

    return false;
}

size_t memory_manager_alloc_size(HMEMORYMANAGER mgr)
{
    size_t alloc_size = 0;

    HAVLNODE pool_node = avl_first(mgr);
    while (pool_node)
    {
        alloc_size += memory_pool_alloc_size((HMEMORYPOOL)avl_node_value_user(pool_node));
        pool_node = avl_next(pool_node);
    }

    return alloc_size;
}

size_t(memory_manager_total_size)(HMEMORYMANAGER mgr)
{
    size_t total_size = sizeof_avl_tree() + avl_tree_size(mgr) * sizeof_avl_node();

    HAVLNODE pool_node = avl_first(mgr);
    while (pool_node)
    {
        total_size += memory_pool_total_size((HMEMORYPOOL)avl_node_value_user(pool_node));
        pool_node = avl_next(pool_node);
    }

    return total_size;
}

void memory_manager_tidy(HMEMORYMANAGER mgr)
{
    HAVLNODE pool_node = avl_first(mgr);
    while (pool_node)
    {
        memory_pool_tidy((HMEMORYPOOL)avl_node_value_user(pool_node));
        pool_node = avl_next(pool_node);
    }
}
