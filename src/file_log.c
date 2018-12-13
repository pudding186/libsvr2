#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <time.h>
#include <direct.h>
#include "./lib_svr_common_def.h"
#include "../include/timer.h"
#include "../include/loop_cache.h"
#include "../include/memory_pool.h"
#include "../include/file_log.h"

#define MAX_CMD_DATA_LEN    512
#define MAX_LOG_QUEUE_LEN   4*1024
#define MAX_LOG_FILE_PATH   1024
#define MAX_LOG_FILE_SIZE   1024*1024*1024
#define PRINT_THREAD_IDX    0

typedef enum st_log_option
{
    opt_write = 0x00000001,
    opt_flush = (0x00000001 << 1),
}log_option;

typedef struct st_log_queue
{
    HLOOPPTRQUEUE   cmd_queue;
    HLOOPPTRQUEUE   rcy_queue;
    void*           to_rcy;
}log_queue;

typedef struct st_file_log
{
    wchar_t*        path;
    wchar_t*        name;
    FILE*           file;
    size_t          size;
    size_t          thread_idx;
    size_t          file_idx;
    time_t          time;
    size_t          flag;
}file_log;

typedef struct st_log_cmd
{
    log_option      option;
    file_log_level  level;
    char            data[MAX_CMD_DATA_LEN];
    char*           data_ex;
    int             data_len;
    file_log*       log;
    time_t          time;
    bool            begin_log;
    bool            begin_print;
    bool            end_log;
    bool            end_print;
}log_cmd;

typedef struct st_log_proc 
{
    struct st_log_proc* next_proc;
    log_queue*      queue;
    HMEMORYUNIT     cmd_unit;
    HMEMORYMANAGER  mem_mgr;
}log_proc;

typedef struct st_log_thread_param
{
    HANDLE  log_thread;
    size_t  thread_idx;

    size_t  run_tick;
    size_t  elapse_tick;
    size_t  last_calc_tick;
    size_t  busy_pct;
}log_thread_param;

typedef struct st_file_log_manager
{
    log_thread_param*   log_thread_param_arry;
    size_t              log_thread_param_num;
    log_proc*           proc_head;
    bool                m_is_run;
}file_log_manager;

static file_log_manager g_file_log_mgr = { 0 };

const char* log_lv_to_str(file_log_level lv)
{
    switch (lv)
    {
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
        return "[UNKNOW]";
    }
}

void _begin_console(file_log_level lv)
{
#ifdef _WIN32
    switch (lv)
    {
    case log_sys:
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN);
        break;
    case log_cri:
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED);
        break;
    case log_wrn:
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
        break;
    case log_inf:
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN);
        break;
    case log_dbg:
        break;
    }
#else
    switch (log_level)
    {
    case log_sys:
        printf("\033[32m");
        break;
    case log_cri:
        printf("\033[31m");
        break;
    case log_wrn:
        printf("\033[33m");
        break;
    case log_inf:
        printf("\033[32m");
        break;
    case log_dbg:
        break;
    }
#endif
}

void _end_console(void)
{
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    printf("\033[0m");
#endif
}

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
        if (*(p+i) == L'\\')
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

