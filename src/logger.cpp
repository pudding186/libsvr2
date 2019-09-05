#ifdef _MSC_VER
#include <windows.h>
#undef max
#undef min
#elif __GNUC__
#include <limits.h>
#include <endian.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#else
#error "unknown compiler"
#endif

#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <fmt/format-inl.h>

#include "./lib_svr_common_def.h"
#include "./lib_svr_common_def.h"
#include "../include/utility.hpp"
#include "../include/memory_pool.h"
#include "../include/loop_cache.h"
#include "../include/logger.hpp"
#include "../include/timer.h"

#define MAX_LOG_FILE_PATH       1024
#define MAX_LOG_FILE_SIZE       1024*1024*1024

typedef enum st_log_option
{
    opt_write   = 0x00000001,
    opt_write_c = (0x00000001 << 1),
    opt_flush   = (0x00000001 << 2),
    opt_close   = (0x00000001 << 3),
    opt_open    = (0x00000001 << 4),
}log_option;

typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> TPMS;

typedef struct st_file_logger
{
#ifdef _MSC_VER
    wchar_t*        path;
    wchar_t*        name;
#elif __GNUC__
    char*           path;
    char*           name;
#else
#error "unknown compiler"
#endif
    FILE*       file;
    size_t      log_thread_idx;
    struct tm   file_time;
    size_t      file_size;
    size_t      file_idx;

    size_t      write_ack;
    std::atomic<size_t>* write_req;
}file_logger;

typedef struct st_log_cmd
{
    log_option      option;
    file_logger*    logger;
    SFormatArgs<>*  fmt_args;
    TPMS            tpms;
    unsigned int    t_id;
}log_cmd;

typedef struct st_log_queue
{
    HLOOPPTRQUEUE   cmd_queue;
    HLOOPPTRQUEUE   rcy_queue;
}log_queue;

typedef struct st_log_proc
{
    struct st_log_proc* next_proc;
    log_queue*          queue;
    bool                is_run;
}log_proc;

typedef struct st_log_obj_pool
{
    struct st_log_obj_pool*     next_pool;
    SMemory::IClassMemory*  obj_pool;
}log_obj_pool;

class log_thread
{
public:
    log_thread();

    ~log_thread();

    unsigned int _proc_log();

    void _proc_log_end();

    void _log_func();

    bool _check_log(log_cmd* cmd);

    void _do_cmd(log_cmd* cmd);

    inline void set_idx(size_t idx) { m_idx = idx; }
    inline unsigned int get_proc_count(void) { return m_do_proc_count; }
protected:
private:
    std::thread m_log_thread;
    size_t      m_idx;
    time_t      m_last_log_time;
    struct tm   m_last_log_tm;
    char        m_time_str[32];
    unsigned int m_do_proc_count;
};

typedef struct st_logger_manager
{
    log_thread*         log_thread_array;
    size_t              log_thread_num;
    size_t              log_queue_size;
    log_proc*           log_proc_head;
    log_obj_pool*       log_obj_pool_head;
    bool                is_run;
}logger_manager;

static logger_manager* g_logger_manager = 0;
static TLS_VAR log_proc* s_log_proc = 0;

class log_proc_check
{
public:
    log_proc_check(void)
    {
        //m_proc = 0;
    }

    ~log_proc_check(void)
    {
        if (s_log_proc)
        {
            s_log_proc->is_run = false;
        }
    }

    //log_proc* m_proc;
protected:
private:
};

static thread_local log_proc_check s_check;

//////////////////////////////////////////////////////////////////////////

log_thread::log_thread()
{
    m_log_thread = std::thread(&log_thread::_log_func, this);
    m_last_log_time = std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()));
#ifdef _MSC_VER
    localtime_s(&m_last_log_tm, &m_last_log_time);
#elif __GNUC__
    localtime_r(&m_last_log_time, &m_last_log_tm);
#else
#error "unknown compiler"
#endif
    strftime(m_time_str, sizeof(m_time_str), "%Y-%m-%d %H:%M:%S", &m_last_log_tm);
    m_do_proc_count = 0;
}

log_thread::~log_thread()
{
    m_log_thread.join();
}

#ifdef _MSC_VER

