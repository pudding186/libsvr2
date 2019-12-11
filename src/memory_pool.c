#include <string.h>
#include <stdlib.h>
#include "./lib_svr_common_def.h"
#include "../include/memory_pool.h"
#include "../include/avl_tree.h"

TLS_VAR HMEMORYMANAGER lib_svr_mem_mgr = 0;

mem_block* _create_memory_block(mem_unit* unit, size_t unit_count)
{
    unsigned char* ptr;
    size_t i;
    mem_block* block;
    size_t block_size = sizeof(mem_block) + unit_count * (sizeof(void*) + unit->unit_size);

    block = (mem_block*)malloc(block_size);

    //block->next = unit->block_head;
    //unit->block_head = block;

    do 
    {
        block->next = unit->block_head;
    } while (InterlockedCompareExchangePointer(
        &unit->block_head, 
        block, 
        block->next) != block->next);

    ptr = (unsigned char*)block + sizeof(mem_block);

    for (i = 0; i < unit_count - 1; i++)
    {
        *((void**)ptr) = ptr + sizeof(void*) + unit->unit_size;
        ptr += sizeof(void*) + unit->unit_size;
    }

    *((void**)ptr) = unit->unit_free_head;
    unit->unit_free_head = (unsigned char*)block + sizeof(mem_block);

    return block;
}

mem_block* _create_memory_block_mt(mem_unit* unit, size_t unit_count)
{
    unsigned char* ptr;
    size_t i;
    mem_block* block;
    size_t block_size = sizeof(mem_block) + unit_count * (sizeof(void*) + unit->unit_size);

    block = (mem_block*)malloc(block_size);

    do
    {
        block->next = unit->block_head;
    } while (InterlockedCompareExchangePointer(
        &unit->block_head,
        block,
        block->next) != block->next);

    ptr = (unsigned char*)block + sizeof(mem_block);

    for (i = 0; i < unit_count - 1; i++)
    {
        *((void**)ptr) = ptr + sizeof(void*) + unit->unit_size;
        ptr += sizeof(void*) + unit->unit_size;
    }

    tag_pointer tp_next;
    tp_next.u_data.tp.ptr = (unsigned char*)block + sizeof(mem_block);

    //tag_pointer tp = unit->unit_free_head_mt;
    tag_pointer tp;
    tp.u_data.tp.ptr = (void*)0xFFFFFFFFFFFFFFFF;
    tp.u_data.tp.tag = 0;

    while (!InterlockedCompareExchange128(
        unit->unit_free_head_mt.u_data.bit128.Int,
        tp_next.u_data.bit128.Int[1],
        tp_next.u_data.bit128.Int[0],
        tp.u_data.bit128.Int))
    {
        *((void**)ptr) = tp.u_data.tp.ptr;
        tp_next.u_data.tp.tag = tp.u_data.tp.tag + 1;
    }

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

    unit->unit_free_head_mt.u_data.tp.ptr = 0;
    unit->unit_free_head_mt.u_data.tp.tag = 0;
    unit->unit_create_thread = &lib_svr_mem_mgr;
    unit->block_head = 0;
    unit->unit_free_head = 0;
    unit->unit_size = unit_size;
    
    memory_unit_set_grow_bytes(unit, 4 * 1024);

    return unit;
}

void destroy_memory_unit(mem_unit* unit)
{
    while (unit->block_head)
    {
        mem_block* block = unit->block_head;
        unit->block_head = block->next;

        free(block);
    }

    free(unit);
}

