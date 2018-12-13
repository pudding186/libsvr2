#pragma once
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern HLOOPCACHE(create_loop_cache)(void* cache_data, size_t size);

extern void (destroy_loop_cache)(HLOOPCACHE cache);

extern bool (loop_cache_push_data)(HLOOPCACHE cache, const void* data, size_t size);

extern bool (loop_cache_pop_data)(HLOOPCACHE cache, void* data, size_t size);

extern bool (loop_cache_copy_data)(HLOOPCACHE cache, void* data, size_t size);

extern bool (loop_cache_push)(HLOOPCACHE cache, size_t size);

extern bool (loop_cache_pop)(HLOOPCACHE cache, size_t size);

extern void (loop_cache_get_free)(HLOOPCACHE cache, void** cache_ptr, size_t* cache_len);

extern void (loop_cache_get_data)(HLOOPCACHE cache, void** cache_ptr, size_t* cache_len);

extern size_t(loop_cache_free_size)(HLOOPCACHE cache);

extern size_t(loop_cache_data_size)(HLOOPCACHE cache);

extern size_t(loop_cache_size)(HLOOPCACHE cache);

extern void* (loop_cache_get_cache)(HLOOPCACHE cache);

extern void (loop_cache_reset)(HLOOPCACHE cache, size_t size, void* data);

extern void (loop_cache_reinit)(HLOOPCACHE cache);

extern HLOOPPTRQUEUE (create_loop_ptr_queue)(size_t size);

extern void (destroy_loop_ptr_queue)(HLOOPPTRQUEUE queue);

extern bool (loop_ptr_queue_push)(HLOOPPTRQUEUE queue, void* ptr);

extern void* (loop_ptr_queue_pop)(HLOOPPTRQUEUE queue);

#ifdef  __cplusplus
}
#endif