#pragma once
#include "./lib_svr_def.h"
#include "./format_args.hpp"

#ifdef __cplusplus
extern "C" {
#endif

extern bool (init_logger_manager)(size_t log_thread_num, size_t max_log_event_num);

extern void (uninit_logger_manager)(void);

extern HFILELOGGER (create_file_logger)(const char* path_utf8, const char* name_utf8);

extern void (destroy_file_logger)(HFILELOGGER file_logger);

#ifdef __cplusplus
}

extern void (update_logger_obj_pool)(SMemory::IClassMemory* new_obj_pool);
extern void (update_logger_mem_pool)(HMEMORYMANAGER new_mem_pool);

template<typename T>
SMemory::CClassMemory<T>* logger_obj_pool(void)
{
    static thread_local SMemory::CClassMemory<T>* obj_pool = 0;

    if (!obj_pool)
    {
        obj_pool = new SMemory::CClassMemory<T>();

        update_logger_obj_pool(obj_pool);
    }

    return obj_pool;
}

extern bool file_logger_async_log(HFILELOGGER file_logger, bool is_c_format, SFormatArgs<>* fmt_args, bool is_block);

template<typename... Args>
void file_logger_log(HFILELOGGER file_logger, const char* fmt, Args&&... args)
{
    SFormatArgs<>* fmt_args = logger_obj_pool<SFormatArgs<const char*, special_decay_type<Args>...>>()->New(1, fmt, std::forward<Args>(args)...);

    file_logger_async_log(file_logger, false, fmt_args, true);
}

template<typename... Args>
void file_logger_print(HFILELOGGER file_logger, const char* fmt, Args&&... args)
{
    SFormatArgs<>* fmt_args = logger_obj_pool<SFormatArgs<const char*, special_decay_type<Args>...>>()->New(1, fmt, std::forward<Args>(args)...);

    file_logger_async_log(file_logger, true, fmt_args, true);
}
#endif