void* memory_unit_alloc(HMEMORYUNIT unit)
{
    tag_pointer alloc_tp;

    if (unit->unit_create_thread == &lib_svr_mem_mgr)
    {
        if (!unit->unit_free_head)
        {
            if (!unit->grow_count)
            {
                return 0;
            }

            _create_memory_block(unit, unit->grow_count);
        }

        alloc_tp.u_data.tp.ptr = unit->unit_free_head;
        unit->unit_free_head = *(void**)alloc_tp.u_data.tp.ptr;
    }
    else
    {
        tag_pointer alloc_pt_next;

        if (!unit->unit_free_head_mt.u_data.tp.ptr)
        {
            if (!unit->grow_count)
            {
                return 0;
            }

            _create_memory_block_mt(unit, unit->grow_count);
        }

        alloc_tp.u_data.tp.ptr = (void*)0xFFFFFFFFFFFFFFFF;
        alloc_tp.u_data.tp.tag = 0;

        while (!InterlockedCompareExchange128(
            unit->unit_free_head_mt.u_data.bit128.Int,
            alloc_pt_next.u_data.bit128.Int[1],
            alloc_pt_next.u_data.bit128.Int[0],
            alloc_tp.u_data.bit128.Int))
        {
            while (!alloc_tp.u_data.tp.ptr)
            {
                _create_memory_block_mt(unit, unit->grow_count);
                alloc_tp = unit->unit_free_head_mt;
            }

            alloc_pt_next.u_data.tp.ptr = *(void**)alloc_tp.u_data.tp.ptr;
            alloc_pt_next.u_data.tp.tag = alloc_tp.u_data.tp.tag + 1;
        }

        //alloc_tp = unit->unit_free_head_mt;

        //do 
        //{
        //    while (!alloc_tp.u_data.tp.ptr)
        //    {
        //        _create_memory_block_mt(unit, unit->grow_count);
        //        alloc_tp = unit->unit_free_head_mt;
        //    }

        //    alloc_pt_next.u_data.tp.ptr = *(void**)alloc_tp.u_data.tp.ptr;
        //    alloc_pt_next.u_data.tp.tag = alloc_tp.u_data.tp.tag + 1;

        //} while (!InterlockedCompareExchange128(
        //    unit->unit_free_head_mt.u_data.bit128.Int,
        //    alloc_pt_next.u_data.bit128.Int[1],
        //    alloc_pt_next.u_data.bit128.Int[0],
        //    alloc_tp.u_data.bit128.Int));
    }

    *(void**)alloc_tp.u_data.tp.ptr = unit;

    return (unsigned char*)alloc_tp.u_data.tp.ptr + sizeof(void*);
}

void memory_unit_free(mem_unit* unit, void* mem)
{
    void** ptr_mem_unit = memory_unit_check_data(mem);

    if ((*ptr_mem_unit) != unit)
    {
        printf("0x%p != 0x%p\n", (*ptr_mem_unit), unit);
        CRUSH_CODE();
        return;
    }

    if (unit->unit_create_thread == &lib_svr_mem_mgr)
    {
        *ptr_mem_unit = unit->unit_free_head;
        unit->unit_free_head = ptr_mem_unit;
    }
    else
    {
        tag_pointer tp_next;
        tp_next.u_data.tp.ptr = ptr_mem_unit;

        tag_pointer tp;

        tp.u_data.tp.ptr = (void*)0xFFFFFFFFFFFFFFFF;
        tp.u_data.tp.tag = 0;

        while (!InterlockedCompareExchange128(
            unit->unit_free_head_mt.u_data.bit128.Int,
            tp_next.u_data.bit128.Int[1],
            tp_next.u_data.bit128.Int[0],
            tp.u_data.bit128.Int))
        {
            *ptr_mem_unit = tp.u_data.tp.ptr;
            tp_next.u_data.tp.tag = tp.u_data.tp.tag + 1;
        }

        //do 
        //{
        //    *ptr_mem_unit = tp.u_data.tp.ptr;
        //    
        //    tp_next.u_data.tp.tag = tp.u_data.tp.tag + 1;
        //} while (!InterlockedCompareExchange128(
        //    unit->unit_free_head_mt.u_data.bit128.Int,
        //    tp_next.u_data.bit128.Int[1],
        //    tp_next.u_data.bit128.Int[0],
        //    tp.u_data.bit128.Int));
    }
}