bool _mk_dir(const wchar_t* dir)
{
    size_t i;
    wchar_t* p1;
    wchar_t* p;
    wchar_t path[MAX_LOG_FILE_PATH] = { 0 };
    wcsncpy_s(path, MAX_LOG_FILE_PATH, dir, _TRUNCATE);
    p1 = path + 1;
    p = path;

    for (i = 0; i < wcslen(path); i++)
    {
        if (*(p + i) == L'\\')
        {
            *(p + i) = L'/';
        }
    }

    do
    {
        p1 = wcschr(p1, L'/');
        if (p1 != 0)
        {
            *p1 = L'\0';
        }

        if (-1 == _wmkdir(path))
        {
            if ((0 == p1) && (errno != EEXIST))
            {
                return false;
            }
        }

        if (p1 != 0)
        {
            *p1++ = L'/';
        }
    } while (p1);

    return true;
}

bool log_thread::_check_log(log_cmd* cmd)
{
    wchar_t file_full_path[MAX_LOG_FILE_PATH];

    time_t cur_time = std::chrono::system_clock::to_time_t(cmd->tpms);

    if (!cmd->logger->file)
    {
        localtime_s(&cmd->logger->file_time, &cur_time);

        _snwprintf_s(file_full_path, MAX_LOG_FILE_PATH, _TRUNCATE,
            L"%ls/%ls_%04d-%02d-%02d.txt",
            cmd->logger->path, cmd->logger->name,
            cmd->logger->file_time.tm_year + 1900,
            cmd->logger->file_time.tm_mon + 1,
            cmd->logger->file_time.tm_mday);

        if (_wfopen_s(&cmd->logger->file, file_full_path, L"a"))
        {
            CRUSH_CODE();
            return false;
        }

        cmd->logger->file_idx = 0;

        fseek(cmd->logger->file, 0, SEEK_END);
        cmd->logger->file_size = ftell(cmd->logger->file);
    }

    if (cur_time != m_last_log_time)
    {
        m_last_log_tm.tm_sec += (int)(cur_time - m_last_log_time);

        if (m_last_log_tm.tm_sec > 59)
        {
            localtime_s(&m_last_log_tm, &cur_time);
        }

        m_last_log_time = cur_time;
        strftime(m_time_str, sizeof(m_time_str), "%Y-%m-%d %H:%M:%S", &m_last_log_tm);

        if (m_last_log_tm.tm_yday != cmd->logger->file_time.tm_yday)
        {
            cmd->logger->file_time = m_last_log_tm;

            if (cmd->logger->file)
            {
                fclose(cmd->logger->file);
                cmd->logger->file = 0;
            }

            _snwprintf_s(file_full_path, MAX_LOG_FILE_PATH, _TRUNCATE,
                L"%ls/%ls_%04d-%02d-%02d.txt",
                cmd->logger->path, cmd->logger->name,
                cmd->logger->file_time.tm_year + 1900,
                cmd->logger->file_time.tm_mon + 1,
                cmd->logger->file_time.tm_mday);

            if (_wfopen_s(&cmd->logger->file, file_full_path, L"a"))
            {
                CRUSH_CODE();
                return false;
            }

            fseek(cmd->logger->file, 0, SEEK_END);
            cmd->logger->file_size = ftell(cmd->logger->file);
        }
    }

    while (cmd->logger->file_size >= MAX_LOG_FILE_SIZE)
    {
        cmd->logger->file_idx++;

        fclose(cmd->logger->file);
        cmd->logger->file = 0;

        _snwprintf_s(file_full_path, MAX_LOG_FILE_PATH, _TRUNCATE,
            L"%ls/%ls_%04d-%02d-%02d_%zu.txt",
            cmd->logger->path, cmd->logger->name,
            cmd->logger->file_time.tm_year + 1900,
            cmd->logger->file_time.tm_mon + 1,
            cmd->logger->file_time.tm_mday,
            cmd->logger->file_idx);

        if (_wfopen_s(&cmd->logger->file, file_full_path, L"a"))
        {
            CRUSH_CODE();
            return false;
        }

        fseek(cmd->logger->file, 0, SEEK_END);
        cmd->logger->file_size = ftell(cmd->logger->file);
    }

    return true;
}
#elif __GNUC__
bool _mk_dir(const char* dir)
{
    char szPath[MAX_LOG_FILE_PATH] = { 0 };
    strncpy(szPath, dir, MAX_LOG_FILE_PATH);
    char* p1 = szPath + 1;

    char* p = szPath;

    for (size_t i = 0; i < strlen(szPath); i++)
    {
        if (*(p + i) == '\\')
        {
            *(p + i) = '/';
        }
    }

    do
    {
        p1 = strchr(p1, '/');
        if (p1 != 0)
        {
            *p1 = '\0';
        }

        if (-1 == mkdir(szPath, 0755))
        {
            if ((0 == p1) && (errno != EEXIST))
            {
                return false;
            }
        }
        if (p1 != 0)
        {
            *p1++ = '/';
        }

    } while (p1 != 0);

    return true;
}

