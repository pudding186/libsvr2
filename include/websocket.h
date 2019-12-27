#pragma once
#include "./lib_svr_def.h"
#include "./net_tcp.h"
#include "./net_ssl.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern HWSMANAGER(create_net_ws)(HTCPMANAGER net_mgr,
        pfn_on_open func_on_open,
        pfn_on_close func_on_close,
        pfn_on_fail func_on_fail,
        pfn_on_message func_on_message,
        pfn_on_http_upgrade func_on_http_upgrade);

extern void (destroy_net_ws)(HWSMANAGER ws_mgr);

extern HWSLISTENER(net_ws_listen)(HWSMANAGER mgr,
    const char* ip,
    unsigned short port,
    unsigned int recv_buf_size,
    unsigned int send_buf_size,
    SSL_CTX* svr_ssl_ctx);

extern HWSSESSION net_ws_connect(HWSMANAGER ws_mgr,
    const char* uri,
    const char* extra_headers,
    unsigned int recv_buf_size,
    unsigned int send_buf_size,
    SSL_CTX* cli_ssl_ctx);

extern bool (net_ws_send_text)(HWSSESSION ws_session, const char* data, unsigned int length, bool compress);

extern bool (net_ws_send_binary)(HWSSESSION ws_session, const char* data, unsigned int length, bool compress);

extern bool (net_ws_send_ping)(HWSSESSION ws_session, const char* message, unsigned int length);

extern bool (net_ws_send_pong)(HWSSESSION ws_session, const char* message, unsigned int length);

extern void (net_ws_close_session)(HWSSESSION ws_session);

extern void (net_ws_close_listener)(HWSLISTENER ws_listener);

extern HSESSION (net_ws_to_tcp_session)(HWSSESSION ws_session);

extern HLISTENER (net_ws_to_tcp_listener)(HWSLISTENER ws_listener);

extern void (net_ws_set_session_data)(HWSSESSION ws_session, void* user_data);

extern void (net_ws_set_listener_data)(HWSLISTENER ws_listener, void* user_data);

extern void* (net_ws_get_session_data)(HWSSESSION ws_session);

extern void* (net_ws_get_listener_data)(HWSLISTENER ws_listener);

#ifdef  __cplusplus
}
#endif