bool _check_log(file_log* log, time_t cur_time)
{
    wchar_t file_full_path[MAX_LOG_FILE_PATH];

    struct tm tm_cmd;
    struct tm tm_file;

    localtime_s(&tm_cmd, &cur_time);

    if (!log->file)
    {
        _snwprintf_s(file_full_path, MAX_LOG_FILE_PATH, _TRUNCATE,
            L"%ls/%ls_%04d-%02d-%02d.txt",
            log->path, log->name,
            tm_cmd.tm_year + 1900,
            tm_cmd.tm_mon + 1,
            tm_cmd.tm_mday);

        log->time = cur_time;
        log->file_idx = 0;

        if (_wfopen_s(&log->file, file_full_path, L"a"))
        {
            return false;
        }
        fseek(log->file, 0, SEEK_END);
        log->size = ftell(log->file);
    }

    localtime_s(&tm_file, &log->time);

    if (tm_cmd.tm_year != tm_file.tm_year
        || tm_cmd.tm_mon != tm_file.tm_mon
        || tm_cmd.tm_mday != tm_file.tm_mday)
    {
        _snwprintf_s(file_full_path, MAX_LOG_FILE_PATH, _TRUNCATE,
            L"%ls/%ls_%04d-%02d-%02d.txt",
            log->path, log->name,
            tm_cmd.tm_year + 1900,
            tm_cmd.tm_mon + 1,
            tm_cmd.tm_mday);

        log->time = cur_time;
        log->file_idx = 0;

        if (log->file)
        {
            fclose(log->file);
        }

        if (_wfopen_s(&log->file, file_full_path, L"a"))
        {
            return false;
        }
        fseek(log->file, 0, SEEK_END);
        log->size = ftell(log->file);
    }

    while (log->size >= MAX_LOG_FILE_SIZE)
    {
        log->file_idx++;

        _snwprintf_s(file_full_path, MAX_LOG_FILE_PATH, _TRUNCATE,
            L"%ls/%ls_%04d-%02d-%02d_%zu.txt",
            log->path, log->name,
            tm_file.tm_year + 1900,
            tm_file.tm_mon + 1,
            tm_file.tm_mday,
            log->file_idx);

        fclose(log->file);

        if (_wfopen_s(&log->file, file_full_path, L"a"))
        {
            return false;
        }
        fseek(log->file, 0, SEEK_END);
        log->size = ftell(log->file);
    }

    return true;
}

file_log_level g_last_print_level = log_dbg;

bool _proc_print(log_thread_param* param)
{
    bool need_sleep = true;

    log_proc* proc = g_file_log_mgr.proc_head;

    while (proc)
    {
        log_cmd* cmd = proc->queue[param->thread_idx].to_rcy;

        if (cmd)
        {
            if (!loop_ptr_queue_push(proc->queue[param->thread_idx].rcy_queue,
                cmd))
            {
                proc = proc->next_proc;
                continue;
            }
            else
            {
                proc->queue[param->thread_idx].to_rcy = 0;
            }
        }

        cmd = loop_ptr_queue_pop(proc->queue[param->thread_idx].cmd_queue);

        if (cmd)
        {
            need_sleep = false;

            proc->queue[param->thread_idx].to_rcy = cmd;

            switch (cmd->option)
            {
            case opt_write:
            {
                struct tm st_cur_time;
                char* data;

                data = cmd->data;
                if (cmd->data_ex)
                {
                    data = cmd->data_ex;
                }

                localtime_s(&st_cur_time, &cmd->time);

                if (g_last_print_level != cmd->level)
                {
                    _begin_console(cmd->level);
                    g_last_print_level = cmd->level;
                }

                printf("%04d-%02d-%02d %02d:%02d:%02d %s %s\r\n",
                    st_cur_time.tm_year + 1900, 
                    st_cur_time.tm_mon + 1, 
                    st_cur_time.tm_mday,
                    st_cur_time.tm_hour, 
                    st_cur_time.tm_min, 
                    st_cur_time.tm_sec,
                    log_lv_to_str(cmd->level), 
                    data);
            }
            break;
            }
        }


        proc = proc->next_proc;
    }

    return need_sleep;
}

void _proc_print_end(log_thread_param* param)
{
    log_cmd* cmd;

    log_proc* proc = g_file_log_mgr.proc_head;
    bool is_busy = false;

    for (;;)
    {
        if (!proc)
        {
            if (is_busy)
            {
                proc = g_file_log_mgr.proc_head;
                is_busy = false;
            }
            else
            {
                return;
            }
        }

        cmd = loop_ptr_queue_pop(proc->queue[param->thread_idx].cmd_queue);

        if (cmd)
        {
            is_busy = true;

            switch (cmd->option)
            {
            case opt_write:
            {
                struct tm st_cur_time;
                char* data;


                data = cmd->data;
                if (cmd->data_ex)
                {
                    data = cmd->data_ex;
                }

                localtime_s(&st_cur_time, &cmd->time);

                if (g_last_print_level != cmd->level)
                {
                    _begin_console(cmd->level);
                    g_last_print_level = cmd->level;
                }

                printf("%04d-%02d-%02d %02d:%02d:%02d %s %s\r\n",
                    st_cur_time.tm_year + 1900, 
                    st_cur_time.tm_mon + 1, 
                    st_cur_time.tm_mday,
                    st_cur_time.tm_hour, 
                    st_cur_time.tm_min, 
                    st_cur_time.tm_sec,
                    log_lv_to_str(cmd->level), 
                    data);
            }
            break;
            }
        }

        proc = proc->next_proc;
    }
}

