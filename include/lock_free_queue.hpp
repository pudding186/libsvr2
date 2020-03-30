#pragma once
#include "./lib_svr_def.h"
#ifdef __cplusplus
#include <atomic>
#include <cstddef>
#include <new>

template <typename T, bool multi_productor = true, bool multi_consumer = true>
class LockFreeLoopQueue
{
public:

};

template <typename T>
class LockFreeLoopQueue<T, true, true>
{
public:
    static inline size_t _roundup_power_of_2(size_t buffer_size)
    {
        size_t result = buffer_size - 1;
        for (size_t i = 1; i <= sizeof(void*) * 4; i <<= 1)
        {
            result |= result >> i;
        }
        return result + 1;
    }

    LockFreeLoopQueue(size_t queue_size) :
        _capacity(_roundup_power_of_2(queue_size))
    {
        _capacityMask = _capacity - 1;
        _queue = new Node[_capacity];
        for (size_t i = 0; i < _capacity; ++i)
        {
            _queue[i].tail.store(i, std::memory_order_relaxed);
            _queue[i].head.store((size_t)(-1), std::memory_order_relaxed);
        }

        _tail.store(0, std::memory_order_relaxed);
        _head.store(0, std::memory_order_relaxed);
    }

    ~LockFreeLoopQueue()
    {
        delete[] _queue;
    }

    size_t capacity() const { return _capacity; }

    size_t size() const
    {
        size_t head = _head.load(std::memory_order_acquire);
        return _tail.load(std::memory_order_relaxed) - head;
    }

    bool push(const T& data)
    {
        Node* node;
        size_t tail = _tail.load(std::memory_order_relaxed);
        for (;;)
        {
            node = &_queue[tail & _capacityMask];
            if (node->tail.load(std::memory_order_relaxed) != tail)
            {
                return false;
            }

            if ((_tail.compare_exchange_weak(tail, tail + 1, std::memory_order_relaxed)))
            {
                break;
            }
        }
        //node->data = std::move(data);
        new (&node->data)T(data);
        node->head.store(tail, std::memory_order_release);
        return true;
    }

    bool pop(T& result)
    {
        Node* node;
        size_t head = _head.load(std::memory_order_relaxed);
        for (;;)
        {
            node = &_queue[head & _capacityMask];
            if (node->head.load(std::memory_order_relaxed) != head)
            {
                return false;
            }

            if (_head.compare_exchange_weak(head, head + 1, std::memory_order_relaxed))
            {
                break;
            }
        }
        //result = std::move(node->data);
        result = node->data;
        (&node->data)->~T();
        node->tail.store(head + _capacity, std::memory_order_release);
        return true;
    }


private:
    struct Node
    {
        T data;
        std::atomic<size_t> tail;
        std::atomic<size_t> head;
    };

private:
    size_t _capacityMask;
    Node* _queue;
    size_t _capacity;
    char cacheLinePad1[64];
    std::atomic<size_t> _tail;
    char cacheLinePad2[64];
    std::atomic<size_t> _head;
    char cacheLinePad3[64];
};

template <typename T>
class LockFreeLoopQueue<T, true, false>
{
public:
    static inline size_t _roundup_power_of_2(size_t buffer_size)
    {
        size_t result = buffer_size - 1;
        for (size_t i = 1; i <= sizeof(void*) * 4; i <<= 1)
        {
            result |= result >> i;
        }

        return result + 1;
    }

    LockFreeLoopQueue(size_t queue_size) :
        _capacity(_roundup_power_of_2(queue_size))
    {
        _capacityMask = _capacity - 1;
        _queue = new Node[_capacity];
        for (size_t i = 0; i < _capacity; ++i)
        {
            _queue[i].tail.store(i, std::memory_order_relaxed);
            _queue[i].head.store((size_t)-1, std::memory_order_relaxed);
        }

        _tail.store(0, std::memory_order_relaxed);
        _head.store(0, std::memory_order_relaxed);
    }

    ~LockFreeLoopQueue()
    {
        delete[] _queue;
    }

    size_t capacity() const { return _capacity; }

    size_t size() const
    {
        size_t head = _head.load(std::memory_order_acquire);
        return _tail.load(std::memory_order_relaxed) - head;
    }

    bool push(const T& data)
    {
        Node* node;
        size_t tail = _tail.load(std::memory_order_relaxed);
        for (;;)
        {
            node = &_queue[tail & _capacityMask];
            if (node->tail.load(std::memory_order_relaxed) != tail)
            {
                return false;
            }

            if ((_tail.compare_exchange_weak(tail, tail + 1, std::memory_order_relaxed)))
            {
                break;
            }
        }
        //node->data = std::move(data);
        new (&node->data)T(data);
        node->head.store(tail, std::memory_order_release);
        return true;
    }

    bool pop(T& result)
    {
        Node* node;
        size_t head = _head.load(std::memory_order_relaxed);

        node = &_queue[head & _capacityMask];

        if (node->head.load(std::memory_order_relaxed) != head)
        {
            return false;
        }

        _head.store(head + 1, std::memory_order_relaxed);

        if (node->head.load(std::memory_order_relaxed) != head)
        {
            return false;
        }

        //result = std::move(node->data);
        result = node->data;
        (&node->data)->~T();
        node->tail.store(head + _capacity, std::memory_order_release);
        return true;
    }


private:
    struct Node
    {
        T data;
        std::atomic<size_t> tail;
        std::atomic<size_t> head;
    };

private:
    size_t _capacityMask;
    Node* _queue;
    size_t _capacity;
    char cacheLinePad1[64];
    std::atomic<size_t> _tail;
    char cacheLinePad2[64];
    std::atomic<size_t> _head;
    char cacheLinePad3[64];
};

