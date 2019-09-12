#include <string.h>
#include "./lib_svr_common_def.h"
#include "../include/loop_cache.h"

loop_cache* create_loop_cache(void* cache, size_t size)
{
    loop_cache* lc = 0;

    if (!size)
    {
        return 0;
    }

    if (cache)
    {
        lc = (loop_cache*)malloc(sizeof(loop_cache));
        lc->cache_begin = (char*)cache;
    }
    else
    {
        char* ptr = (char*)malloc(sizeof(loop_cache) + size);
        lc = (loop_cache*)ptr;
        lc->cache_begin = ptr + sizeof(loop_cache);
    }

    lc->head = lc->cache_begin;
    lc->tail = lc->cache_begin;
    lc->cache_end = lc->cache_begin + size;
    lc->size = size;

    return lc;
}

void destroy_loop_cache(loop_cache* cache)
{
    if (cache)
    {
        free(cache);
    }
}

bool loop_cache_push_data(loop_cache* cache, const void* data, size_t size)
{
    size_t dist = cache->tail + cache->size - cache->head;
    size_t used = dist >= cache->size ? (dist - cache->size) : dist;

    if (size + used + 1 > cache->size)
        return false;

    if (cache->tail + size >= cache->cache_end)
    {
        size_t seg_1 = cache->cache_end - cache->tail;
        size_t seg_2 = size - seg_1;

        memcpy(cache->tail, data, seg_1);
        memcpy(cache->cache_begin, (char*)data + seg_1, seg_2);
        cache->tail = cache->cache_begin + seg_2;
    }
    else
    {
        memcpy(cache->tail, data, size);
        cache->tail += size;
    }

    return true;
}

bool loop_cache_pop_data(loop_cache* cache, void* data, size_t size)
{
    size_t dist = cache->tail + cache->size - cache->head;
    size_t used = dist >= cache->size ? (dist - cache->size) : dist;

    if (size > used)
        return false;

    if (cache->head + size >= cache->cache_end)
    {
        size_t seg_1 = cache->cache_end - cache->head;
        size_t seg_2 = size - seg_1;

        memcpy(data, cache->head, seg_1);
        memcpy((char*)data + seg_1, cache->cache_begin, seg_2);
        cache->head = cache->cache_begin + seg_2;
    }
    else
    {
        memcpy(data, cache->head, size);
        cache->head += size;
    }

    return true;
}

bool loop_cache_copy_data(loop_cache* cache, void* data, size_t size)
{
    size_t dist = cache->tail + cache->size - cache->head;
    size_t used = dist >= cache->size ? (dist - cache->size) : dist;

    if (size > used)
        return false;

    if (cache->head + size >= cache->cache_end)
    {
        size_t seg_1 = cache->cache_end - cache->head;
        size_t seg_2 = size - seg_1;

        memcpy(data, cache->head, seg_1);
        memcpy((char*)data + seg_1, cache->cache_begin, seg_2);
    }
    else
    {
        memcpy(data, cache->head, size);
    }

    return true;
}

bool loop_cache_push(loop_cache* cache, size_t size)
{
    size_t dist = cache->tail + cache->size - cache->head;
    size_t used = dist >= cache->size ? (dist - cache->size) : dist;

    if (size + used + 1 > cache->size)
    {
        return false;
    }

    if (cache->tail + size >= cache->cache_end)
    {
        size_t seg_1 = cache->cache_end - cache->tail;
        size_t seg_2 = size - seg_1;
        cache->tail = cache->cache_begin + seg_2;
    }
    else
    {
        cache->tail += size;
    }

    return true;
}

bool loop_cache_pop(loop_cache* cache, size_t size)
{
    size_t dist = cache->tail + cache->size - cache->head;
    size_t used = dist >= cache->size ? (dist - cache->size) : dist;

    if (size > used)
        return false;

    if (cache->head + size >= cache->cache_end)
    {
        size_t seg_1 = cache->cache_end - cache->head;
        size_t seg_2 = size - seg_1;
        cache->head = cache->cache_begin + seg_2;
    }
    else
    {
        cache->head += size;
    }

    return true;
}

void loop_cache_get_free(loop_cache* cache, void** cache_ptr, size_t* cache_len)
{
    size_t dist = cache->tail + cache->size - cache->head;
    size_t used = dist >= cache->size ? (dist - cache->size) : dist;

    if (*cache_len)
    {
        if ((*cache_len) > (cache->size - used - 1))
        {
            *cache_len = cache->size - used - 1;
        }
    }
    else
    {
        *cache_len = cache->size - used - 1;
    }

    if (cache->tail + (*cache_len) >= cache->cache_end)
    {
        *cache_len = cache->cache_end - cache->tail;
    }

    *cache_ptr = cache->tail;
}

void loop_cache_get_data(loop_cache* cache, void** cache_ptr, size_t* cache_len)
{
    size_t dist = cache->tail + cache->size - cache->head;
    size_t used = dist >= cache->size ? (dist - cache->size) : dist;

    if (*cache_len)
    {
        if ((*cache_len) > used)
        {
            *cache_len = used;
        }
    }
    else
    {
        *cache_len = used;
    }

    if (cache->head + (*cache_len) >= cache->cache_end)
    {
        *cache_len = cache->cache_end - cache->head;
    }

    *cache_ptr = cache->head;
}

size_t loop_cache_free_size(loop_cache* cache)
{
    size_t dist = cache->tail + cache->size - cache->head;
    size_t used = dist >= cache->size ? (dist - cache->size) : dist;

    return cache->size - used - 1;
}

size_t loop_cache_data_size(loop_cache* cache)
{
    size_t dist = cache->tail + cache->size - cache->head;
    size_t used = dist >= cache->size ? (dist - cache->size) : dist;

    return used;
}

size_t loop_cache_size(loop_cache* cache)
{
    return cache->size;
}

void* loop_cache_get_cache(loop_cache* cache)
{
    return cache->cache_begin;
}

void loop_cache_reset(loop_cache* cache, size_t size, void* data)
{
    //cache->cache_begin = (char*)data;

    //if (cache->alloc_cache)
    //{
    //    free(cache->alloc_cache);
    //    cache->alloc_cache = 0;
    //}


    //cache->head = cache->cache_begin;
    //cache->tail = cache->cache_begin;
    //cache->cache_end = cache->cache_begin + size;
    //cache->size = size;

    cache->cache_begin = (char*)data;
    cache->head = cache->cache_begin;
    cache->tail = cache->cache_begin;
    cache->cache_end = cache->cache_begin + size;
    cache->size = size;
}

void loop_cache_reinit(loop_cache* cache)
{
    cache->head = cache->cache_begin;
    cache->tail = cache->cache_begin;
}
