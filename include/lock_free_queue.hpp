#pragma once
#include <atomic>
#include <cstddef>

template <typename T, bool multi_productor = true, bool multi_consumer = true>
class LockFreeLoopQueue
{

};

template <typename T>
class LockFreeLoopQueue<T, true, true>
{
public:
    LockFreeLoopQueue(size_t queue_size) :
        max_size_(_roundup_poser_of_2(queue_size)),
        buffer_(new cell_t[_roundup_poser_of_2(queue_size)]),
        buffer_mask_(_roundup_poser_of_2(queue_size) - 1)
    {
        for (size_t i = 0; i != queue_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~LockFreeLoopQueue()
    {
        delete[] buffer_;
    }

    void reinit(void)
    {
        size_t buffer_size = buffer_mask_ + 1;
        assert((buffer_size >= 2) &&
            ((buffer_size & (buffer_size - 1)) == 0));
        for (size_t i = 0; i != buffer_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    bool push(T&& data)
    {
        cell_t* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0)
            {
                if (enqueue_pos_.compare_exchange_weak
                (pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (dif < 0)
                return false;
            else
                pos = enqueue_pos_.load(std::memory_order_relaxed);
        }
        cell->data_ = std::move(data);
        cell->sequence_.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& data)
    {
        cell_t* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq =
                cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0)
            {
                if (dequeue_pos_.compare_exchange_weak
                (pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (dif < 0)
                return false;
            else
                pos = dequeue_pos_.load(std::memory_order_relaxed);
        }
        data = std::move(cell->data_);
        cell->sequence_.store
        (pos + buffer_mask_ + 1, std::memory_order_release);
        return true;
    }

    size_t approx_size() const
    {
        size_t first_pos = dequeue_pos_.load(std::memory_order_relaxed);
        size_t last_pos = enqueue_pos_.load(std::memory_order_relaxed);
        if (last_pos <= first_pos)
            return 0;
        auto size = last_pos - first_pos;
        return size < max_size_ ? size : max_size_;
    }

    size_t capacity() const
    {
        return max_size_;
    }
private:

    static inline size_t _roundup_poser_of_2(size_t queue_size)
    {
        if (queue_size == 0)
        {
            return 0;
        }

        size_t position = 0;
        for (size_t i = queue_size; i != 0; i >>= 1)
        {
            position++;
        }

        return static_cast<size_t>((size_t)1 << position);
    }

    struct cell_t
    {
        std::atomic<size_t>   sequence_;
        T                     data_;
    };

    size_t const            max_size_;
    static size_t const     cacheline_size = 64;
    typedef char            cacheline_pad_t[cacheline_size];
    cacheline_pad_t         pad0_;
    cell_t* const           buffer_;
    size_t const            buffer_mask_;
    cacheline_pad_t         pad1_;
    std::atomic<size_t>     enqueue_pos_;
    cacheline_pad_t         pad2_;
    std::atomic<size_t>     dequeue_pos_;
    cacheline_pad_t         pad3_;
    LockFreeLoopQueue(LockFreeLoopQueue const&);
    void operator = (LockFreeLoopQueue const&);
};

template <typename T>
class LockFreeLoopQueue<T, true, false>
{
public:
    LockFreeLoopQueue(size_t queue_size) :
        max_size_(_roundup_poser_of_2(queue_size)),
        buffer_(new cell_t[_roundup_poser_of_2(queue_size)]),
        buffer_mask_(_roundup_poser_of_2(queue_size) - 1)
    {
        for (size_t i = 0; i != queue_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~LockFreeLoopQueue()
    {
        delete[] buffer_;
    }

    void reinit(void)
    {
        size_t buffer_size = buffer_mask_ + 1;
        assert((buffer_size >= 2) &&
            ((buffer_size & (buffer_size - 1)) == 0));
        for (size_t i = 0; i != buffer_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    bool push(T&& data)
    {
        cell_t* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0)
            {
                if (enqueue_pos_.compare_exchange_weak
                (pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (dif < 0)
                return false;
            else
                pos = enqueue_pos_.load(std::memory_order_relaxed);
        }
        cell->data_ = std::move(data);
        cell->sequence_.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& data)
    {
        cell_t* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq =
                cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0)
            {
                //if (dequeue_pos_.compare_exchange_weak
                //(pos, pos + 1, std::memory_order_relaxed))
                //    break;
                dequeue_pos_.store(pos + 1, std::memory_order_relaxed);
                break;
            }
            else if (dif < 0)
                return false;
            else
                pos = dequeue_pos_.load(std::memory_order_relaxed);
        }
        data = std::move(cell->data_);
        cell->sequence_.store
        (pos + buffer_mask_ + 1, std::memory_order_release);
        return true;
    }

    size_t approx_size() const
    {
        size_t first_pos = dequeue_pos_.load(std::memory_order_relaxed);
        size_t last_pos = enqueue_pos_.load(std::memory_order_relaxed);
        if (last_pos <= first_pos)
            return 0;
        auto size = last_pos - first_pos;
        return size < max_size_ ? size : max_size_;
    }

    size_t capacity() const
    {
        return max_size_;
    }
private:

    static inline size_t _roundup_poser_of_2(size_t queue_size)
    {
        if (queue_size == 0)
        {
            return 0;
        }

        size_t position = 0;
        for (size_t i = queue_size; i != 0; i >>= 1)
        {
            position++;
        }

        return static_cast<size_t>((size_t)1 << position);
    }

    struct cell_t
    {
        std::atomic<size_t>   sequence_;
        T                     data_;
    };

    size_t const            max_size_;
    static size_t const     cacheline_size = 64;
    typedef char            cacheline_pad_t[cacheline_size];
    cacheline_pad_t         pad0_;
    cell_t* const           buffer_;
    size_t const            buffer_mask_;
    cacheline_pad_t         pad1_;
    std::atomic<size_t>     enqueue_pos_;
    cacheline_pad_t         pad2_;
    std::atomic<size_t>     dequeue_pos_;
    cacheline_pad_t         pad3_;
    LockFreeLoopQueue(LockFreeLoopQueue const&);
    void operator = (LockFreeLoopQueue const&);
};

template <typename T>
class LockFreeLoopQueue<T, false, true>
{
public:
    LockFreeLoopQueue(size_t queue_size) :
        max_size_(_roundup_poser_of_2(queue_size)),
        buffer_(new cell_t[_roundup_poser_of_2(queue_size)]),
        buffer_mask_(_roundup_poser_of_2(queue_size) - 1)
    {
        for (size_t i = 0; i != queue_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~LockFreeLoopQueue()
    {
        delete[] buffer_;
    }

    void reinit(void)
    {
        size_t buffer_size = buffer_mask_ + 1;
        assert((buffer_size >= 2) &&
            ((buffer_size & (buffer_size - 1)) == 0));
        for (size_t i = 0; i != buffer_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    bool push(T&& data)
    {
        cell_t* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0)
            {
                //if (enqueue_pos_.compare_exchange_weak
                //(pos, pos + 1, std::memory_order_relaxed))
                //    break;
                enqueue_pos_.store(pos + 1, std::memory_order_relaxed);
                break;
            }
            else if (dif < 0)
                return false;
            else
                pos = enqueue_pos_.load(std::memory_order_relaxed);
        }
        cell->data_ = std::move(data);
        cell->sequence_.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& data)
    {
        cell_t* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq =
                cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0)
            {
                if (dequeue_pos_.compare_exchange_weak
                (pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (dif < 0)
                return false;
            else
                pos = dequeue_pos_.load(std::memory_order_relaxed);
        }
        data = std::move(cell->data_);
        cell->sequence_.store
        (pos + buffer_mask_ + 1, std::memory_order_release);
        return true;
    }

    size_t approx_size() const
    {
        size_t first_pos = dequeue_pos_.load(std::memory_order_relaxed);
        size_t last_pos = enqueue_pos_.load(std::memory_order_relaxed);
        if (last_pos <= first_pos)
            return 0;
        auto size = last_pos - first_pos;
        return size < max_size_ ? size : max_size_;
    }

    size_t capacity() const
    {
        return max_size_;
    }
private:

    static inline size_t _roundup_poser_of_2(size_t queue_size)
    {
        if (queue_size == 0)
        {
            return 0;
        }

        size_t position = 0;
        for (size_t i = queue_size; i != 0; i >>= 1)
        {
            position++;
        }

        return static_cast<size_t>((size_t)1 << position);
    }

    struct cell_t
    {
        std::atomic<size_t>   sequence_;
        T                     data_;
    };

    size_t const            max_size_;
    static size_t const     cacheline_size = 64;
    typedef char            cacheline_pad_t[cacheline_size];
    cacheline_pad_t         pad0_;
    cell_t* const           buffer_;
    size_t const            buffer_mask_;
    cacheline_pad_t         pad1_;
    std::atomic<size_t>     enqueue_pos_;
    cacheline_pad_t         pad2_;
    std::atomic<size_t>     dequeue_pos_;
    cacheline_pad_t         pad3_;
    LockFreeLoopQueue(LockFreeLoopQueue const&);
    void operator = (LockFreeLoopQueue const&);
};

template <typename T>
class LockFreeLoopQueue<T, false, false>
{
public:
    LockFreeLoopQueue(size_t queue_size) :
        max_size_(_roundup_poser_of_2(queue_size)),
        buffer_(new cell_t[_roundup_poser_of_2(queue_size)]),
        buffer_mask_(_roundup_poser_of_2(queue_size) - 1)
    {
        for (size_t i = 0; i != queue_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~LockFreeLoopQueue()
    {
        delete[] buffer_;
    }

    void reinit(void)
    {
        size_t buffer_size = buffer_mask_ + 1;
        assert((buffer_size >= 2) &&
            ((buffer_size & (buffer_size - 1)) == 0));
        for (size_t i = 0; i != buffer_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    bool push(T&& data)
    {
        cell_t* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0)
            {
                //if (enqueue_pos_.compare_exchange_weak
                //(pos, pos + 1, std::memory_order_relaxed))
                //    break;
                enqueue_pos_.store(pos + 1, std::memory_order_relaxed);
                break;
            }
            else if (dif < 0)
                return false;
            else
                pos = enqueue_pos_.load(std::memory_order_relaxed);
        }
        cell->data_ = std::move(data);
        cell->sequence_.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& data)
    {
        cell_t* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq =
                cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0)
            {
                //if (dequeue_pos_.compare_exchange_weak
                //(pos, pos + 1, std::memory_order_relaxed))
                //    break;
                dequeue_pos_.store(pos + 1, std::memory_order_relaxed);
                break;
            }
            else if (dif < 0)
                return false;
            else
                pos = dequeue_pos_.load(std::memory_order_relaxed);
        }
        data = std::move(cell->data_);
        cell->sequence_.store
        (pos + buffer_mask_ + 1, std::memory_order_release);
        return true;
    }

    size_t approx_size() const
    {
        size_t first_pos = dequeue_pos_.load(std::memory_order_relaxed);
        size_t last_pos = enqueue_pos_.load(std::memory_order_relaxed);
        if (last_pos <= first_pos)
            return 0;
        auto size = last_pos - first_pos;
        return size < max_size_ ? size : max_size_;
    }

    size_t capacity() const
    {
        return max_size_;
    }
private:

    static inline size_t _roundup_poser_of_2(size_t queue_size)
    {
        if (queue_size == 0)
        {
            return 0;
        }

        size_t position = 0;
        for (size_t i = queue_size; i != 0; i >>= 1)
        {
            position++;
        }

        return static_cast<size_t>((size_t)1 << position);
    }

    struct cell_t
    {
        std::atomic<size_t>   sequence_;
        T                     data_;
    };

    size_t const            max_size_;
    static size_t const     cacheline_size = 64;
    typedef char            cacheline_pad_t[cacheline_size];
    cacheline_pad_t         pad0_;
    cell_t* const           buffer_;
    size_t const            buffer_mask_;
    cacheline_pad_t         pad1_;
    std::atomic<size_t>     enqueue_pos_;
    cacheline_pad_t         pad2_;
    std::atomic<size_t>     dequeue_pos_;
    cacheline_pad_t         pad3_;
    LockFreeLoopQueue(LockFreeLoopQueue const&);
    void operator = (LockFreeLoopQueue const&);
};