bool log_thread::_check_log(log_cmd* cmd)
{
    char file_full_path[MAX_LOG_FILE_PATH];

    time_t cur_time = std::chrono::system_clock::to_time_t(cmd->tpms);

    if (!cmd->logger->file)
    {
        localtime_r(&cur_time, &cmd->logger->file_time);

        snprintf(file_full_path, MAX_LOG_FILE_PATH,
            "%s/%s_%04d-%02d-%02d.txt",
            cmd->logger->path, cmd->logger->name,
            cmd->logger->file_time.tm_year + 1900,
            cmd->logger->file_time.tm_mon + 1,
            cmd->logger->file_time.tm_mday);

        //if (_wfopen_s(&cmd->logger->file, file_full_path, L"a"))
        cmd->logger->file = fopen(file_full_path, "a");
        if (!cmd->logger->file)
        {
            CRUSH_CODE();
            return false;
        }

        cmd->logger->file_idx = 0;

        fseek(cmd->logger->file, 0, SEEK_END);
        cmd->logger->file_size = ftell(cmd->logger->file);
    }

    if (cur_time != m_last_log_time)
    {
        m_last_log_tm.tm_sec += (int)(cur_time - m_last_log_time);

        if (m_last_log_tm.tm_sec > 59)
        {
            localtime_r(&cur_time, &m_last_log_tm);
        }

        m_last_log_time = cur_time;
        strftime(m_time_str, sizeof(m_time_str), "%Y-%m-%d %H:%M:%S", &m_last_log_tm);

        if (m_last_log_tm.tm_yday != cmd->logger->file_time.tm_yday)
        {
            cmd->logger->file_time = m_last_log_tm;

            if (cmd->logger->file)
            {
                fclose(cmd->logger->file);
                cmd->logger->file = 0;
            }

            snprintf(file_full_path, MAX_LOG_FILE_PATH,
                "%s/%s_%04d-%02d-%02d.txt",
                cmd->logger->path, cmd->logger->name,
                cmd->logger->file_time.tm_year + 1900,
                cmd->logger->file_time.tm_mon + 1,
                cmd->logger->file_time.tm_mday);

            cmd->logger->file = fopen(file_full_path, "a");
            if (!cmd->logger->file)
            {
                CRUSH_CODE();
                return false;
            }

            fseek(cmd->logger->file, 0, SEEK_END);
            cmd->logger->file_size = ftell(cmd->logger->file);
        }
    }

    while (cmd->logger->file_size >= MAX_LOG_FILE_SIZE)
    {
        cmd->logger->file_idx++;

        fclose(cmd->logger->file);
        cmd->logger->file = 0;

        snprintf(file_full_path, MAX_LOG_FILE_PATH,
            "%s/%s_%04d-%02d-%02d_%zu.txt",
            cmd->logger->path, cmd->logger->name,
            cmd->logger->file_time.tm_year + 1900,
            cmd->logger->file_time.tm_mon + 1,
            cmd->logger->file_time.tm_mday,
            cmd->logger->file_idx);

        cmd->logger->file = fopen(file_full_path, "a");
        if (!cmd->logger->file)
        {
            CRUSH_CODE();
            return false;
        }

        fseek(cmd->logger->file, 0, SEEK_END);
        cmd->logger->file_size = ftell(cmd->logger->file);
    }

    return true;
}

#else
#error "unknown compiler"
#endif


