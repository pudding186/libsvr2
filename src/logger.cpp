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
    log_option          option;
    file_logger*        logger;
    file_logger_level   lv;
    SFormatArgs<>*      fmt_args;
    TPMS                tpms;
}log_cmd;

typedef struct st_print_cmd
{
    file_logger_level   lv;
    size_t              data_len;
}print_cmd;

typedef struct st_log_queue
{
    HLOOPCACHE  cmd_queue;
    HLOOPCACHE  rcy_queue;
}log_queue;

typedef struct st_log_proc
{
    struct st_log_proc* next_proc;
    log_queue*          queue;
    time_t              last_log_time;
    struct tm           last_log_tm;
    char                time_str[32];
    unsigned int        t_id;
    bool                is_run;
}log_proc;

typedef struct st_log_obj_pool
{
    struct st_log_obj_pool*     next_pool;
    SMemory::IClassMemory*      obj_pool;
}log_obj_pool;
typedef struct st_log_mem_pool
{
    struct st_log_mem_pool*     next_pool;
    HMEMORYMANAGER              mem_pool;
}log_mem_pool;

class log_thread
{
public:
    log_thread();

    ~log_thread();

    unsigned int _proc_log();

    void _proc_log_end();

    void _log_func();

    bool _check_log(log_cmd* cmd, log_proc* proc);

    void _do_cmd(log_cmd* cmd, log_proc* proc);

    inline void set_idx(size_t idx) { m_idx = idx; }
    inline unsigned int get_proc_count(void) { return m_do_proc_count; }
    inline std::thread& log_thread_ref(void) { return m_log_thread; }
    inline HLOOPCACHE print_data_cache(void) { return m_print_data_cache; }
    inline bool is_run(void) { return m_is_run; }
protected:
private:
    std::thread     m_log_thread;
    size_t          m_idx;
    unsigned int    m_do_proc_count;
    HLOOPCACHE      m_print_data_cache;
    bool            m_is_run;
};

class print_thread
{
public:
    print_thread();
    ~print_thread();

    void _check_print(print_cmd* cmd);

    void _print_func();

    unsigned int _proc_print();

    void _proc_print_end();
    
protected:
private:
    std::thread         m_print_thread;
    file_logger_level   m_last_level;
};

typedef struct st_logger_manager
{
    log_thread*         log_thread_array;
    size_t              log_thread_num;
    size_t              log_queue_size;
    log_proc*           log_proc_head;
    log_proc**          main_log_proc;
    log_obj_pool*       log_obj_pool_head;
    log_mem_pool*       log_mem_pool_head;
    print_thread*       print_thread_pt;
    size_t              print_cache_size;
    bool                is_run;
}logger_manager;

static logger_manager* g_logger_manager = 0;
static TLS_VAR log_proc* s_log_proc = 0;

class log_proc_check
{
public:
    log_proc_check(void)
    {
    }

    ~log_proc_check(void)
    {
        if (s_log_proc)
        {
            s_log_proc->is_run = false;
        }
    }

    bool m_is_use;

protected:
private:
};

static thread_local log_proc_check s_check;

//////////////////////////////////////////////////////////////////////////

log_thread::log_thread()
{
    m_print_data_cache = create_loop_cache(0, g_logger_manager->print_cache_size);
    m_log_thread = std::thread(&log_thread::_log_func, this);
    m_do_proc_count = 0;
    
}