template <typename T>
class LockFreeLoopQueue<T, false, true>
{
public:
    static inline size_t _roundup_power_of_2(size_t buffer_size)
    {
        size_t result = buffer_size - 1;
        for (size_t i = 1; i <= sizeof(void*) * 4; i <<= 1)
        {
            result |= result >> i;
        }
        return result + 1;
    }

    LockFreeLoopQueue(size_t queue_size) :
        _capacity(_roundup_power_of_2(queue_size))
    {
        _capacityMask = _capacity - 1;
        _queue = new Node[_capacity];
        for (size_t i = 0; i < _capacity; ++i)
        {
            _queue[i].tail.store(i, std::memory_order_relaxed);
            _queue[i].head.store(-1, std::memory_order_relaxed);
        }

        _tail.store(0, std::memory_order_relaxed);
        _head.store(0, std::memory_order_relaxed);
    }

    ~LockFreeLoopQueue()
    {
        delete[] _queue;
    }

    size_t capacity() const { return _capacity; }

    size_t size() const
    {
        size_t head = _head.load(std::memory_order_acquire);
        return _tail.load(std::memory_order_relaxed) - head;
    }

    bool push(const T& data)
    {
        Node* node;
        size_t tail = _tail.load(std::memory_order_relaxed);
        node = &_queue[tail & _capacityMask];
        if (node->tail.load(std::memory_order_relaxed) != tail)
        {
            return false;
        }
        _tail.store(tail + 1, std::memory_order_relaxed);

        //node->data = std::move(data);
        new (&node->data)T(data);
        node->head.store(tail, std::memory_order_release);
        return true;
    }

    bool pop(T& result)
    {
        Node* node;
        size_t head = _head.load(std::memory_order_relaxed);
        for (;;)
        {
            node = &_queue[head & _capacityMask];
            if (node->head.load(std::memory_order_relaxed) != head)
            {
                return false;
            }

            if (_head.compare_exchange_weak(head, head + 1, std::memory_order_relaxed))
            {
                break;
            }
        }
        //result = std::move(node->data);
        result = node->data;
        (&node->data)->~T();
        node->tail.store(head + _capacity, std::memory_order_release);
        return true;
    }


private:
    struct Node
    {
        T data;
        std::atomic<size_t> tail;
        std::atomic<size_t> head;
    };

private:
    size_t _capacityMask;
    Node* _queue;
    size_t _capacity;
    char cacheLinePad1[64];
    std::atomic<size_t> _tail;
    char cacheLinePad2[64];
    std::atomic<size_t> _head;
    char cacheLinePad3[64];
};

template <typename T>
class LockFreeLoopQueue<T, false, false>
{
public:
    static inline size_t _roundup_power_of_2(size_t buffer_size)
    {
        size_t result = buffer_size - 1;
        for (size_t i = 1; i <= sizeof(void*) * 4; i <<= 1)
        {
            result |= result >> i;
        }
        return result + 1;
    }

    size_t _increment(size_t idx)const { return ((idx + 1)&_capacityMask); }

    LockFreeLoopQueue(size_t queue_size) :
        _capacity(_roundup_power_of_2(queue_size))
    {
        _capacityMask = _capacity - 1;
        _queue = new Node[_capacity];

        _tail.store(0, std::memory_order_relaxed);
        _head.store(0, std::memory_order_relaxed);
    }

    ~LockFreeLoopQueue()
    {
        delete[] _queue;
    }

    size_t capacity() const { return _capacity; }

    size_t size() const
    {
        size_t head = _head.load(std::memory_order_acquire);
        return _tail.load(std::memory_order_relaxed) - head;
    }

    bool push(const T& data)
    {
        const auto current_tail = _tail.load(std::memory_order_relaxed);
        const auto next_tail = _increment(current_tail);
        if (next_tail != _head.load(std::memory_order_acquire))
        {
            //_queue[current_tail].data = std::move(data);
            new (&_queue[current_tail].data)T(data);
            _tail.store(next_tail, std::memory_order_release);
            return true;
        }

        return false; // full queue
    }

    bool pop(T& result)
    {
        const auto current_head = _head.load(std::memory_order_relaxed);
        if (current_head == _tail.load(std::memory_order_acquire))
            return false; // empty queue
        //result = std::move(_queue[current_head].data);
        result = _queue[current_head].data;
        (&_queue[current_head].data)->~T();
        _head.store(_increment(current_head), std::memory_order_release);
        return true;
    }


private:
    struct Node
    {
        T data;
    };

private:
    size_t _capacityMask;
    Node* _queue;
    size_t _capacity;
    char cacheLinePad1[64];
    std::atomic<size_t> _tail;
    char cacheLinePad2[64];
    std::atomic<size_t> _head;
    char cacheLinePad3[64];
};

extern "C" {
#endif

    HLOCKFREEPTRQUEUE (create_lock_free_ptr_queue)(size_t queue_size);
    void (destroy_lock_free_ptr_queue)(HLOCKFREEPTRQUEUE ptr_queue);
    bool (lock_free_ptr_queue_push)(HLOCKFREEPTRQUEUE ptr_queue, void* ptr);
    void* (lock_free_ptr_queue_pop)(HLOCKFREEPTRQUEUE ptr_queue);

#ifdef __cplusplus
}
#endif