void log_thread::_do_cmd(log_cmd* cmd)
{
    switch (cmd->option)
    {
    case opt_write:
    {
        _check_log(cmd);

        fmt::memory_buffer out_prefix;
        fmt::memory_buffer out_data;

        fmt::format_to(out_prefix, "{}.{:<4} <{:<5}> ", m_time_str, cmd->tpms.time_since_epoch().count() % 1000, cmd->t_id);
        cmd->fmt_args->format_to_buffer(out_data);

        size_t write_size = std::fwrite(out_prefix.data(), sizeof(char), out_prefix.size(), cmd->logger->file);

        write_size += std::fwrite(out_data.data(), sizeof(char), out_data.size(), cmd->logger->file);

        cmd->logger->file_size += write_size;

        cmd->logger->write_ack++;
    }
    break;
    case opt_write_c:
    {
        _check_log(cmd);

        fmt::memory_buffer out_prefix;
        fmt::format_to(out_prefix, "{}.{:<4} <{:<5}> ", m_time_str, cmd->tpms.time_since_epoch().count() % 1000, cmd->t_id);

        int write_size = (int)std::fwrite(out_prefix.data(), sizeof(char), out_prefix.size(), cmd->logger->file);
        if (write_size > 0)
        {
            cmd->logger->file_size += write_size;
        }

        write_size = cmd->fmt_args->fprintf(cmd->logger->file);
        if (write_size > 0)
        {
            cmd->logger->file_size += write_size;
        }

        cmd->logger->write_ack++;
    }
    break;
    case opt_close:
    {
        if (cmd->logger->file)
        {
            fclose(cmd->logger->file);
            delete cmd->logger->write_req;
            free(cmd->logger);
        }
    }
    break;
    default:
    {
        CRUSH_CODE();
    }
    }
}

unsigned int log_thread::_proc_log()
{
    unsigned int proc_count = 0;

    log_proc* proc = g_logger_manager->log_proc_head;

    while (proc)
    {
        log_cmd* cmd = (log_cmd*)loop_ptr_queue_pop(proc->queue[m_idx].cmd_queue);

        if (cmd)
        {
            _do_cmd(cmd);
            ++proc_count;

            while (!loop_ptr_queue_push(proc->queue[m_idx].rcy_queue, cmd))
            {
                if (!proc->is_run)
                {
                    break;
                }
#ifdef _MSC_VER
                Sleep(1);
#elif __GNUC__
                usleep(1000);
#else
#error "unknown compiler"
#endif
            }
        }

        proc = proc->next_proc;
    }

    return proc_count;
}

void log_thread::_proc_log_end()
{
    log_proc* proc = g_logger_manager->log_proc_head;
    bool is_busy = false;

    for (;;)
    {
        if (!proc)
        {
            if (is_busy)
            {
                proc = g_logger_manager->log_proc_head;
                is_busy = false;
            }
            else
            {
                return;
            }
        }

        log_cmd* cmd = (log_cmd*)loop_ptr_queue_pop(proc->queue[m_idx].cmd_queue);

        if (cmd)
        {
            _do_cmd(cmd);
            is_busy = true;
        }

        proc = proc->next_proc;
    }
}

void log_thread::_log_func()
{
    unsigned int cur_do_proc_count = 0;
    unsigned int run_loop_check = 0;
    unsigned int last_do_proc_count = 0;
    while (g_logger_manager->is_run)
    {
        last_do_proc_count = _proc_log();
        cur_do_proc_count += last_do_proc_count;
        ++run_loop_check;

        if (run_loop_check >= 100)
        {
            m_do_proc_count = cur_do_proc_count;
            run_loop_check = 0;
            cur_do_proc_count = 0;
        }

        if (!last_do_proc_count)
        {
#ifdef _MSC_VER
            Sleep(10);
#elif __GNUC__
            usleep(10000000);
#else
#error "unknown compiler"
#endif
        }
    }

    _proc_log_end();
}

size_t _get_idle_log_thread_idx(void)
{
    size_t idle_log_thread_idx = 0;
    unsigned int do_proc_count = g_logger_manager->log_thread_array[0].get_proc_count();

    for (size_t i = 1; i < g_logger_manager->log_thread_num; i++)
    {
        if (do_proc_count > g_logger_manager->log_thread_array[i].get_proc_count())
        {
            do_proc_count = g_logger_manager->log_thread_array[i].get_proc_count();
            idle_log_thread_idx = i;
        }
    }

    return idle_log_thread_idx;
}

void update_logger_obj_pool(SMemory::IClassMemory* new_obj_pool)
{
    log_obj_pool* new_pool = (log_obj_pool*)malloc(sizeof(log_obj_pool));
    new_pool->obj_pool = new_obj_pool;

    std::mutex mtx;
    mtx.lock();
    new_pool->next_pool = g_logger_manager->log_obj_pool_head;
    g_logger_manager->log_obj_pool_head = new_pool;
    mtx.unlock();
}

