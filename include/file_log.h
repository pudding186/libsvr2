#pragma once
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern bool (init_file_log_manager)(size_t log_thread_num);

extern void (uninit_file_log_manager)(void);

extern HFILELOG (create_file_log)(const char* path, const char* name, size_t log_thread_idx, unsigned int code_page);

extern HFILELOG(create_wfile_log)(const wchar_t* path, const wchar_t* name, size_t log_thread_idx);

extern void (destroy_file_log)(HFILELOG log);

extern bool (file_log_write)(HFILELOG log, file_log_level lv, const char* format, ...);

extern void (file_log_flush)(HFILELOG log);

#ifdef  __cplusplus
}
#endif