log_thread::~log_thread()
{
    m_log_thread.join();

    destroy_loop_cache(m_print_data_cache);
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

bool log_thread::_check_log(log_cmd* cmd, log_proc* proc)
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

    if (cur_time != proc->last_log_time)
    {
        if (cur_time < proc->last_log_time)
        {
            CRUSH_CODE();
        }

        proc->last_log_tm.tm_sec += (int)(cur_time - proc->last_log_time);

        if (proc->last_log_tm.tm_sec > 59)
        {
            localtime_s(&proc->last_log_tm, &cur_time);
        }

        proc->last_log_time = cur_time;
        strftime(proc->time_str, sizeof(proc->time_str), "%Y-%m-%d %H:%M:%S", &proc->last_log_tm);

        if (proc->last_log_tm.tm_yday != cmd->logger->file_time.tm_yday)
        {
            cmd->logger->file_time = proc->last_log_tm;

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

bool log_thread::_check_log(log_cmd* cmd, log_proc* proc)
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

    if (cur_time != proc->last_log_time)
    {
        if (cur_time < proc->last_log_time)
        {
            CRUSH_CODE();
        }

        proc->last_log_tm.tm_sec += (int)(cur_time - proc->last_log_time);

        if (proc->last_log_tm.tm_sec > 59)
        {
            localtime_r(&cur_time, &proc->last_log_tm);
        }

        proc->last_log_time = cur_time;
        strftime(proc->time_str, sizeof(proc->time_str), "%Y-%m-%d %H:%M:%S", &proc->last_log_tm);

        if (proc->last_log_tm.tm_yday != cmd->logger->file_time.tm_yday)
        {
            cmd->logger->file_time = proc->last_log_tm;

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

const char* log_lv_to_str(file_logger_level lv)
{
    switch (lv)
    {
    case log_nul:
        return "";
        break;
    case log_dbg:
        return "[DBG]";
        break;
    case log_inf:
        return "[INF]";
        break;
    case log_wrn:
        return "[WRN]";
        break;
    case log_cri:
        return "[CRI]";
        break;
    case log_sys:
        return "[SYS]";
        break;
    default:
        return "[OMG]";
    }
}

void log_thread::_do_cmd(log_cmd* cmd, log_proc* proc)
{
    switch (cmd->option)
    {
    case opt_write:
    {
        _check_log(cmd, proc);

        fmt::memory_buffer out_prefix;
        fmt::memory_buffer out_data;
        fmt::format_to(out_prefix, "{}.{:<4}<{:<5}> {}: ", proc->time_str, cmd->tpms.time_since_epoch().count() % 1000, proc->t_id, log_lv_to_str(cmd->lv));
        cmd->fmt_args->format_to_buffer(out_data);
        out_data.push_back('\n');

        size_t write_size = std::fwrite(out_prefix.data(), sizeof(char), out_prefix.size(), cmd->logger->file);
        write_size += std::fwrite(out_data.data(), sizeof(char), out_data.size(), cmd->logger->file);

        cmd->logger->file_size += write_size;

        cmd->logger->write_ack++;

        print_cmd pt_cmd;
        pt_cmd.data_len = out_prefix.size() + out_data.size();
        pt_cmd.lv = cmd->lv;

        if (loop_cache_free_size(m_print_data_cache) >= sizeof(print_cmd)+pt_cmd.data_len)
        {
            loop_cache_push_data(m_print_data_cache, &pt_cmd, sizeof(print_cmd));
            loop_cache_push_data(m_print_data_cache, out_prefix.data(), out_prefix.size());
            loop_cache_push_data(m_print_data_cache, out_data.data(), out_data.size());
        }
    }
    break;
    case opt_write_c:
    {
        _check_log(cmd, proc);

        fmt::memory_buffer out_prefix;
        fmt::memory_buffer out_data;
        fmt::format_to(out_prefix, "{}.{:<4}<{:<5}> {}: ", proc->time_str, cmd->tpms.time_since_epoch().count() % 1000, proc->t_id, log_lv_to_str(cmd->lv));
        cmd->fmt_args->format_c_to_buffer(out_data);
        out_data.push_back('\n');

        size_t write_size = std::fwrite(out_prefix.data(), sizeof(char), out_prefix.size(), cmd->logger->file);
        write_size += std::fwrite(out_data.data(), sizeof(char), out_data.size(), cmd->logger->file);

        cmd->logger->file_size += write_size;

        cmd->logger->write_ack++;

        print_cmd pt_cmd;
        pt_cmd.data_len = out_prefix.size() + out_data.size();
        pt_cmd.lv = cmd->lv;

        if (loop_cache_free_size(m_print_data_cache) >= sizeof(print_cmd) + pt_cmd.data_len)
        {
            loop_cache_push_data(m_print_data_cache, &pt_cmd, sizeof(print_cmd));
            loop_cache_push_data(m_print_data_cache, out_prefix.data(), out_prefix.size());
            loop_cache_push_data(m_print_data_cache, out_data.data(), out_data.size());
        }
    }
    break;
    case opt_flush:
    {
        if (cmd->logger->file)
        {
            fflush(cmd->logger->file);
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
        log_cmd* cmd;

        if (loop_cache_pop_data(proc->queue[m_idx].cmd_queue, &cmd, sizeof(log_cmd*)))
        {
            _do_cmd(cmd, proc);
            ++proc_count;

            while (!loop_cache_push_data(proc->queue[m_idx].rcy_queue, &cmd, sizeof(log_cmd*)))
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

        log_cmd* cmd;

        if (loop_cache_pop_data(proc->queue[m_idx].cmd_queue, &cmd, sizeof(log_cmd*)))
        {
            _do_cmd(cmd, proc);
            is_busy = true;
        }

        proc = proc->next_proc;
    }
}

void log_thread::_log_func()
{
    m_is_run = true;
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

    m_is_run = false;
}

//////////////////////////////////////////////////////////////////////////

print_thread::print_thread()
{
    m_print_thread = std::thread(&print_thread::_print_func, this);
    m_last_level = log_nul;
}

print_thread::~print_thread()
{
    m_print_thread.join();
}

void print_thread::_check_print(print_cmd* cmd)
{
    if (cmd->lv != m_last_level)
    {
        m_last_level = cmd->lv;

        switch (m_last_level)
        {
        case log_sys:
            printf("\033[1;32m");
            break;
        case log_cri:
            printf("\033[1;31m");
            break;
        case log_wrn:
            printf("\033[1;33m");
            break;
        case log_inf:
            printf("\033[1;34m");
            break;
        case log_dbg:
            printf("\033[1;37m");
            break;
        case log_nul:
            printf("\033[0m");
            break;
        default:
            break;
        }
    }
}

void print_thread::_print_func()
{
    while (g_logger_manager->is_run)
    {
        if (!_proc_print())
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

    for (size_t i = 0; i < g_logger_manager->log_thread_num;)
    {
        if (g_logger_manager->log_thread_array[i].is_run())
        {
            i = 0;
#ifdef _MSC_VER
            Sleep(10);
#elif __GNUC__
            usleep(10000000);
#else
#error "unknown compiler"
#endif
            continue;
        }
        else
        {
            i++;
        }
    }

    _proc_print_end();
}

unsigned int print_thread::_proc_print()
{
    unsigned int print_count = 0;

    for (size_t i = 0; i < g_logger_manager->log_thread_num; i++)
    {
        print_cmd cmd;

        if (loop_cache_pop_data(g_logger_manager->log_thread_array[i].print_data_cache(),
            &cmd, sizeof(print_cmd)))
        {
            _check_print(&cmd);

            size_t print_len = cmd.data_len;
            char* data = 0;
            loop_cache_get_data(g_logger_manager->log_thread_array[i].print_data_cache(),
                (void**)&data, &print_len);
            std::fwrite(data, sizeof(char), print_len, stdout);

            if (cmd.data_len > print_len)
            {
                print_len = cmd.data_len - print_len;
                loop_cache_get_data(g_logger_manager->log_thread_array[i].print_data_cache(),
                    (void**)&data, &print_len);
                std::fwrite(data, sizeof(char), print_len, stdout);
            }

            loop_cache_pop(g_logger_manager->log_thread_array[i].print_data_cache(),
                cmd.data_len);

            ++print_count;
        }
    }

    return print_count;
}

void print_thread::_proc_print_end()
{
    for (;;)
    {
        bool busy = false;

        for (size_t i = 0; i < g_logger_manager->log_thread_num; i++)
        {
            print_cmd cmd;

            if (loop_cache_pop_data(g_logger_manager->log_thread_array[i].print_data_cache(),
                &cmd, sizeof(print_cmd)))
            {
                _check_print(&cmd);

                size_t print_len = cmd.data_len;
                char* data;
                loop_cache_get_data(g_logger_manager->log_thread_array[i].print_data_cache(),
                    (void**)&data, &print_len);
                std::fwrite(data, sizeof(char), print_len, stdout);

                if (cmd.data_len > print_len)
                {
                    print_len = cmd.data_len - print_len;
                    loop_cache_get_data(g_logger_manager->log_thread_array[i].print_data_cache(),
                        (void**)&data, &print_len);
                    std::fwrite(data, sizeof(char), print_len, stdout);
                }

                loop_cache_pop(g_logger_manager->log_thread_array[i].print_data_cache(),
                    cmd.data_len);

                busy = true;
            }
        }

        if (!busy)
        {
            break;
        }
    }

    print_cmd end_print_cmd;
    end_print_cmd.data_len = 0;
    end_print_cmd.lv = log_nul;
    _check_print(&end_print_cmd);
}

//////////////////////////////////////////////////////////////////////////

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

void update_logger_mem_pool(HMEMORYMANAGER new_mem_pool)
{
    log_mem_pool* new_pool = (log_mem_pool*)malloc(sizeof(log_mem_pool));
    new_pool->mem_pool = new_mem_pool;

    std::mutex mtx;
    mtx.lock();
    new_pool->next_pool = g_logger_manager->log_mem_pool_head;
    g_logger_manager->log_mem_pool_head = new_pool;
    mtx.unlock();
}

HMEMORYMANAGER logger_mem_pool(void)
{
    static thread_local HMEMORYMANAGER mem_mgr = 0;

    if (!mem_mgr)
    {
        mem_mgr = create_memory_manager(8, 128, 65536, 4 * 1024, 2);

        update_logger_mem_pool(mem_mgr);
    }

    return mem_mgr;
}

log_proc* _get_log_proc(void)
{
    if (!s_log_proc)
    {
        s_log_proc = (log_proc*)malloc(sizeof(log_proc));
        s_log_proc->queue = (log_queue*)malloc(sizeof(log_queue)*g_logger_manager->log_thread_num);

        for (size_t i = 0; i < g_logger_manager->log_thread_num; i++)
        {
            s_log_proc->queue[i].cmd_queue = create_loop_cache(0, sizeof(log_cmd*)*g_logger_manager->log_queue_size);
            s_log_proc->queue[i].rcy_queue = create_loop_cache(0, sizeof(log_cmd*)*g_logger_manager->log_queue_size);
        }

        s_log_proc->last_log_time = std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()));
#ifdef _MSC_VER
        localtime_s(&s_log_proc->last_log_tm, &s_log_proc->last_log_time);
        s_log_proc->t_id = ::GetCurrentThreadId();
#elif __GNUC__
        localtime_r(&s_log_proc->last_log_time, &s_log_proc->last_log_tm);
        s_log_proc->t_id = syscall(__NR_gettid);
#else
#error "unknown compiler"
#endif
        strftime(s_log_proc->time_str, sizeof(s_log_proc->time_str), "%Y-%m-%d %H:%M:%S", &s_log_proc->last_log_tm);

        s_check.m_is_use = true;
        s_log_proc->is_run = true;

        std::mutex mtx;
        mtx.lock();
        s_log_proc->next_proc = g_logger_manager->log_proc_head;
        g_logger_manager->log_proc_head = s_log_proc;
        mtx.unlock();
    }

    return s_log_proc;
}

void _free_log_cmd(log_proc* proc)
{
    for (size_t i = 0; i < g_logger_manager->log_thread_num; i++)
    {
        for (;;)
        {
            log_cmd* cmd;

            if (loop_cache_pop_data(proc->queue[i].rcy_queue, &cmd, sizeof(log_cmd*)))
            {
                SMemory::Delete(cmd->fmt_args);
                SMemory::Delete(cmd);
            }
            else
            {
                break;
            }
        }
    }
}

log_cmd* _get_log_cmd(log_proc* proc)
{
    log_cmd* cmd = 0;
    log_cmd* last_cmd = 0;

    for (size_t i = 0; i < g_logger_manager->log_thread_num; i++)
    {
        for (;;)
        {
            if (loop_cache_pop_data(proc->queue[i].rcy_queue, &last_cmd, sizeof(log_cmd*)))
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
        cmd = logger_obj_pool<log_cmd>()->New(1);
    }

    return cmd;
}

bool file_logger_async_log(file_logger* logger, bool is_c_format, file_logger_level lv, SFormatArgs<>* fmt_args, bool is_block)
{
    log_proc* proc = _get_log_proc();

    log_cmd* cmd = _get_log_cmd(proc);

    cmd->option = opt_write;

    if (is_c_format)
    {
        cmd->option = opt_write_c;
    }

    cmd->fmt_args = fmt_args;
    cmd->logger = logger;
    cmd->lv = lv;

    cmd->tpms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    if (loop_cache_push_data(proc->queue[logger->log_thread_idx].cmd_queue, &cmd, sizeof(log_cmd*)))
    {
        (*(logger->write_req))++;
        return true;
    }
    else
    {
        if (is_block)
        {
            while (!loop_cache_push_data(proc->queue[logger->log_thread_idx].cmd_queue, &cmd, sizeof(log_cmd*)))
            {
                _free_log_cmd(proc);
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

bool init_logger_manager(size_t log_thread_num, size_t max_log_event_num, size_t print_cache_size)
{
    if (!g_logger_manager)
    {
        g_logger_manager = new logger_manager;
        g_logger_manager->is_run = true;
        g_logger_manager->log_proc_head = 0;
        g_logger_manager->log_obj_pool_head = 0;
        g_logger_manager->log_thread_num = log_thread_num;
        g_logger_manager->log_queue_size = max_log_event_num;
        g_logger_manager->print_cache_size = print_cache_size;
        g_logger_manager->log_thread_array = S_NEW(log_thread, log_thread_num);
        g_logger_manager->main_log_proc = &s_log_proc;
        for (size_t i = 0; i < log_thread_num; i++)
        {
            g_logger_manager->log_thread_array[i].set_idx(i);
        }
        g_logger_manager->print_thread_pt = S_NEW(print_thread, 1);
    }

    return true;
}

void uninit_logger_manager(void)
{
    if (g_logger_manager)
    {
        g_logger_manager->is_run = false;

        S_DELETE(g_logger_manager->print_thread_pt);
        S_DELETE(g_logger_manager->log_thread_array);

        log_proc* proc = g_logger_manager->log_proc_head;

        while (proc)
        {
            log_proc* cur_proc = proc;
            proc = proc->next_proc;

            for (size_t i = 0; i < g_logger_manager->log_thread_num; i++)
            {
                destroy_loop_cache(cur_proc->queue[i].cmd_queue);
                destroy_loop_cache(cur_proc->queue[i].rcy_queue);
            }
            free(cur_proc->queue);
            free(cur_proc);
            cur_proc = 0;
            
        }

        if (g_logger_manager->main_log_proc)
        {
            *(g_logger_manager->main_log_proc) = 0;
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

    while (!loop_cache_push_data(proc->queue[logger->log_thread_idx].cmd_queue, &cmd, sizeof(log_cmd*)))
    {
        _free_log_cmd(proc);
    }
}

void file_logger_flush(file_logger* logger)
{
    log_proc* proc = _get_log_proc();

    log_cmd* cmd = _get_log_cmd(proc);

    cmd->option = opt_flush;
    cmd->fmt_args = 0;
    cmd->logger = logger;

    if (loop_cache_push_data(proc->queue[logger->log_thread_idx].cmd_queue, &cmd, sizeof(log_cmd*)))
    {
        (*(logger->write_req))++;
    }
    else
    {
        SMemory::Delete(cmd);
    }
}