log_proc* _get_log_proc(void)
{
    if (!s_log_proc)
    {
        s_log_proc = (log_proc*)malloc(sizeof(log_proc));
        s_log_proc->queue = (log_queue*)malloc(sizeof(log_queue)*g_logger_manager->log_thread_num);

        for (size_t i = 0; i < g_logger_manager->log_thread_num; i++)
        {
            s_log_proc->queue[i].cmd_queue = create_loop_ptr_queue(g_logger_manager->log_queue_size);
            s_log_proc->queue[i].rcy_queue = create_loop_ptr_queue(g_logger_manager->log_queue_size);
        }

        //s_check.m_proc = s_log_proc;
        //s_log_proc->is_run = true;
        s_log_proc->is_run = true;

        std::mutex mtx;
        mtx.lock();
        s_log_proc->next_proc = g_logger_manager->log_proc_head;
        g_logger_manager->log_proc_head = s_log_proc;
        mtx.unlock();
    }

    return s_log_proc;
}

log_cmd* _get_log_cmd(log_proc* proc)
{
    log_cmd* cmd = 0;
    log_cmd* last_cmd = 0;

    for (size_t i = 0; i < g_logger_manager->log_thread_num; i++)
    {
        for (;;)
        {
            last_cmd = (log_cmd*)loop_ptr_queue_pop(proc->queue[i].rcy_queue);

            if (last_cmd)
            {
                if (cmd)
                {
                    SMemory::Delete(cmd->fmt_args);
                    SMemory::Delete(cmd);
                }
                cmd = last_cmd;
            }
            else
            {
                break;
            }
        }
    }

    if (cmd)
    {
        SMemory::Delete(cmd->fmt_args);
    }
    else
    {
        cmd = logger_obj_pool<log_cmd>().New(1);
    }

    return cmd;
}

bool file_logger_async_log(file_logger* logger, bool is_c_format, SFormatArgs<>* fmt_args, bool is_block)
{
    log_proc* proc = _get_log_proc();

    log_cmd* cmd = _get_log_cmd(proc);

    cmd->option = opt_write;

    if (is_c_format)
    {
        cmd->option = opt_write_c;
    }
    else
    {
        cmd->option = opt_write;
    }
    cmd->fmt_args = fmt_args;
    cmd->logger = logger;

#ifdef _MSC_VER
    cmd->t_id = ::GetCurrentThreadId();
#elif __GNUC__
    cmd->t_id = syscall(__NR_getpid);
#else
#error "unknown compiler"
#endif

    cmd->tpms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

    if (loop_ptr_queue_push(proc->queue[logger->log_thread_idx].cmd_queue, cmd))
    {
        (*(logger->write_req))++;
        return true;
    }
    else
    {
        if (is_block)
        {
            while (!loop_ptr_queue_push(proc->queue[logger->log_thread_idx].cmd_queue, cmd))
            {
                SMemory::Delete(_get_log_cmd(proc));
            }

            (*(logger->write_req))++;
            return true;
        }
        else
        {
            SMemory::Delete(cmd->fmt_args);
            SMemory::Delete(cmd);

            return false;
        }
    }
}

bool init_logger_manager(size_t log_thread_num, size_t max_log_event_num)
{
    if (!g_logger_manager)
    {
        g_logger_manager = new logger_manager;
        g_logger_manager->is_run = true;
        g_logger_manager->log_proc_head = 0;
        g_logger_manager->log_obj_pool_head = 0;
        g_logger_manager->log_thread_num = log_thread_num;
        g_logger_manager->log_queue_size = max_log_event_num;
        g_logger_manager->log_thread_array = S_NEW(log_thread, log_thread_num);

        for (size_t i = 0; i < log_thread_num; i++)
        {
            g_logger_manager->log_thread_array[i].set_idx(i);
        }
    }

    return true;
}