void memory_unit_quick_free(mem_unit* unit, void** ptr_mem_unit)
{
    if (unit->unit_create_thread == &lib_svr_mem_mgr)
    {
        *ptr_mem_unit = unit->unit_free_head;
        unit->unit_free_head = ptr_mem_unit;
    }
    else
    {
        tag_pointer tp_next;
        tp_next.u_data.tp.ptr = ptr_mem_unit;

        tag_pointer tp;

        tp.u_data.tp.ptr = (void*)0xFFFFFFFFFFFFFFFF;
        tp.u_data.tp.tag = 0;

        while (!InterlockedCompareExchange128(
            unit->unit_free_head_mt.u_data.bit128.Int,
            tp_next.u_data.bit128.Int[1],
            tp_next.u_data.bit128.Int[0],
            tp.u_data.bit128.Int))
        {
            *ptr_mem_unit = tp.u_data.tp.ptr;
            tp_next.u_data.tp.tag = tp.u_data.tp.tag + 1;
        }
    }
}

void** memory_unit_check_data(void* mem)
{
    return (void**)((unsigned char*)mem - sizeof(void*));
}

bool memory_unit_check(mem_unit* unit, void* mem)
{
    if (*(mem_unit**)memory_unit_check_data(mem) == unit)
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
        unsigned char* mem = (unsigned char*)malloc(sizeof(void*) + mem_size);
        *(void**)mem = pool;
        return mem + sizeof(void*);
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
        pool->units[i] = create_memory_unit(pool->min_mem_size + i * pool->align);

        memory_unit_set_grow_bytes(pool->units[i], pool->grow);

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

    void** check_data = memory_unit_check_data(old_mem);

    if ((*check_data) != pool)
    {
        mem_unit* unit = *(mem_unit**)check_data;

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
        unsigned char* new_mem_ptr = (unsigned char*)realloc((unsigned char*)old_mem - sizeof(void*), mem_size + sizeof(void*));

        if (new_mem_ptr)
        {
            return new_mem_ptr + sizeof(void*);
        }

        return 0;
    }

}

void memory_pool_free(mem_pool* pool, void* mem)
{
    void** check_data = memory_unit_check_data(mem);

    if (*(mem_pool**)check_data == pool)
    {
        free(check_data);
    }
    else if (*(mem_unit**)check_data == pool->units[
        ((*(mem_unit**)check_data)->unit_size - pool->min_mem_size) >> pool->shift])
    {
        memory_unit_quick_free(*(mem_unit**)check_data, check_data);
    }
    else
    {
        CRUSH_CODE();
    }
}

bool memory_pool_check(mem_pool* pool, void* mem)
{
    void** check_data = memory_unit_check_data(mem);

    if (*(mem_unit**)check_data ==
        pool->units[((*(mem_unit**)check_data)->unit_size - pool->min_mem_size) >> pool->shift])
    {
        return true;
    }

    if (*(mem_pool**)check_data == pool)
    {
        return true;
    }

    return false;
}

mem_mgr* create_memory_manager(size_t align, size_t start_size, size_t max_size, size_t grow_size, size_t grow_power)
{
    size_t last_start_size = sizeof(size_t);

    mem_mgr* mgr = (mem_mgr*)malloc(sizeof(mem_mgr));

    mgr->mem_pool_map.root = 0;
    mgr->mem_pool_map.size = 0;
    mgr->mem_pool_map.head = 0;
    mgr->mem_pool_map.tail = 0;

    mgr->mem_pool_map.key_cmp = 0;
    mgr->mem_pool_map.tree_unit = 0;
    mgr->mem_pool_map.node_unit = create_memory_unit(sizeof(avl_node));
    memory_unit_set_grow_count(mgr->mem_pool_map.node_unit, 32);

    avl_node* tmp_node = memory_unit_alloc(mgr->mem_pool_map.node_unit);
    memory_unit_free(mgr->mem_pool_map.node_unit, tmp_node);

    while (start_size <= max_size)
    {
        HMEMORYPOOL pool = create_memory_pool(align, last_start_size, start_size, grow_size);

        avl_tree_insert_integer(&mgr->mem_pool_map, pool->max_mem_size, pool);
        align *= grow_power;
        last_start_size = start_size + 1;
        start_size *= grow_power;
    }

    return mgr;
}

