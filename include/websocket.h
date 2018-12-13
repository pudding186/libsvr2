#pragma once
#include "./lib_svr_def.h"
#include "./iocp_tcp.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern HWSMANAGER(create_ws_manager)(HNETMANAGER net_mgr,
        pfn_on_open func_on_open,
        pfn_on_close func_on_close,
        pfn_on_fail func_on_fail,
        pfn_on_message func_on_message,
        pfn_on_http_upgrade func_on_http_upgrade);

extern void (destroy_ws_manager)(HWSMANAGER ws_mgr);

extern HWSLISTENER(ws_listen)(HWSMANAGER mgr,
    const char* ip,
    unsigned short port,
    unsigned int recv_buf_size,
    unsigned int send_buf_size);

extern HWSSESSION ws_connect(HWSMANAGER ws_mgr, 
    const char* uri, 
    const char* extra_headers, 
    unsigned int recv_buf_size, 
    unsigned int send_buf_size);

extern bool (ws_send)(HWSSESSION ws_session, ws_op_code code, const char* data, unsigned int length, bool compress);

#ifdef  __cplusplus
}
#endif