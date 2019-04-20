#pragma once
#ifdef __GNUC__
#include <sys/types.h>
#include <sys/socket.h>
#include "./lib_svr_def.h"

#ifdef  __cplusplus
extern "C" {
#endif

extern HNETMANAGER (create_epoll_tcp)(
    pfn_on_establish func_on_establish, 
    pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error, 
    pfn_on_recv func_on_recv, 
    pfn_parse_packet func_parse_packet,
    unsigned int max_socket_num, 
    unsigned int max_io_thread_num, 
    unsigned int max_accept_ex_num
    );

extern void (destroy_epoll_tcp)(HNETMANAGER mgr);

extern HSESSION (epoll_tcp_connect)(
    HNETMANAGER mgr,
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

extern HLISTENER (epoll_tcp_listen)(
    HNETMANAGER mgr,
    const char * ip,
    unsigned short port,
    unsigned int recv_buf_size,
    unsigned int send_buf_size,
    bool reuse_addr,
    pfn_on_establish func_on_establish,
    pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error,
    pfn_on_recv func_on_recv,
    pfn_parse_packet func_parse_packet);

extern bool (epoll_tcp_send)(HSESSION session, const void* data, unsigned int len);

extern void (epoll_tcp_close_session)(HSESSION session);

extern void (epoll_tcp_close_listener)(HLISTENER listener);

extern bool (epoll_tcp_run)(HNETMANAGER mgr, unsigned int run_time);

extern int (epoll_tcp_session_socket)(HSESSION session);

extern int (epoll_tcp_listener_socket)(HLISTENER listener);

extern void (epoll_tcp_set_listener_data)(HLISTENER listener, void* user_data);

extern void (epoll_tcp_set_session_data)(HSESSION session, void* user_data);

extern void* (epoll_tcp_get_listener_data)(HLISTENER listener);

extern void* (epoll_tcp_get_session_data)(HSESSION session);

extern unsigned int (epoll_tcp_get_send_free_size)(HSESSION session);

extern void (epoll_tcp_set_send_control)(HSESSION session, unsigned int pkg_size, unsigned int delay_time);

#ifdef  __cplusplus
}
#endif
#endif