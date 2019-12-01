#pragma once
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern HMEMORYUNIT(create_memory_unit)(size_t unit_size);
extern void (destroy_memory_unit)(HMEMORYUNIT unit);
extern void* (memory_unit_alloc)(HMEMORYUNIT unit);
extern void (memory_unit_set_grow_bytes)(HMEMORYUNIT unit, size_t grow_bytes);
extern void (memory_unit_set_grow_count)(HMEMORYUNIT unit, size_t grow_count);
extern void (memory_unit_free)(HMEMORYUNIT unit, void* mem);
extern void (memory_unit_quick_free)(HMEMORYUNIT unit, void** ptr_mem_unit);
extern bool (memory_unit_check)(HMEMORYUNIT unit, void* mem);
extern void** (memory_unit_check_data)(void* mem);

extern HMEMORYPOOL(create_memory_pool)(size_t align, size_t min_mem_size, size_t max_mem_size, size_t grow_size);
extern void (destroy_memory_pool)(HMEMORYPOOL pool);
extern void* (memory_pool_alloc)(HMEMORYPOOL pool, size_t mem_size);
extern void* (memory_pool_realloc)(HMEMORYPOOL pool, void* old_mem, size_t mem_size);
extern void (memory_pool_free)(HMEMORYPOOL pool, void* mem);
extern bool (memory_pool_check)(HMEMORYPOOL pool, void* mem);

extern HMEMORYMANAGER(create_memory_manager)(size_t align, size_t start_size, size_t max_size, size_t grow_size, size_t grow_power);
extern void (destroy_memory_manager)(HMEMORYMANAGER mgr);
extern void* (memory_manager_alloc)(HMEMORYMANAGER mgr, size_t mem_size);
extern void* (memory_manager_realloc)(HMEMORYMANAGER mgr, void* old_mem, size_t mem_size);
extern void (memory_manager_free)(HMEMORYMANAGER mgr, void* mem);
extern bool (memory_manager_check)(HMEMORYMANAGER mgr, void* mem);


#ifdef  __cplusplus
}
#endif