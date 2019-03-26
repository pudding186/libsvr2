#pragma once
#ifdef _MSC_VER

#include <WinSock2.h>
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern HNETMANAGER(create_iocp_tcp)(
    pfn_on_establish func_on_establish,
    pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error,
    pfn_on_recv func_onrecv,
    pfn_parse_packet func_parse_packet,
    unsigned int max_socket_num,
    unsigned int max_io_thread_num,
    unsigned int max_accept_ex_num);

extern void (destroy_iocp_tcp)(HNETMANAGER mgr);

extern HSESSION(iocp_tcp_connect)(HNETMANAGER mgr,
    const char* ip,
    unsigned short port,
    unsigned int recv_buf_size,
    unsigned int send_buf_size,
    bool reuse_addr,
    const char* bind_ip,
    unsigned short bind_port,
    pfn_on_establish func_on_establish,
    pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error,
    pfn_on_recv func_on_recv,
    pfn_parse_packet func_parse_packet);

extern HLISTENER(iocp_tcp_listen)(HNETMANAGER mgr,
    const char* ip,
    unsigned short port,
    unsigned int recv_buf_size,
    unsigned int send_buf_size,
    bool reuse_addr,
    pfn_on_establish func_on_establish,
    pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error,
    pfn_on_recv func_on_recv,
    pfn_parse_packet func_parse_packet);

extern bool (iocp_tcp_send)(HSESSION socket, const void* data, unsigned int len);

extern void (iocp_tcp_close_session)(HSESSION socket);

extern void (iocp_tcp_close_listener)(HLISTENER listener);

extern bool (iocp_tcp_run)(HNETMANAGER mgr, unsigned int run_time);

extern SOCKET(iocp_tcp_session_socket)(HSESSION socket);

extern SOCKET(iocp_tcp_listener_socket)(HLISTENER listener);

extern void (iocp_tcp_set_listener_data)(HLISTENER listener, void* user_data);

extern void (iocp_tcp_set_session_data)(HSESSION socket, void* user_data);

extern void* (iocp_tcp_get_listener_data)(HLISTENER listener);

extern void* (iocp_tcp_get_session_data)(HSESSION socket);

extern bool (iocp_tcp_get_peer_ip_port)(HSESSION socket, ip_info* info);

extern bool (iocp_tcp_get_local_ip_port)(HSESSION socket, ip_info* info);

extern bool (iocp_tcp_get_peer_sock_addr)(HSESSION socket, addr_info* info);

extern bool (iocp_tcp_get_local_sock_addr)(HSESSION socket, addr_info* info);

extern unsigned int (iocp_tcp_get_send_free_size)(HSESSION socket);

extern void (iocp_tcp_set_send_control)(HSESSION socket, unsigned int pkg_size, unsigned int delay_time);

#ifdef  __cplusplus
}
#endif
#endif