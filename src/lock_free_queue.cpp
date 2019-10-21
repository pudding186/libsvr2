#include "./lib_svr_common_def.h"
#include "../include/lock_free_queue.hpp"

typedef struct st_lock_free_ptr_queue
{
    LockFreeLoopQueue<void*, true, true>* queue;
}lock_free_ptr_queue;

lock_free_ptr_queue* create_lock_free_ptr_queue(size_t queue_size)
{
    lock_free_ptr_queue* ptr_queue = (lock_free_ptr_queue*)malloc(sizeof(lock_free_ptr_queue));
    ptr_queue->queue = new LockFreeLoopQueue<void *, true, true>(queue_size);
    return ptr_queue;
}

void destroy_lock_free_ptr_queue(lock_free_ptr_queue* ptr_queue)
{
    if (ptr_queue)
    {
        delete ptr_queue->queue;
        free(ptr_queue);
    }
}

bool lock_free_ptr_queue_push(lock_free_ptr_queue* ptr_queue, void* ptr)
{
    return ptr_queue->queue->push(ptr);
}

void* lock_free_ptr_queue_pop(lock_free_ptr_queue* ptr_queue)
{
    void* ptr = 0;
    ptr_queue->queue->pop(ptr);
    return ptr;
}
