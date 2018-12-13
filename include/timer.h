#pragma once
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern HTIMERMANAGER(create_timer_manager)(pfn_on_timer func_on_timer);

extern void (destroy_timer_manager)(HTIMERMANAGER mgr);

extern HTIMERINFO(timer_add)(HTIMERMANAGER mgr, unsigned elapse, int count, void* data);

extern void (timer_mod)(HTIMERINFO timer, unsigned elapse, int count, void* data);

extern void (timer_del)(HTIMERINFO timer);

extern void (timer_update)(HTIMERMANAGER mgr, unsigned run_time);

extern void* (timer_get_data)(HTIMERINFO timer);

extern int (timer_remain_count)(HTIMERINFO timer);

//////////////////////////////////////////////////////////////////////////
extern unsigned int (get_tick)(void);

extern time_t(get_time)(void);

extern char* (time_to_string)(time_t time, char* str, size_t str_len);

extern time_t (string_to_time)(const char* time_string);

extern time_t now_hour(void);

extern time_t now_day(void);

extern time_t now_week(void);

extern unsigned now_month(void);

extern unsigned now_year(void);

extern time_t week_day_to_time(time_t week_day);

extern time_t time_to_week_day(time_t tm);

extern time_t day_begin_time(time_t day);

extern time_t week_begin_time(time_t week);

extern time_t month_begin_time(time_t month);

extern time_t year_begin_time(time_t year);

#ifdef  __cplusplus
}
#endif