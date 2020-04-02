#pragma once
#include "../../include/smemory.hpp"
#include "../../include/lock_free_queue.hpp"
#include "../../include/timer.h"
#include <windows.h>
#include <thread>
#include <vector>

class TaskProc;
class ITask;

typedef struct st_task_iocp_data
{
    OVERLAPPED  over_lapped;
    ITask*      task;
}task_iocp_data;

class ITask
{
    friend class TaskManager;
public:
    ITask()
    {
        m_data.task = this;
        ZeroMemory(&m_data.over_lapped, sizeof(m_data.over_lapped));
    }

    virtual void OnProc(TaskProc* proc) = 0;
    virtual void OnResult() = 0;
private:
    task_iocp_data m_data;
};



class TaskProc
{
public:
    TaskProc(HANDLE iocp_port, LockFreeLoopQueue<ITask*, true, false>& result_queue)
        :m_iocp_port(iocp_port), m_result_queue(result_queue)
    {
        m_is_run = true;
        m_proc_thread = std::thread(&TaskProc::_do_task, this);
    }

    ~TaskProc()
    {
        m_is_run = false;
        m_proc_thread.join();
    }
protected:

    void _do_task()
    {
        DWORD byte_transferred;
        ITask* task;
        
        task_iocp_data* data;
        

        for (;;)
        {
            task = 0;
            data = 0;

            GetQueuedCompletionStatus(
                m_iocp_port,
                &byte_transferred,
                (PULONG_PTR)&task,
                (LPOVERLAPPED*)&data,
                INFINITE);

            if (!data)
            {
                return;
            }

            data->task->OnProc(this);
            while (!m_result_queue.push(data->task))
            {
                if (m_is_run)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                else
                {
                    break;
                }
            }
        }
    }

protected:
    bool        m_is_run;
    std::thread m_proc_thread;
    HANDLE      m_iocp_port;
    LockFreeLoopQueue<ITask*, true, false>& m_result_queue;
private:
};

class TaskManager
{
public:
    TaskManager()
    {
        m_iocp_port = 0;
        m_result_queue = 0;
    }

    ~TaskManager()
    {

    }

    bool Init(size_t result_queue_size, size_t proc_num = 0)
    {
        m_iocp_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (!m_iocp_port)
        {
            return false;
        }

        m_result_queue = new LockFreeLoopQueue<ITask *, true, false>(result_queue_size);
        if (!m_result_queue)
        {
            CloseHandle(m_iocp_port);
            return false;
        }

        if (!proc_num)
        {
            SYSTEM_INFO sys_info;
            GetSystemInfo(&sys_info);

            proc_num = sys_info.dwNumberOfProcessors + 2;
        }

        m_proc_array.resize(proc_num);

        for (size_t i = 0; i < proc_num; i++)
        {
            m_proc_array[i] = new TaskProc(m_iocp_port, *m_result_queue);
        }

        return true;
    }

    void Uninit()
    {
        for (size_t i = 0; i < m_proc_array.size(); i++)
        {
            PostQueuedCompletionStatus(m_iocp_port, 0, 0, NULL);
        }

        for (size_t i = 0; i < m_proc_array.size(); i++)
        {
            delete m_proc_array[i];
        }

        delete m_result_queue;

        CloseHandle(m_iocp_port);
    }

    template<typename T, typename... Args>
    ITask* CreateTask(Args&&... args)
    {
        return static_cast<ITask*>(S_NEW(T, 1, std::forward<Args>(args)...));
    }

    template<typename T>
    T* DynamicCastTask(ITask* task)
    {
        if (SMemory::IsValidPtr<T>(task))
        {
            return static_cast<T*>(task);
        }

        return 0;
    }

    bool AddTask(ITask* task)
    {
        if (PostQueuedCompletionStatus(m_iocp_port, 0, (ULONG_PTR)task, &task->m_data.over_lapped))
        {
            return true;
        }

        return false;
    }

    bool Run(unsigned int run_time)
    {
        unsigned int tick = get_tick();

        for (;;)
        {
            ITask* task;
            if (!m_result_queue->pop(task))
            {
                return false;
            }

            task->OnResult();

            S_DELETE(task);

            if (run_time)
            {
                if (get_tick() - tick > run_time)
                {
                    break;
                }
            }
        }

        return true;
    }

protected:
    HANDLE                                  m_iocp_port;
    std::vector<TaskProc*>                  m_proc_array;
    LockFreeLoopQueue<ITask*, true, false>* m_result_queue;
};

