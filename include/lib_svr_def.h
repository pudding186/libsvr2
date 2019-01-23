#pragma once
#include <stdbool.h>
#include <stddef.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct st_mem_unit*     HMEMORYUNIT;
typedef struct st_mem_pool*		HMEMORYPOOL;
typedef struct st_mem_mgr*      HMEMORYMANAGER;

typedef void* (*lib_svr_alloc)(void* user_data, void* mem, size_t old_size, size_t new_size);

//////////////////////////////////////////////////////////////////////////
typedef struct st_avl_node* HAVLNODE;
typedef struct st_avl_tree* HAVLTREE;

typedef struct st_rb_node* HRBNODE;
typedef struct st_rb_tree* HRBTREE;

typedef ptrdiff_t(*compare_func)(void* key1, void* key2);

//////////////////////////////////////////////////////////////////////////
typedef struct st_timer_manager* HTIMERMANAGER;
typedef struct st_timer_info* HTIMERINFO;

typedef void(*pfn_on_timer)(HTIMERINFO timer);

//////////////////////////////////////////////////////////////////////////
typedef struct st_loop_cache* HLOOPCACHE;
typedef struct st_loop_ptr_queue* HLOOPPTRQUEUE;

//////////////////////////////////////////////////////////////////////////
typedef struct st_iocp_tcp_socket*      HSESSION;
typedef struct st_iocp_tcp_listener*    HLISTENER;
typedef struct st_iocp_tcp_manager*     HNETMANAGER;

typedef enum e_iocp_tcp_error
{
    error_ssl           = -6,
    error_connect_fail  = -5,
    error_system        = -4,
    error_send_overflow = -3,
    error_recv_overflow = -2,
    error_packet        = -1,
    error_ok            = 0,
}iocp_tcp_error;

typedef struct st_ip_info
{
    enum
    {
        ip_unknow = 0,
        ip_v4 = 2,
        ip_v6 = 23
    }ip_type;
    char ip_str[64];
    unsigned short port;
}ip_info;

typedef struct st_addr_info
{
    enum
    {
        addr_unknow = 0,
        addr_v4 = 2,
        addr_v6 = 23
    }addr_type;

    union 
    {
        struct sockaddr_in* v4;
        struct sockaddr_in6* v6;
    }sock_addr_ptr;
}addr_info;

typedef unsigned int(*pfn_parse_packet)(HSESSION session, const char* data, const unsigned int len);
typedef void (*pfn_on_establish)(HLISTENER net_handle, HSESSION session);
typedef void (*pfn_on_terminate)(HSESSION session);
typedef void (*pfn_on_error)(HSESSION session, iocp_tcp_error module_error, int system_error);
typedef void (*pfn_on_recv)(HSESSION session, const char* data, const unsigned int len);

//////////////////////////////////////////////////////////////////////////
typedef struct st_ws_socket*    HWSSESSION;
typedef struct st_ws_listener*  HWSLISTENER;
typedef struct st_ws_manager*   HWSMANAGER;
typedef void*                   HSSLCTX;

//typedef enum e_web_socket_error
//{
//    error_web_socket_http       = -6,
//    error_web_socket_packet     = -5,
//    error_web_socket_tcp        = -4,
//    error_web_socket_connect    = -3,
//    error_web_socket_upgrade    = -2,
//    error_web_socket_ok         = 0,
//}web_socket_error;
//
//typedef enum e_message_type
//{
//    message_text = 1,
//    message_binary = 2,
//}message_type;
typedef enum e_ws_error
{
    ws_error_ok = 0,
    ws_error_tcp = -5,
    ws_error_http = -6,
    ws_error_websocket = -7
}ws_error;

typedef enum e_ws_op_code
{
    op_text = 1,
    op_binary = 2,
    op_close = 8,
    op_ping = 9,
    op_pong = 10
}ws_op_code;

typedef unsigned int (*pfn_on_http_upgrade)(HWSLISTENER ws_listener, const char* http_request_data, unsigned int http_request_length, char* http_response_data, unsigned int max_http_response_length);
typedef void (*pfn_on_open)(HWSLISTENER ws_listener, HWSSESSION ws_session, const char* url_data, unsigned int url_length);
typedef void (*pfn_on_close)(HWSSESSION ws_session, unsigned short status_code);
typedef void (*pfn_on_fail)(HWSSESSION ws_session, ws_error error, int error_code);
typedef void (*pfn_on_message)(HWSSESSION ws_session, ws_op_code code, const char* data, const unsigned int len);



//////////////////////////////////////////////////////////////////////////
typedef enum e_json_value_type
{
    json_null = 0,
    json_string,
    json_integer,
    json_float,
    json_object,
    json_array,
    json_true,
    json_false,
}json_value_type;

typedef struct st_json_struct* HJSONSTRUCT;
typedef struct st_json_node* HJSONNODE;

typedef void(*json_string_append) (void *ud, const char* data, size_t data_size);


//////////////////////////////////////////////////////////////////////////
typedef struct st_mem_trace_info
{
    const char* name;
    const char* file;
    size_t      line;
    size_t      size;
}MemTraceInfo;

typedef struct st_ptr_info
{
    MemTraceInfo*  info;
    size_t      size;
}PtrInfo;

//////////////////////////////////////////////////////////////////////////
typedef struct st_file_log* HFILELOG;

typedef enum st_file_log_level
{
    log_dbg = 0x00000001,
    log_inf = (0x00000001 << 1),
    log_wrn = (0x00000001 << 2),
    log_cri = (0x00000001 << 3),
    log_sys = (0x00000001 << 4),
}file_log_level;

//////////////////////////////////////////////////////////////////////////
typedef bool (*pfn_wfile)(const wchar_t* path, const wchar_t* name, void* user_data);
typedef bool (*pfn_wdir)(const wchar_t* dir, void* user_data);

#ifdef __cplusplus
}

//////////////////////////////////////////////////////////////////////////
typedef class CFuncPerformanceMgr* HFUNCPERFMGR;

#endif // __cplusplus

