#pragma once

#ifdef _MSC_VER
#include <WinSock2.h>
#elif __GNUC__
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#error "unknown compiler"
#endif

#include "./lib_svr_def.h"

#ifdef  __cplusplus
extern "C" {
#endif

extern HTCPMANAGER (create_net_tcp)(
	pfn_on_establish func_on_establish,
	pfn_on_terminate func_on_terminate,
	pfn_on_error func_on_error,
	pfn_on_recv func_on_recv,
	pfn_parse_packet func_parse_packet,
	unsigned int max_socket_num,
	unsigned int max_io_thread_num,
	unsigned int max_accept_ex_num
	);

extern void (destroy_net_tcp)(HTCPMANAGER mgr);

extern HSESSION(net_tcp_connect)(
    HTCPMANAGER mgr,
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

extern HLISTENER(net_tcp_listen)(
    HTCPMANAGER mgr,
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

extern bool (net_tcp_send)(HSESSION session, const void* data, unsigned int len);

extern void (net_tcp_close_session)(HSESSION session);

extern void (net_tcp_close_listener)(HLISTENER listener);

extern bool (net_tcp_run)(HTCPMANAGER mgr, unsigned int run_time);

#ifdef _MSC_VER
extern SOCKET (net_tcp_session_socket)(HSESSION session);

extern SOCKET (net_tcp_listener_socket)(HLISTENER listener);
#elif __GNUC__
extern int (net_tcp_session_socket)(HSESSION session);

extern int (net_tcp_listener_socket)(HLISTENER listener);
#else
#error "unknown compiler"
#endif

extern void (net_tcp_set_listener_data)(HLISTENER listener, void* user_data);

extern void (net_tcp_set_session_data)(HSESSION session, void* user_data);

extern void* (net_tcp_get_listener_data)(HLISTENER listener);

extern void* (net_tcp_get_session_data)(HSESSION session);

extern bool (net_tcp_get_peer_ip_port)(HSESSION session, ip_info* info);

extern bool (net_tcp_get_local_ip_port)(HSESSION session, ip_info* info);

extern bool (net_tcp_get_peer_sock_addr)(HSESSION session, addr_info* info);

extern bool (net_tcp_get_local_sock_addr)(HSESSION session, addr_info* info);

extern unsigned int (net_tcp_get_send_free_size)(HSESSION session);

extern unsigned int (net_tcp_get_recv_max_size)(HSESSION session);

extern void (net_tcp_set_send_control)(HSESSION session, unsigned int pkg_size, unsigned int delay_time);


#ifdef  __cplusplus
}
#endif