bool _proc_log(log_thread_param* param)
{
    bool need_sleep = true;

    log_proc* proc = g_file_log_mgr.proc_head;

    while (proc)
    {
        log_cmd* cmd = proc->queue[param->thread_idx].to_rcy;

        if (cmd)
        {
            if (!loop_ptr_queue_push(proc->queue[param->thread_idx].rcy_queue, 
                cmd))
            {
                proc = proc->next_proc;
                continue;
            }
            else
            {
                proc->queue[param->thread_idx].to_rcy = 0;
            }
        }

        cmd = loop_ptr_queue_pop(proc->queue[param->thread_idx].cmd_queue);

        if (cmd)
        {
            need_sleep = false;

            proc->queue[param->thread_idx].to_rcy = cmd;

            switch (cmd->option)
            {
            case opt_write:
            {
                struct tm st_cur_time;
                int write_size;
                char* data;

                _check_log(cmd->log, cmd->time);

                data = cmd->data;
                if (cmd->data_ex)
                {
                    data = cmd->data_ex;
                }

                localtime_s(&st_cur_time, &cmd->time);
                write_size = fprintf_s(cmd->log->file,
                    "%04d-%02d-%02d %02d:%02d:%02d %s %s\r\n",
                    st_cur_time.tm_year + 1900, st_cur_time.tm_mon + 1, st_cur_time.tm_mday,
                    st_cur_time.tm_hour, st_cur_time.tm_min, st_cur_time.tm_sec,
                    log_lv_to_str(cmd->level), data);

                if (write_size > 0)
                {
                    cmd->log->size += write_size;
                }

            }
            break;
            case opt_flush:
            {
                fflush(cmd->log->file);
            }
            break;
            }
        }
        

        proc = proc->next_proc;
    }

    return need_sleep;
}

void _proc_log_end(log_thread_param* param)
{
    log_cmd* cmd;

    log_proc* proc = g_file_log_mgr.proc_head;
    bool is_busy = false;

    for (;;)
    {
        if (!proc)
        {
            if (is_busy)
            {
                proc = g_file_log_mgr.proc_head;
                is_busy = false;
            }
            else
            {
                return;
            }
        }

        cmd = loop_ptr_queue_pop(proc->queue[param->thread_idx].cmd_queue);

        if (cmd)
        {
            is_busy = true;

            switch (cmd->option)
            {
            case opt_write:
            {
                struct tm st_cur_time;
                int write_size;
                char* data;

                _check_log(cmd->log, cmd->time);

                data = cmd->data;
                if (cmd->data_ex)
                {
                    data = cmd->data_ex;
                }

                localtime_s(&st_cur_time, &cmd->time);
                write_size = fprintf_s(cmd->log->file,
                    "%04d-%02d-%02d %02d:%02d:%02d %s %s\r\n",
                    st_cur_time.tm_year + 1900, st_cur_time.tm_mon + 1, st_cur_time.tm_mday,
                    st_cur_time.tm_hour, st_cur_time.tm_min, st_cur_time.tm_sec,
                    log_lv_to_str(cmd->level), data);

                if (write_size > 0)
                {
                    cmd->log->size += write_size;
                }

            }
            break;
            case opt_flush:
            {
                fflush(cmd->log->file);
            }
            break;
            }
        }

        proc = proc->next_proc;
    }
}