void destroy_memory_manager(mem_mgr* mgr)
{
    HAVLNODE pool_node = avl_first(&mgr->mem_pool_map);
    while (pool_node)
    {
        destroy_memory_pool((HMEMORYPOOL)avl_node_value_user(pool_node));
        pool_node = avl_next(pool_node);
    }
    destroy_memory_unit(mgr->mem_pool_map.node_unit);
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

void* memory_manager_alloc(mem_mgr* mgr, size_t mem_size)
{
    HAVLNODE pool_node = avl_tree_find_integer_nearby(&mgr->mem_pool_map, mem_size);

    if ((size_t)avl_node_key_integer(pool_node) < mem_size)
    {
        if (avl_next(pool_node))
        {
            pool_node = avl_next(pool_node);
        }
    }

    return memory_pool_alloc((HMEMORYPOOL)avl_node_value_user(pool_node), mem_size);
}

void* memory_manager_realloc(mem_mgr* mgr, void* old_mem, size_t mem_size)
{
    HMEMORYUNIT unit = 0;
    void* new_mem = 0;
    HAVLNODE old_node = 0;
    HAVLNODE new_node = 0;

    if (!old_mem)
    {
        return memory_manager_alloc(mgr, mem_size);
    }

    unit = *memory_unit_check_data(old_mem);

    if (unit == avl_node_value_user(avl_last(&mgr->mem_pool_map)))
    {
        return memory_pool_realloc((HMEMORYPOOL)avl_node_value_user(avl_last(&mgr->mem_pool_map)), old_mem, mem_size);
    }

    if (mem_size <= unit->unit_size)
    {
        return old_mem;
    }

    old_node = avl_tree_find_integer_nearby(&mgr->mem_pool_map, unit->unit_size);
    if (avl_node_key_integer(old_node) < unit->unit_size)
    {
        if (avl_next(old_node))
        {
            old_node = avl_next(old_node);
        }
    }

    new_node = avl_tree_find_integer_nearby(&mgr->mem_pool_map, mem_size);
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

void memory_manager_free(mem_mgr* mgr, void* mem)
{
    HMEMORYUNIT unit = 0;
    HAVLNODE pool_node = 0;
    HMEMORYPOOL pool = 0;

    if (!mem)
        return;
    void** check_data = memory_unit_check_data(mem);
    unit = *check_data;

    if (unit == avl_node_value_user(avl_last(&mgr->mem_pool_map)))
    {
        memory_pool_free((HMEMORYPOOL)avl_node_value_user(avl_last(&mgr->mem_pool_map)), mem);
        return;
    }

    pool_node = avl_tree_find_integer_nearby(&mgr->mem_pool_map, unit->unit_size);
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

bool memory_manager_check(mem_mgr* mgr, void* mem)
{
    HMEMORYUNIT unit = 0;
    HAVLNODE pool_node = 0;
    HMEMORYPOOL pool = 0;

    unit = *memory_unit_check_data(mem);

    if (unit == avl_node_value_user(avl_last(&mgr->mem_pool_map)))
    {
        return true;
    }

    pool_node = avl_tree_find_integer_nearby(&mgr->mem_pool_map, unit->unit_size);
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


void* libsvr_memory_manager_realloc(void* old_mem, size_t mem_size)
{
    return memory_manager_realloc(lib_svr_mem_mgr, old_mem, mem_size);
}

void* libsvr_memory_manager_alloc(size_t mem_size)
{
    return memory_manager_alloc(lib_svr_mem_mgr, mem_size);
}

void libsvr_memory_manager_free(void* mem)
{
    memory_manager_free(lib_svr_mem_mgr, mem);
}

bool libsvr_memory_manager_check(void* mem)
{
    return memory_manager_check(lib_svr_mem_mgr, mem);
}