void uninit_logger_manager(void)
{
    if (g_logger_manager)
    {
        g_logger_manager->is_run = false;

        S_DELETE(g_logger_manager->log_thread_array);


        log_proc* proc = g_logger_manager->log_proc_head;

        while (proc)
        {
            log_proc* cur_proc = proc;
            proc = proc->next_proc;

            for (size_t i = 0; i < g_logger_manager->log_thread_num; i++)
            {
                destroy_loop_ptr_queue(cur_proc->queue[i].cmd_queue);
                destroy_loop_ptr_queue(cur_proc->queue[i].rcy_queue);
            }
            free(cur_proc->queue);
            free(cur_proc);
            cur_proc = 0;
            
        }

        log_obj_pool* pool = g_logger_manager->log_obj_pool_head;

        while (pool)
        {
            log_obj_pool* cur_pool = pool;
            pool = pool->next_pool;

            delete cur_pool->obj_pool;
            free(cur_pool);
        }

        delete g_logger_manager;
        g_logger_manager = 0;
    }
}

#ifdef _MSC_VER
file_logger* create_file_logger(const char* path_utf8, const char* name_utf8)
{
    wchar_t w_path[MAX_LOG_FILE_PATH];
    wchar_t w_name[MAX_LOG_FILE_PATH];

    if (!is_valid_utf8((const unsigned char*)path_utf8, strlen(path_utf8)))
    {
        return 0;
}

    if (!is_valid_utf8((const unsigned char*)name_utf8, strlen(name_utf8)))
    {
        return 0;
    }

    int w_path_len = MultiByteToWideChar(CP_UTF8, 0, path_utf8, -1, w_path, MAX_LOG_FILE_PATH);
    int w_name_len = MultiByteToWideChar(CP_UTF8, 0, name_utf8, -1, w_name, MAX_LOG_FILE_PATH);

    if (!w_path_len || !w_name_len)
    {
        return 0;
    }

    if (!_mk_dir(w_path))
    {
        return 0;
    }

    char* ptr = (char*)malloc(sizeof(file_logger) + sizeof(wchar_t)*(w_path_len + w_name_len));

    file_logger* logger = (file_logger*)ptr;
    logger->path = (wchar_t*)(ptr + sizeof(file_logger));
    logger->name = (wchar_t*)(ptr + sizeof(file_logger) + sizeof(wchar_t)*w_path_len);

    wcscpy_s(logger->path, w_path_len, w_path);
    wcscpy_s(logger->name, w_name_len, w_name);

    logger->log_thread_idx = _get_idle_log_thread_idx();
    logger->file = 0;
    logger->file_time = { 0 };
    logger->file_size = 0;
    logger->file_idx = 0;

    logger->write_ack = 0;

    logger->write_req = new std::atomic<size_t>(0);

    return logger;
}
#elif __GNUC__
file_logger* create_file_logger(const char* path_utf8, const char* name_utf8)
{
    size_t path_len = strlen(path_utf8);
    size_t name_len = strlen(name_utf8);
    if (!is_valid_utf8((const unsigned char*)path_utf8, path_len))
    {
        return 0;
    }

    if (!is_valid_utf8((const unsigned char*)name_utf8, name_len))
    {
        return 0;
    }

    if (!_mk_dir(path_utf8))
    {
        return 0;
    }

    char* ptr = (char*)malloc(sizeof(file_logger) + sizeof(char)*(path_len + name_len + 2));

    file_logger* logger = (file_logger*)ptr;
    logger->path = (char*)(ptr + sizeof(file_logger));
    logger->name = (char*)(ptr + sizeof(file_logger) + sizeof(char)*(path_len + 1));

    strncpy(logger->path, path_utf8, path_len + 1);
    strncpy(logger->name, name_utf8, name_len + 1);

    logger->log_thread_idx = _get_idle_log_thread_idx();
    logger->file = 0;
    logger->file_time = { 0 };
    logger->file_size = 0;
    logger->file_idx = 0;

    logger->write_ack = 0;

    logger->write_req = new std::atomic<size_t>(0);

    return logger;
}
#else
#error "unknown compiler"
#endif



void destroy_file_logger(file_logger* logger)
{
    log_proc* proc = _get_log_proc();

    log_cmd* cmd = _get_log_cmd(proc);

    cmd->option = opt_close;
    cmd->fmt_args = 0;
    cmd->logger = logger;

    while (logger->write_req->load() != logger->write_ack)
    {
#ifdef _MSC_VER
        Sleep(1);
#elif __GNUC__
        usleep(1000);
#else
#error "unknown compiler"
#endif
    }

    while (!loop_ptr_queue_push(proc->queue[logger->log_thread_idx].cmd_queue, cmd))
    {
        SMemory::Delete(_get_log_cmd(proc));
    }
}