static unsigned int _stdcall _log_thread_func(void* arg)
{
    log_thread_param* param = (log_thread_param*)arg;


    if (param->thread_idx == PRINT_THREAD_IDX)
    {
        while (g_file_log_mgr.m_is_run)
        {
            if (_proc_print(param))
            {
                Sleep(10);
            }
        }

        _proc_print_end(param);
    }
    else
    {
        while (g_file_log_mgr.m_is_run)
        {
            if (_proc_log(param))
            {
                Sleep(10);
            }
        }

        _proc_log_end(param);
    }

    return 0;
}

__declspec(thread) static log_proc* _thread_log_proc = 0;

log_proc* _get_thread_log_proc(void)
{
    if (!_thread_log_proc)
    {
        _thread_log_proc = (log_proc*)malloc(sizeof(log_proc));
        _thread_log_proc->queue = (log_queue*)malloc(sizeof(log_queue)*g_file_log_mgr.log_thread_param_num);

        for (size_t i = 0; i < g_file_log_mgr.log_thread_param_num; i++)
        {
            _thread_log_proc->queue[i].cmd_queue = create_loop_ptr_queue(MAX_LOG_QUEUE_LEN);
            _thread_log_proc->queue[i].rcy_queue = create_loop_ptr_queue(MAX_LOG_QUEUE_LEN);
            _thread_log_proc->queue[i].to_rcy = 0;
        }

        _thread_log_proc->cmd_unit = create_memory_unit(sizeof(log_cmd));
        _thread_log_proc->mem_mgr = create_memory_manager(8, 128, 65536, 4 * 1024, 2);

        _thread_log_proc->next_proc = g_file_log_mgr.proc_head;
        g_file_log_mgr.proc_head = _thread_log_proc;
    }

    return _thread_log_proc;
}

size_t log_thread_busy_pct(size_t thread_idx)
{
    if (thread_idx >= g_file_log_mgr.log_thread_param_num)
    {
        return 100;
    }

    return g_file_log_mgr.log_thread_param_arry[thread_idx].busy_pct;
}

bool init_file_log_manager(size_t log_thread_num)
{
    unsigned thread_id = 0;

    if (g_file_log_mgr.log_thread_param_num)
    {
        return true;
    }

    log_thread_num++;

    g_file_log_mgr.proc_head = 0;
    g_file_log_mgr.m_is_run = true;
    g_file_log_mgr.log_thread_param_num = log_thread_num;
    g_file_log_mgr.log_thread_param_arry =
        (log_thread_param*)malloc(sizeof(log_thread_param)*log_thread_num);

    for (size_t i = 0; i < log_thread_num; i++)
    {
        g_file_log_mgr.log_thread_param_arry[i].log_thread = 0;
        g_file_log_mgr.log_thread_param_arry[i].thread_idx = 0;
        g_file_log_mgr.log_thread_param_arry[i].run_tick = 0;
        g_file_log_mgr.log_thread_param_arry[i].elapse_tick = 0;
        g_file_log_mgr.log_thread_param_arry[i].busy_pct = 0;
        g_file_log_mgr.log_thread_param_arry[i].last_calc_tick = 0;
    }

    for (size_t i = 0; i < log_thread_num; i++)
    {
        g_file_log_mgr.log_thread_param_arry[i].thread_idx = i;
        g_file_log_mgr.log_thread_param_arry[i].log_thread = (HANDLE)_beginthreadex(NULL, 
            0,
            _log_thread_func,
            &g_file_log_mgr.log_thread_param_arry[i],
            0,
            &thread_id);

        if (!g_file_log_mgr.log_thread_param_arry[i].log_thread)
        {
            free(g_file_log_mgr.log_thread_param_arry);
            g_file_log_mgr.m_is_run = false;
            return false;
        }
    }

    return true;
}

void uninit_file_log_manager(void)
{
    g_file_log_mgr.m_is_run = false;

    for (size_t i = 0; i < g_file_log_mgr.log_thread_param_num; i++)
    {
        if (g_file_log_mgr.log_thread_param_arry[i].log_thread)
        {
            WaitForSingleObject(g_file_log_mgr.log_thread_param_arry[i].log_thread,
                INFINITE);
            CloseHandle(g_file_log_mgr.log_thread_param_arry[i].log_thread);
            g_file_log_mgr.log_thread_param_arry[i].log_thread = 0;
        }
    }

    log_proc* proc = g_file_log_mgr.proc_head;

    while (proc)
    {
        log_proc* proc_nex = proc->next_proc;

        destroy_memory_manager(proc->mem_mgr);
        destroy_memory_unit(proc->cmd_unit);

        for (size_t i = 0; i < g_file_log_mgr.log_thread_param_num; i++)
        {
            destroy_loop_ptr_queue(proc->queue[i].cmd_queue);
            destroy_loop_ptr_queue(proc->queue[i].rcy_queue);
        }

        free(proc->queue);

        free(proc);

        proc = proc_nex;
    }

    g_file_log_mgr.log_thread_param_num = 0;
}

file_log* create_wfile_log(const wchar_t* path, const wchar_t* name, size_t log_thread_idx)
{
    size_t log_path_len = wcslen(path);
    size_t log_name_len = wcslen(name);

    char* ptr = (char*)malloc(sizeof(file_log) + sizeof(wchar_t)*(log_path_len + log_name_len + 2));

    file_log* log = (file_log*)ptr;
    log->path = (wchar_t*)(ptr + sizeof(file_log));
    log->name = (wchar_t*)(ptr + sizeof(file_log) + (log_path_len + 1) * sizeof(wchar_t));

    wcscpy_s(log->path, log_path_len+1, path);
    wcscpy_s(log->name, log_name_len+1, name);

    log->file = 0;
    log->size = 0;
    log->file_idx = 0;
    log->thread_idx = log_thread_idx;
    log->time = 0;
    log->flag = 0;

    if (!_mk_dir(log->path))
    {
        free(log);
        return 0;
    }

    if (!_check_log(log, get_time()))
    {
        free(log);
        return 0;
    }

    return log;
}

file_log* create_file_log(const char* path, const char* name, size_t log_thread_idx, unsigned int code_page)
{
    wchar_t w_path[MAX_LOG_FILE_PATH];
    wchar_t w_name[MAX_LOG_FILE_PATH];

    if (!MultiByteToWideChar(code_page, 0, path, -1, w_path, MAX_LOG_FILE_PATH))
    {
        return 0;
    }

    if (!MultiByteToWideChar(code_page, 0, name, -1, w_name, MAX_LOG_FILE_PATH))
    {
        return 0;
    }

    return create_wfile_log(w_path, w_name, log_thread_idx);
}

void destroy_file_log(file_log* log)
{
    fclose(log->file);
    free(log);
}

void file_log_flush(HFILELOG log)
{
    log_proc* proc = _get_thread_log_proc();

    log_cmd* cmd = 0;

    for (;;)
    {
        log_cmd* check_cmd = loop_ptr_queue_pop(proc->queue[log->thread_idx].rcy_queue);
        if (!check_cmd)
        {
            check_cmd = loop_ptr_queue_pop(proc->queue[PRINT_THREAD_IDX].rcy_queue);

            if (!check_cmd)
            {
                break;
            }
            else
            {
                check_cmd->end_print = true;

                if (check_cmd->begin_log)
                {
                    if (!check_cmd->end_log)
                    {
                        continue;
                    }
                }

                cmd = check_cmd;
                break;
            }
        }
        else
        {
            check_cmd->end_log = true;
            if (check_cmd->begin_print)
            {
                if (!check_cmd->end_print)
                {
                    continue;
                }
            }
            cmd = check_cmd;
            break;
        }
    }

    if (cmd)
    {
        if (cmd->data_ex)
        {
            memory_manager_free(proc->mem_mgr, cmd->data_ex);
            cmd->data_ex = 0;
        }
    }
    else
    {
        cmd = memory_unit_alloc(proc->cmd_unit, 4096);
        cmd->data_ex = 0;
    }

    cmd->option = opt_flush;
    cmd->level = log_dbg;
    cmd->data_len = 0;
    cmd->log = log;
    cmd->time = 0;
    cmd->begin_log = false;
    cmd->begin_print = false;
    cmd->end_log = false;
    cmd->end_print = false;

    while (!loop_ptr_queue_push(proc->queue[log->thread_idx].cmd_queue, cmd))
    {
        if (g_file_log_mgr.m_is_run)
        {
            log_cmd* rcy_cmd = loop_ptr_queue_pop(proc->queue[log->thread_idx].rcy_queue);
            if (rcy_cmd)
            {
                if (rcy_cmd->data_ex)
                {
                    memory_manager_free(proc->mem_mgr, rcy_cmd->data_ex);
                }

                memory_unit_free(proc->cmd_unit, rcy_cmd);
            }
            else
            {
                Sleep(10);
            }
        }
        else
        {
            return;
        }
    }

    cmd->begin_log = true;
}

bool file_log_write(file_log* log, file_log_level lv, const char* format, ...)
{
    log_proc* proc = _get_thread_log_proc();

    log_cmd* cmd = 0;

    for (;;)
    {
        log_cmd* check_cmd = loop_ptr_queue_pop(proc->queue[log->thread_idx].rcy_queue);
        if (!check_cmd)
        {
            check_cmd = loop_ptr_queue_pop(proc->queue[PRINT_THREAD_IDX].rcy_queue);

            if (!check_cmd)
            {
                break;
            }
            else
            {
                check_cmd->end_print = true;

                if (check_cmd->begin_log)
                {
                    if (!check_cmd->end_log)
                    {
                        continue;
                    }
                }

                cmd = check_cmd;
                break;
            }
        }
        else
        {
            check_cmd->end_log = true;
            if (check_cmd->begin_print)
            {
                if (!check_cmd->end_print)
                {
                    continue;
                }
            }
            cmd = check_cmd;
            break;
        }
    }

    if (cmd)
    {
        if (cmd->data_ex)
        {
            memory_manager_free(proc->mem_mgr, cmd->data_ex);
            cmd->data_ex = 0;
        }
    }
    else
    {
        cmd = memory_unit_alloc(proc->cmd_unit, 4096);
        cmd->data_ex = 0;
    }

    cmd->option = opt_write;
    cmd->level = lv;
    cmd->data_len = 0;
    cmd->log = log;
    cmd->time = get_time();
    cmd->begin_log = false;
    cmd->begin_print = false;
    cmd->end_log = false;
    cmd->end_print = false;

    va_list args;
    va_start(args, format);
    cmd->data_len = vsnprintf(cmd->data, sizeof(cmd->data), format, args);
    va_end(args);

    if (cmd->data_len < 0)
    {
        memory_unit_free(proc->cmd_unit, cmd);
        return false;
    }
    else if (cmd->data_len >= sizeof(cmd->data))
    {
        va_list args_ex;

        int new_data_len = cmd->data_len + 1;
        cmd->data_ex = memory_manager_alloc(proc->mem_mgr, new_data_len);


        va_start(args_ex, format);
        cmd->data_len = vsnprintf(cmd->data_ex, new_data_len, format, args_ex);
        va_end(args_ex);

        if (cmd->data_len != (new_data_len - 1))
        {
            memory_manager_free(proc->mem_mgr, cmd->data_ex);
            memory_unit_free(proc->cmd_unit, cmd);
            return false;
        }
    }

    while (!loop_ptr_queue_push(proc->queue[log->thread_idx].cmd_queue, cmd))
    {
        if (g_file_log_mgr.m_is_run)
        {
            log_cmd* rcy_cmd = loop_ptr_queue_pop(proc->queue[log->thread_idx].rcy_queue);
            if (rcy_cmd)
            {
                if (rcy_cmd->data_ex)
                {
                    memory_manager_free(proc->mem_mgr, rcy_cmd->data_ex);
                }

                memory_unit_free(proc->cmd_unit, rcy_cmd);
            }
            else
            {
                Sleep(10);
            }
        }
        else
        {
            return false;
        }
    }

    cmd->begin_log = true;

    if (loop_ptr_queue_push(proc->queue[PRINT_THREAD_IDX].cmd_queue, cmd))
    {
        cmd->begin_print = true;
    }

    return true;
}
