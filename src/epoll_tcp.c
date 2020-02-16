#ifdef __GNUC__
#include <sys/epoll.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "./lib_svr_common_def.h"
#include "../include/utility.hpp"
#include "../include/loop_cache.h"
#include "../include/timer.h"
#include "../include/memory_pool.h"
#include "../include/rb_tree.h"
#include "../include/net_tcp.h"
#include "../include/net_ssl.h"

//socket state
#define SOCKET_STATE_NONE       0
#define SOCKET_STATE_CONNECT    1
#define SOCKET_STATE_LISTEN     2
#define SOCKET_STATE_ESTABLISH  3
#define SOCKET_STATE_ERROR      4
#define SOCKET_STATE_TERMINATE  5
#define SOCKET_STATE_DELETE     6
#define SOCKET_STATE_ACCEPT     7
#define SOCKET_STATE_CLOSE      8


#define NET_EVENT_ESTABLISH     1
#define NET_EVENT_TERMINATE     2
#define NET_EVENT_DATA          3
#define NET_EVENT_SYSTEM_ERROR  4
#define NET_EVENT_MODULE_ERROR  5
#define NET_EVENT_RECV_ACTIVATE 6
#define NET_EVENT_CONNECT_FAIL  7
#define NET_EVENT_SSL_ERROR     8
#define NET_EVENT_ACCEPT        9
#define NET_EVENT_CLOSE         10
#define NET_EVENT_ACCEPT_FAIL   11
#define NET_EVENT_SSL_ESTABLISH 12


#define NET_REQUEST_CONNECT         11
#define NET_REQUEST_SEND            12
#define NET_REQUEST_RECV            13
#define NET_REQUEST_RECV_ACTIVATE   14
#define NET_REQUEST_ACCEPT          15
#define NET_REQUEST_ACCEPT_FAIL     16
#define NET_REQUEST_CLOSE_LISTENER  17
#define NET_REQUEST_TERMINATE       18
#define NET_REQUEST_CLOSE_SOCKET    19
#define NET_REQUEST_SSL_CONNECT     20


#define PROC_NONE               0
#define PROC_LISTENER           1
#define PROC_SOCKET             2

#define MAX_IP_LEN              2048

#define DELAY_CLOSE_SOCKET      15
#define DELAY_SEND_CHECK        5
#define MAX_TRY_CLOSE_SEND      4

#define MAX_RUN_LOOP_CHECK		100

extern void* _main_thread_alloc(mem_unit* unit);
extern void _main_thread_free(mem_unit* unit, void** ptr_mem_unit);

typedef struct st_event_establish
{
    struct st_epoll_tcp_listener* listener;
}event_establish;

typedef struct st_event_data
{
    unsigned int data_len;
}event_data;

typedef struct st_event_system_error
{
    int err_code;
}event_system_error;

typedef struct st_event_ssl_error
{
    unsigned int err_code;
}event_ssl_error;

typedef struct st_event_module_error
{
    net_tcp_error err_code;
}event_module_error;

typedef struct st_event_connect_fail
{
    unsigned int err_code;
}event_connect_fail;

typedef struct st_event_accept_fail
{
    unsigned int err_code;
}event_accept_fail;

typedef struct st_event_terminate
{
    unsigned int unuse;
}event_terminate;

typedef struct st_event_accept
{
    struct st_epoll_tcp_listener*   listener;
    int                             accept_socket_fd;
}event_accept;

typedef struct st_event_close
{
    struct st_epoll_tcp_listener*   listener;
}event_close;

typedef struct st_event_recv_activate
{
    unsigned int unuse;
}event_recv_activate;

typedef struct st_net_event
{
    struct st_epoll_tcp_socket* sock_ptr;
    int                         type;
    union
    {
        event_establish         evt_establish;
        event_data              evt_data;
        event_system_error      evt_system_error;
        event_ssl_error         evt_ssl_error;
        event_module_error      evt_module_error;
        event_terminate         evt_terminate;
        event_connect_fail      evt_connect_fail;
        event_accept            evt_accept;
        event_close             evt_close;
        event_accept_fail       evt_accept_fail;
        event_recv_activate     evt_recv_activate;
    }evt;
}net_event;

typedef struct st_request_accept
{
    unsigned int unuse;
}request_accept;

typedef struct st_request_accept_fail
{
    int accept_fd;
}request_accept_fail;

typedef struct st_request_connect
{
    unsigned int unuse;
}request_connect;

typedef struct st_request_ssl_connect
{
    SSL_CTX* ssl_ctx_data;
}request_ssl_connect;

typedef struct st_request_send
{
    unsigned int unuse;
}request_send;

typedef struct st_request_close_listener
{
    struct st_epoll_tcp_listener* listener;
}request_close_listener;

typedef struct st_request_terminate
{
    net_tcp_error module_error;
    int system_error;
}request_terminate;

typedef struct st_request_recv_activate
{
    unsigned int unuse;
}request_recv_activate;

typedef struct st_net_request
{
    struct st_epoll_tcp_socket* sock_ptr;
    int                         type;
    union
    {
        request_accept          req_accept;
        request_accept_fail     req_accept_fail;
        request_connect         req_connect;
        request_send            req_send;
        request_close_listener  req_close_listener;
        request_terminate       req_terminate;
        request_recv_activate   req_recv_activate;
        request_ssl_connect     req_ssl_connect;
    }req;
}net_request;

typedef struct st_epoll_tcp_listener
{
    void*                           user_data;
    pfn_parse_packet                pkg_parser;
    pfn_on_establish                on_establish;
    pfn_on_terminate                on_terminate;
    pfn_on_error                    on_error;
    pfn_on_recv                     on_recv;

    struct st_epoll_tcp_manager*    mgr;
    struct st_epoll_tcp_proc*       proc;
	SSL_CTX*						svr_ssl_ctx;

    int                             sock_fd;
    long                            state;
    unsigned int                    recv_buf_size;
    unsigned int                    send_buf_size;

    unsigned int                    accept_push;
    unsigned int                    accept_pop;
}epoll_tcp_listener;

typedef struct st_epoll_ssl_data
{
    net_ssl_core    core;
    char*           ssl_recv_buf;
    unsigned int    ssl_recv_buf_size;
    char*           ssl_send_buf;
    unsigned int    ssl_send_buf_size;
    unsigned int    bio_read_left;
    unsigned int    ssl_state;
}epoll_ssl_data;

typedef struct st_epoll_tcp_socket
{
    /* data */
    void*                           user_data;
    pfn_parse_packet                pkg_parser;
    pfn_on_establish                on_establish;
    pfn_on_terminate                on_terminate;
    pfn_on_error                    on_error;
    pfn_on_recv                     on_recv;

    struct st_epoll_tcp_manager*    mgr;
    struct st_epoll_tcp_proc*       proc;
    struct st_epoll_tcp_listener*   listener;
	struct st_epoll_ssl_data*		ssl_data_ptr;

    HLOOPCACHE                      recv_loop_cache;
    HLOOPCACHE                      send_loop_cache;

    HTIMERINFO                      timer_send;
    HTIMERINFO                      timer_close;

	struct sockaddr_in6				local_sockaddr;
	struct sockaddr_in6				peer_sockaddr;
    
    int                             sock_fd;
    long                            state;

    unsigned int                    data_has_recv;

    unsigned int                    send_req;
    unsigned int                    send_ack;

    unsigned int                    data_need_send;
    unsigned int                    data_has_send;
    unsigned int                    data_delay_send_size;

    unsigned char                   try_close_send_count;
    bool                            need_send_active;
    bool                            need_req_close;
    bool                            need_recv_active;

}epoll_tcp_socket;

typedef bool (*pfn_do_net_evt)(struct st_epoll_tcp_proc* proc);

typedef struct st_epoll_tcp_proc
{
    int                             epoll_fd;
    struct epoll_event*             arry_epoll_events;
    int                             size_epoll_events;

    pthread_t                       proc_thread;

    HLOOPCACHE                      list_net_req;
    HLOOPCACHE                      list_net_evt;

    HTIMERMANAGER                   timer_mgr;

    struct st_epoll_tcp_manager*    mgr;

	unsigned int					do_proc_count;

    bool                            is_running;
    

}epoll_tcp_proc;


typedef struct st_epoll_tcp_manager
{
    pfn_on_establish            def_on_establish;
    pfn_on_terminate            def_on_terminate;
    pfn_on_error                def_on_error;
    pfn_on_recv                 def_on_recv;
    pfn_parse_packet            def_parse_packet;

    HMEMORYUNIT                 socket_pool;
    HRBTREE                     memory_mgr;

    unsigned int                cur_proc_idx;
	unsigned int				net_proc_num;
    epoll_tcp_proc**            tcp_proc_array;

    char*                       max_pkg_buf;
    unsigned int                max_pkg_buf_size;
}epoll_tcp_manager;

typedef void* (*pfn_proc_func)(void* arg);

epoll_tcp_proc* _get_idle_net_proc(epoll_tcp_manager* mgr)
{
	epoll_tcp_proc* net_proc = mgr->tcp_proc_array[0];

	unsigned int do_proc_count = mgr->tcp_proc_array[0]->do_proc_count;

	for (unsigned int i = 1; i < mgr->net_proc_num; i++)
	{
		if (do_proc_count > mgr->tcp_proc_array[i]->do_proc_count)
		{
			do_proc_count = mgr->tcp_proc_array[i]->do_proc_count;
			net_proc = mgr->tcp_proc_array[i];
		}
	}

	return net_proc;
}

bool _set_nonblock(int sock_fd)
{
    if (sock_fd == -1)
    {
        return false;
    }

    int flags = fcntl(sock_fd, F_GETFL, 0);

    if (flags < 0)
    {
        return false;
    }

    flags |= O_NONBLOCK;

    if (fcntl(sock_fd, F_SETFL, flags) < 0)
    {
        return false;
    }

    return true;
}

void _epoll_tcp_socket_reset(epoll_tcp_socket* sock_ptr)
{
    sock_ptr->pkg_parser = 0;
    sock_ptr->on_establish = 0;
    sock_ptr->on_terminate = 0;
    sock_ptr->on_error = 0;
    sock_ptr->on_recv = 0;

    sock_ptr->proc = 0;
    sock_ptr->listener = 0;

    sock_ptr->timer_send = 0;
    sock_ptr->timer_close = 0;

    sock_ptr->sock_fd = -1;
    sock_ptr->state = SOCKET_STATE_NONE;

    sock_ptr->data_has_recv = 0;

    sock_ptr->send_req = 0;
    sock_ptr->send_ack = 0;

    sock_ptr->data_need_send = 0;
    sock_ptr->data_has_send = 0;
    sock_ptr->data_delay_send_size = 0;
    sock_ptr->try_close_send_count = 0;
    sock_ptr->need_send_active = true;
    sock_ptr->need_req_close = true;
    sock_ptr->need_recv_active = false;

    sock_ptr->user_data = 0;
    sock_ptr->ssl_data_ptr = 0;
}

bool _epoll_tcp_listener_proc_add(epoll_tcp_proc* proc, epoll_tcp_listener* listener)
{
    if (!_set_nonblock(listener->sock_fd))
    {
        return false;
    }

    struct epoll_event evt;
    evt.events = EPOLLIN;
    evt.data.ptr = listener;

    if (epoll_ctl(proc->epoll_fd, EPOLL_CTL_ADD, listener->sock_fd, &evt) < 0)
    {
        return false;
    }

    return true;
}

void _epoll_tcp_listener_proc_del(epoll_tcp_proc* proc, epoll_tcp_listener* listener)
{
    if (listener->sock_fd == -1)
    {
        return;
    }

    epoll_ctl(proc->epoll_fd, EPOLL_CTL_DEL, listener->sock_fd, 0);
}

bool _epoll_tcp_socket_proc_add(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    if (!_set_nonblock(sock_ptr->sock_fd))
    {
        return false;
    }

    struct epoll_event evt;
    evt.events = EPOLLOUT | EPOLLIN | EPOLLET;
    evt.data.ptr = sock_ptr;

    if (epoll_ctl(proc->epoll_fd, EPOLL_CTL_ADD, sock_ptr->sock_fd, &evt) < 0)
    {
        return false;
    }

    return true;
}

void _epoll_tcp_socket_proc_del(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    if (sock_ptr->sock_fd == -1)
    {
        return;
    }

    epoll_ctl(proc->epoll_fd, EPOLL_CTL_DEL, sock_ptr->sock_fd, 0);
}

void* _epoll_tcp_manager_alloc_memory(epoll_tcp_manager* mgr, unsigned int buffer_size)
{
    HMEMORYUNIT unit;
    HRBNODE memory_node = rb_tree_find_integer(mgr->memory_mgr, buffer_size);

    if (!memory_node)
    {
        unit = create_memory_unit(buffer_size);

        if (!unit)
        {
            return 0;
        }

        rb_tree_insert_integer(mgr->memory_mgr, buffer_size, unit);
    }
    else
    {
        unit = (HMEMORYUNIT)rb_node_value_user(memory_node);
    }

    return memory_unit_alloc(unit);
}

void _epoll_tcp_manager_free_memory(epoll_tcp_manager* mgr, void* mem, unsigned int buffer_size)
{
    HRBNODE memory_node = rb_tree_find_integer(mgr->memory_mgr, buffer_size);

    HMEMORYUNIT check_unit = rb_node_value_user(memory_node);

    memory_unit_free(check_unit, mem);
}

void _epoll_tcp_manager_free_ssl_data(epoll_tcp_manager* mgr, epoll_ssl_data* data)
{
    _epoll_tcp_manager_free_memory(mgr, data, sizeof(epoll_ssl_data) + data->ssl_recv_buf_size + data->ssl_send_buf_size);
}

epoll_ssl_data* _epoll_tcp_manager_alloc_ssl_data(epoll_tcp_manager* mgr, unsigned int ssl_recv_cache_size, unsigned int ssl_send_cache_size)
{
    epoll_ssl_data* data_ptr = _epoll_tcp_manager_alloc_memory(mgr, sizeof(epoll_ssl_data) + ssl_recv_cache_size + ssl_send_cache_size);

    data_ptr->ssl_state = SSL_UN_HAND_SHAKE;

    data_ptr->ssl_recv_buf = ((char*)data_ptr) + sizeof(epoll_ssl_data);
    data_ptr->ssl_send_buf = ((char*)data_ptr) + sizeof(epoll_ssl_data) + ssl_recv_cache_size;

    data_ptr->ssl_recv_buf_size = ssl_recv_cache_size;
    data_ptr->ssl_send_buf_size = ssl_send_cache_size;

    data_ptr->bio_read_left = 0;

    data_ptr->core.ssl = 0;
    data_ptr->core.bio[BIO_RECV] = 0;
    data_ptr->core.bio[BIO_SEND] = 0;

    return data_ptr;
}

epoll_tcp_socket* _epoll_tcp_manager_alloc_socket(epoll_tcp_manager* mgr, unsigned int recv_buf_size, unsigned int send_buf_size)
{
    epoll_tcp_socket* sock_ptr = 0;

    if (recv_buf_size < 1024)
    {
        recv_buf_size = 1024;
    }

    if (send_buf_size < 1024)
    {
        send_buf_size = 1024;
    }

    sock_ptr = _main_thread_alloc(mgr->socket_pool);

    if (sock_ptr)
    {
        if (sock_ptr->mgr != mgr)
        {
            CRUSH_CODE();
        }

        _epoll_tcp_socket_reset(sock_ptr);

        if (!sock_ptr->recv_loop_cache)
        {
            if (sock_ptr->send_loop_cache)
            {
                CRUSH_CODE();
            }

            sock_ptr->recv_loop_cache = create_loop_cache(_epoll_tcp_manager_alloc_memory(mgr, recv_buf_size), recv_buf_size);
            sock_ptr->send_loop_cache = create_loop_cache(_epoll_tcp_manager_alloc_memory(mgr, send_buf_size), send_buf_size);
        }
        else
        {
            if ((unsigned int)loop_cache_size(sock_ptr->recv_loop_cache) != recv_buf_size)
            {
                _epoll_tcp_manager_free_memory(mgr, loop_cache_get_cache(sock_ptr->recv_loop_cache), (unsigned int)loop_cache_size(sock_ptr->recv_loop_cache));
                loop_cache_reset(sock_ptr->recv_loop_cache, recv_buf_size, (char*)_epoll_tcp_manager_alloc_memory(mgr, recv_buf_size));
            }

            if ((unsigned int)loop_cache_size(sock_ptr->send_loop_cache) != send_buf_size)
            {
                _epoll_tcp_manager_free_memory(mgr, loop_cache_get_cache(sock_ptr->send_loop_cache), (unsigned int)loop_cache_size(sock_ptr->send_loop_cache));
                loop_cache_reset(sock_ptr->send_loop_cache, send_buf_size, (char*)_epoll_tcp_manager_alloc_memory(mgr, send_buf_size));
            }
        }

        loop_cache_reinit(sock_ptr->recv_loop_cache);
        loop_cache_reinit(sock_ptr->send_loop_cache);

        return sock_ptr;
    }
    else
    {
        return 0;
    }
}

void _epoll_tcp_manager_free_socket(epoll_tcp_manager* mgr, epoll_tcp_socket* sock_ptr)
{
    void** ptr_mem_unit = memory_unit_get_sign(sock_ptr);
    if (!memory_unit_check_sign(mgr->socket_pool, ptr_mem_unit))
    {
        CRUSH_CODE();
    }
    _main_thread_free(mgr->socket_pool, ptr_mem_unit);
}

bool _epoll_tcp_manager_is_socket(epoll_tcp_manager* mgr, void* ptr)
{
	return memory_unit_check(mgr->socket_pool, ptr);
}

bool _is_ssl_error(int ssl_error)
{
	switch (ssl_error)
	{
	case SSL_ERROR_NONE:
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_WANT_CONNECT:
	case SSL_ERROR_WANT_ACCEPT:
		return false;

	default: return true;
	}
}

void _epoll_tcp_proc_push_evt_establish(epoll_tcp_proc* proc, epoll_tcp_listener* listener, epoll_tcp_socket* sock_ptr)
{
    net_event* evt;
    size_t evt_len = sizeof(net_event);

    loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

    if (evt_len != sizeof(net_event))
    {
        CRUSH_CODE();
    }

    evt->sock_ptr = sock_ptr;
    evt->type = NET_EVENT_ESTABLISH;
    evt->evt.evt_establish.listener = listener;

    loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_evt_ssl_establish(epoll_tcp_proc* proc, epoll_tcp_listener* listener, epoll_tcp_socket* sock_ptr)
{
    net_event* evt;
    size_t evt_len = sizeof(net_event);

    loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

    if (evt_len != sizeof(net_event))
    {
        CRUSH_CODE();
    }

    evt->sock_ptr = sock_ptr;
    evt->type = NET_EVENT_SSL_ESTABLISH;
    evt->evt.evt_establish.listener = listener;

    loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_evt_data(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr, unsigned int data_len)
{
    if (sock_ptr->state == SOCKET_STATE_ESTABLISH)
    {
        net_event* evt;
        size_t evt_len = sizeof(net_event);

        loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

        if (evt_len != sizeof(net_event))
        {
            CRUSH_CODE();
        }

        evt->sock_ptr = sock_ptr;
        evt->type = NET_EVENT_DATA;
        evt->evt.evt_data.data_len = data_len;

        loop_cache_push(proc->list_net_evt, evt_len);
    }
}

void _epoll_tcp_proc_push_evt_system_error(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr, int err_code)
{
    net_event* evt;
    size_t evt_len = sizeof(net_event);

    loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

    if (evt_len != sizeof(net_event))
    {
        CRUSH_CODE();
    }

    evt->sock_ptr = sock_ptr;
    evt->type = NET_EVENT_SYSTEM_ERROR;
    evt->evt.evt_system_error.err_code = err_code;

    loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_evt_ssl_error(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr, int err_code)
{
	net_event* evt;
	size_t evt_len = sizeof(net_event);

	loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

	if (evt_len != sizeof(net_event))
	{
		CRUSH_CODE();
	}

	evt->sock_ptr = sock_ptr;
	evt->type = NET_EVENT_SSL_ERROR;
	evt->evt.evt_ssl_error.err_code = err_code;

	loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_evt_module_error(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr, net_tcp_error err_code)
{
    net_event* evt;
    size_t evt_len = sizeof(net_event);

    loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

    if (evt_len != sizeof(net_event))
    {
        CRUSH_CODE();
    }

    evt->sock_ptr = sock_ptr;
    evt->type = NET_EVENT_MODULE_ERROR;
    evt->evt.evt_module_error.err_code = err_code;

    loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_evt_terminate(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    net_event* evt;
    size_t evt_len = sizeof(net_event);

    loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

    if (evt_len != sizeof(net_event))
    {
        CRUSH_CODE();
    }

    evt->sock_ptr = sock_ptr;
    evt->type = NET_EVENT_TERMINATE;

    loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_evt_connect_fail(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr, int err_code)
{
    net_event* evt;
    size_t evt_len = sizeof(net_event);

    loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

    if (evt_len != sizeof(net_event))
    {
        CRUSH_CODE();
    }

    evt->sock_ptr = sock_ptr;
    evt->type = NET_EVENT_CONNECT_FAIL;
    evt->evt.evt_connect_fail.err_code = err_code;

    loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_evt_accept(epoll_tcp_proc* proc, epoll_tcp_listener* listener, int accetp_socket_fd)
{
    net_event* evt;
    size_t evt_len = sizeof(net_event);

    loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

    if (evt_len != sizeof(net_event))
    {
        CRUSH_CODE();
    }

    evt->sock_ptr = 0;
    evt->type = NET_EVENT_ACCEPT;
    evt->evt.evt_accept.accept_socket_fd = accetp_socket_fd;
    evt->evt.evt_accept.listener = listener;

    loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_evt_accept_fail(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    net_event* evt;
    size_t evt_len = sizeof(net_event);

    loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

    if (evt_len != sizeof(net_event))
    {
        CRUSH_CODE();
    }

    evt->sock_ptr = sock_ptr;
    evt->type = NET_EVENT_ACCEPT_FAIL;

    loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_evt_recv_activate(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    net_event* evt;
    size_t evt_len = sizeof(net_event);

    loop_cache_get_free(proc->list_net_evt, (void**)&evt, &evt_len);

    if (evt_len != sizeof(net_event))
    {
        CRUSH_CODE();
    }

    evt->sock_ptr = sock_ptr;
    evt->type = NET_EVENT_RECV_ACTIVATE;

    loop_cache_push(proc->list_net_evt, evt_len);
}

void _epoll_tcp_proc_push_req_close_listener(epoll_tcp_proc* proc, epoll_tcp_listener* listener)
{
    net_request* req;
    size_t req_len = sizeof(net_request);

    loop_cache_get_free(proc->list_net_req, (void**)&req, &req_len);

    if (req_len != sizeof(net_request))
    {
        CRUSH_CODE();
    }

    req->sock_ptr = 0;
    req->type = NET_REQUEST_CLOSE_LISTENER;
    req->req.req_close_listener.listener = listener;

    loop_cache_push(proc->list_net_req, req_len);
}

void _epoll_tcp_proc_push_req_terminate(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr, net_tcp_error module_error, int system_error)
{
    net_request* req;
    size_t req_len = sizeof(net_request);

    loop_cache_get_free(proc->list_net_req, (void**)&req, &req_len);

    if (req_len != sizeof(net_request))
    {
        CRUSH_CODE();
    }

    req->sock_ptr = sock_ptr;
    req->type = NET_REQUEST_TERMINATE;
    req->req.req_terminate.module_error = module_error;
    req->req.req_terminate.system_error = system_error;

    loop_cache_push(proc->list_net_req, req_len);
}

void _epoll_tcp_proc_push_req_connect(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    net_request* req;
    size_t req_len = sizeof(net_request);

    loop_cache_get_free(proc->list_net_req, (void**)&req, &req_len);

    if (req_len != sizeof(net_request))
    {
        CRUSH_CODE();
    }

    req->sock_ptr = sock_ptr;
    req->type = NET_REQUEST_CONNECT;

    loop_cache_push(proc->list_net_req, req_len);
}

void _epoll_tcp_proc_push_req_ssl_connect(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr, SSL_CTX* ssl_ctx_data)
{
    net_request* req;
    size_t req_len = sizeof(net_request);

    loop_cache_get_free(proc->list_net_req, (void**)&req, &req_len);

    if (req_len != sizeof(net_request))
    {
        CRUSH_CODE();
    }

    req->sock_ptr = sock_ptr;
    req->type = NET_REQUEST_SSL_CONNECT;

    req->req.req_ssl_connect.ssl_ctx_data = ssl_ctx_data;

    loop_cache_push(proc->list_net_req, req_len);
}

void _epoll_tcp_proc_push_req_send(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    net_request* req;
    size_t req_len = sizeof(net_request);

    loop_cache_get_free(proc->list_net_req, (void**)&req, &req_len);

    if (req_len != sizeof(net_request))
    {
        CRUSH_CODE();
    }

    req->sock_ptr = sock_ptr;
    req->type = NET_REQUEST_SEND;

    sock_ptr->send_req++;

    loop_cache_push(proc->list_net_req, req_len);
}

void _epoll_tcp_proc_push_req_accept(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    net_request* req;
    size_t req_len = sizeof(net_request);

    loop_cache_get_free(proc->list_net_req, (void**)&req, &req_len);

    if (req_len != sizeof(net_request))
    {
        CRUSH_CODE();
    }

    req->sock_ptr = sock_ptr;
    req->type = NET_REQUEST_ACCEPT;

    loop_cache_push(proc->list_net_req, req_len);
}

void _epoll_tcp_proc_push_req_accept_fail(epoll_tcp_proc* proc, int accept_fd)
{
    net_request* req;
    size_t req_len = sizeof(net_request);

    loop_cache_get_free(proc->list_net_req, (void**)&req, &req_len);

    if (req_len != sizeof(net_request))
    {
        CRUSH_CODE();
    }

    req->sock_ptr = 0;
    req->type = NET_REQUEST_ACCEPT_FAIL;
    req->req.req_accept_fail.accept_fd = accept_fd;

    loop_cache_push(proc->list_net_req, req_len);
}

void _epoll_tcp_proc_push_req_recv_activate(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    net_request* req;
    size_t req_len = sizeof(net_request);

    loop_cache_get_free(proc->list_net_req, (void**)&req, &req_len);

    if (req_len != sizeof(net_request))
    {
        CRUSH_CODE();
    }

    req->sock_ptr = sock_ptr;
    req->type = NET_REQUEST_RECV_ACTIVATE;

    loop_cache_push(proc->list_net_req, req_len);
}

void _epoll_tcp_proc_push_req_close_socket(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    net_request* req;
    size_t req_len = sizeof(net_request);

    loop_cache_get_free(proc->list_net_req, (void**)&req, &req_len);

    if (req_len != sizeof(net_request))
    {
        CRUSH_CODE();
    }

    req->sock_ptr = sock_ptr;
    req->type = NET_REQUEST_CLOSE_SOCKET;

    sock_ptr->send_req++;
    loop_cache_push(proc->list_net_req, req_len);
}

void _epoll_tcp_socket_get_sockaddr(epoll_tcp_socket* sock_ptr)
{
	if (sock_ptr->sock_fd != -1)
	{
		socklen_t addr_len = sizeof(sock_ptr->local_sockaddr);
		getsockname(sock_ptr->sock_fd, (struct sockaddr*)&sock_ptr->local_sockaddr, &addr_len);

		addr_len = sizeof(sock_ptr->peer_sockaddr);
		getpeername(sock_ptr->sock_fd, (struct sockaddr*)&sock_ptr->peer_sockaddr, &addr_len);
	}
}

void _epoll_tcp_socket_on_timer_send(epoll_tcp_socket* sock_ptr)
{
    if (sock_ptr->state == SOCKET_STATE_ESTABLISH)
    {
        if (sock_ptr->data_has_send != sock_ptr->data_need_send)
        {
            if (sock_ptr->send_req == sock_ptr->send_ack)
            {
                if (sock_ptr->need_send_active)
                {
                    _epoll_tcp_proc_push_req_send(sock_ptr->proc, sock_ptr);
                }
            }
        }
    }
}

void _epoll_tcp_socket_on_timer_close(epoll_tcp_socket* sock_ptr)
{
    switch (sock_ptr->state)
    {
    case SOCKET_STATE_TERMINATE:
    {
        if (sock_ptr->try_close_send_count < MAX_TRY_CLOSE_SEND)
        {
            sock_ptr->try_close_send_count++;

            if (sock_ptr->data_has_send != sock_ptr->data_need_send)
            {
                if (sock_ptr->send_req == sock_ptr->send_ack)
                {
                    if (sock_ptr->need_send_active)
                    {
                        _epoll_tcp_proc_push_req_send(sock_ptr->proc, sock_ptr);
                    }
                }

                return;
            }
        }

        sock_ptr->state = SOCKET_STATE_DELETE;
    }
    break;
    case SOCKET_STATE_DELETE:
    {
        if (sock_ptr->need_req_close)
        {
            _epoll_tcp_proc_push_req_close_socket(sock_ptr->proc, sock_ptr);
            sock_ptr->need_req_close = false;
        }

        if (sock_ptr->send_req != sock_ptr->send_ack)
        {
            return;
        }

        if (sock_ptr->sock_fd != -1)
        {
            close(sock_ptr->sock_fd);
            sock_ptr->sock_fd = -1;
        }

        if (sock_ptr->ssl_data_ptr)
        {
            if (sock_ptr->ssl_data_ptr->core.ssl)
            {
                return;
            }
            else
            {
                _epoll_tcp_manager_free_ssl_data(sock_ptr->mgr, sock_ptr->ssl_data_ptr);
                sock_ptr->ssl_data_ptr = 0;
            }
        }

        timer_del(sock_ptr->timer_close);
        sock_ptr->timer_close = 0;

        _epoll_tcp_manager_free_socket(sock_ptr->mgr, sock_ptr);
    }
    break;
    default:
    {
        CRUSH_CODE();
    }
    }
}

void _epoll_tcp_net_on_timer(HTIMERINFO timer)
{
    epoll_tcp_socket* sock_ptr = (epoll_tcp_socket*)timer_get_data(timer);

    if (!sock_ptr)
    {
        CRUSH_CODE();
    }

    if (sock_ptr->timer_send == timer)
    {
        _epoll_tcp_socket_on_timer_send(sock_ptr);
    }
    else if (sock_ptr->timer_close == timer)
    {
        _epoll_tcp_socket_on_timer_close(sock_ptr);
    }
    else
    {
        CRUSH_CODE();
    }
}

void _epoll_tcp_socket_mod_timer_send(epoll_tcp_socket* sock_ptr, unsigned int elapse)
{
    if (sock_ptr->timer_send)
    {
        timer_mod(sock_ptr->timer_send, elapse, -1, sock_ptr);
        return;
    }

    sock_ptr->timer_send = timer_add(sock_ptr->proc->timer_mgr, elapse, -1, sock_ptr);

    if (!sock_ptr->timer_send)
    {
        CRUSH_CODE();
    }
}

void _epoll_tcp_socket_mod_timer_close(epoll_tcp_socket* sock_ptr, unsigned int elapse)
{
    if (sock_ptr->timer_send)
    {
        timer_del(sock_ptr->timer_send);
        sock_ptr->timer_send = 0;
    }

    if (sock_ptr->timer_close)
    {
        timer_mod(sock_ptr->timer_close, elapse, -1, sock_ptr);
        return;
    }

    sock_ptr->timer_close = timer_add(sock_ptr->proc->timer_mgr, elapse, -1, sock_ptr);

    if (!sock_ptr->timer_close)
    {
        CRUSH_CODE();
    }
}

extern bool init_server_ssl_data(net_ssl_core* core, SSL_CTX* ssl_ctx_data);
extern bool init_client_ssl_data(net_ssl_core* core, SSL_CTX* ssl_ctx_data);
extern void uninit_ssl_data(net_ssl_core* core);
extern bool is_ssl_error(int ssl_error);

void _epoll_tcp_socket_on_accept(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    if (proc != sock_ptr->proc)
    {
        CRUSH_CODE();
    }

	if (_epoll_tcp_socket_proc_add(proc, sock_ptr))
	{
        sock_ptr->state = SOCKET_STATE_ESTABLISH;

        if (sock_ptr->ssl_data_ptr)
        {
            if (!init_server_ssl_data(&sock_ptr->ssl_data_ptr->core, sock_ptr->listener->svr_ssl_ctx))
            {
                sock_ptr->state = SOCKET_STATE_DELETE;
                _epoll_tcp_proc_push_evt_accept_fail(proc, sock_ptr);
                return;
            }
        }

        _epoll_tcp_socket_get_sockaddr(sock_ptr);

        _epoll_tcp_proc_push_evt_establish(proc, sock_ptr->listener, sock_ptr);
	}
	else
	{
		close(sock_ptr->sock_fd);
		sock_ptr->sock_fd = -1;
		sock_ptr->state = SOCKET_STATE_DELETE;
		_epoll_tcp_proc_push_evt_accept_fail(proc, sock_ptr);
	}
}

void _epoll_tcp_socket_on_connect_req(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    int local_sockaddr_len = sizeof(struct sockaddr_in);
    int peer_sockaddr_len = sizeof(struct sockaddr_in);

    struct sockaddr_in6 local_sockaddr = { 0 };
    struct sockaddr_in6 peer_sockaddr = { 0 };

    bool reuse_addr;
    char ip_data[MAX_IP_LEN];
    char bind_ip_data[MAX_IP_LEN];
    size_t ip_len = 0;
    size_t bind_ip_len = 0;
    unsigned short port = 0;
    unsigned short bind_port = 0;

    struct addrinfo hints;
    struct addrinfo* result = 0;
    char sz_port[64];

    if (proc != sock_ptr->proc)
    {
        CRUSH_CODE();
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

	if (!loop_cache_pop_data(sock_ptr->recv_loop_cache, &ip_len, sizeof(ip_len)))
	{
		CRUSH_CODE();
	}

	if (!loop_cache_pop_data(sock_ptr->recv_loop_cache, &port, sizeof(port)))
	{
		CRUSH_CODE();
	}

	if (!loop_cache_pop_data(sock_ptr->recv_loop_cache, ip_data, ip_len))
	{
		CRUSH_CODE();
	}


	if (!loop_cache_pop_data(sock_ptr->recv_loop_cache, &bind_ip_len, sizeof(bind_ip_len)))
	{
		CRUSH_CODE();
	}

	if (bind_ip_len)
	{
		if (!loop_cache_pop_data(sock_ptr->recv_loop_cache, &reuse_addr, sizeof(reuse_addr)))
		{
			CRUSH_CODE();
		}

		if (!loop_cache_pop_data(sock_ptr->recv_loop_cache, &bind_port, sizeof(bind_port)))
		{
			CRUSH_CODE();
		}

		if (!loop_cache_pop_data(sock_ptr->recv_loop_cache, bind_ip_data, bind_ip_len))
		{
			CRUSH_CODE();
		}

		hints.ai_flags = AI_PASSIVE;

		ulltostr(bind_port, sz_port, sizeof(sz_port), 10);

		if (getaddrinfo(bind_ip_data, sz_port, &hints, &result))
		{
			if (result)
			{
				freeaddrinfo(result);
			}

			goto ERROR_DEAL;
		}
		else
		{
			if (result->ai_addrlen >= sizeof(sock_ptr->local_sockaddr))
			{
				freeaddrinfo(result);
				goto ERROR_DEAL;
			}

			memcpy(&sock_ptr->local_sockaddr, result->ai_addr, result->ai_addrlen);

			freeaddrinfo(result);
			result = 0;
		}
	}

	if (ip_len <= 0)
	{
		CRUSH_CODE();
	}

    {
        hints.ai_flags = 0;
        ulltostr(port, sz_port, sizeof(sz_port), 10);

        if (getaddrinfo(ip_data, sz_port, &hints, &result))
        {
            if (result)
            {
                freeaddrinfo(result);
            }

            goto ERROR_DEAL;
        }
        else
        {
            if (result->ai_addrlen >= sizeof(peer_sockaddr))
            {
                freeaddrinfo(result);
                goto ERROR_DEAL;
            }

            memcpy(&peer_sockaddr, result->ai_addr, result->ai_addrlen);
            freeaddrinfo(result);
            result = 0;
        }
    }

    if (bind_ip_len)
    {
        if (local_sockaddr.sin6_family != peer_sockaddr.sin6_family)
        {
            goto ERROR_DEAL;
        }
    }

    sock_ptr->sock_fd = socket(peer_sockaddr.sin6_family, SOCK_STREAM, IPPROTO_TCP);
    if (-1 == sock_ptr->sock_fd)
    {
        goto ERROR_DEAL;
    }

    if (reuse_addr)
    {
        int reuse = 1;
        if (setsockopt(sock_ptr->sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
        {
            goto ERROR_DEAL;
        }
    }

    if (peer_sockaddr.sin6_family == AF_INET6)
    {
        peer_sockaddr_len = sizeof(struct sockaddr_in6);
        local_sockaddr_len = sizeof(struct sockaddr_in6);
    }

    if (!bind_ip_len)
    {
        if (peer_sockaddr.sin6_family == AF_INET)
        {
            struct sockaddr_in* addr = (struct sockaddr_in*)&local_sockaddr;
            addr->sin_family = AF_INET;
            addr->sin_addr.s_addr = htonl(INADDR_ANY);
            addr->sin_port = htons(0);
        }
        else
        {
            local_sockaddr.sin6_family = AF_INET6;
            local_sockaddr.sin6_addr = in6addr_any;
            local_sockaddr.sin6_port = htons(0);
        }
    }

    if (-1 == bind(sock_ptr->sock_fd, (struct sockaddr*)&local_sockaddr, local_sockaddr_len))
    {
        goto ERROR_DEAL;
    }

    if (!_epoll_tcp_socket_proc_add(proc, sock_ptr))
    {
        goto ERROR_DEAL;
    }

    if (-1 == connect(sock_ptr->sock_fd, (struct sockaddr*)&peer_sockaddr, peer_sockaddr_len))
    {
        if (errno != EINPROGRESS)
        {
            _epoll_tcp_socket_proc_del(proc, sock_ptr);
            goto ERROR_DEAL;
        }
    }

    return;

ERROR_DEAL:
    if (sock_ptr->sock_fd != -1)
    {
        close(sock_ptr->sock_fd);
        sock_ptr->sock_fd = -1;
    }
    sock_ptr->state = SOCKET_STATE_DELETE;

    _epoll_tcp_proc_push_evt_connect_fail(proc, sock_ptr, errno);
}

void _epoll_tcp_socket_on_ssl_connect_req(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr, SSL_CTX* ssl_ctx_data)
{
    if (!init_client_ssl_data(&sock_ptr->ssl_data_ptr->core, ssl_ctx_data))
    {
        if (sock_ptr->sock_fd != -1)
        {
            close(sock_ptr->sock_fd);
            sock_ptr->sock_fd = -1;
        }
        sock_ptr->state = SOCKET_STATE_DELETE;

        _epoll_tcp_proc_push_evt_connect_fail(proc, sock_ptr, 0);

        return;
    }

    _epoll_tcp_socket_on_connect_req(proc, sock_ptr);
}

void _epoll_tcp_socket_on_close(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    if (sock_ptr->ssl_data_ptr)
    {
        uninit_ssl_data(&sock_ptr->ssl_data_ptr->core);
    }

    if (sock_ptr->sock_fd != -1)
    {
        _epoll_tcp_socket_proc_del(proc, sock_ptr);
        shutdown(sock_ptr->sock_fd, SHUT_RDWR);
        close(sock_ptr->sock_fd);
        sock_ptr->sock_fd = -1;
    }

    sock_ptr->send_ack++;
}

void _epoll_tcp_socket_on_terminate(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr, net_tcp_error module_error, int system_error)
{
    switch (module_error)
    {
    case error_system:
    {
        sock_ptr->state = SOCKET_STATE_DELETE;

        _epoll_tcp_proc_push_evt_system_error(proc, sock_ptr, system_error);
    }
    break;
    case error_send_overflow:
    case error_recv_overflow:
    case error_packet:
    {
        _epoll_tcp_proc_push_evt_module_error(proc, sock_ptr, module_error);
    }
    break;
    case error_ssl:
	{
		sock_ptr->state = SOCKET_STATE_DELETE;

		_epoll_tcp_proc_push_evt_ssl_error(proc, sock_ptr, system_error);
	}
    break;
    case error_ok:
    {
        _epoll_tcp_proc_push_evt_terminate(proc, sock_ptr);
    }
    break;
    default:
    {
        CRUSH_CODE();
    }
    }
}

void _epoll_tcp_listener_close(epoll_tcp_listener* listener)
{
    if (__sync_bool_compare_and_swap(&listener->state, SOCKET_STATE_LISTEN, SOCKET_STATE_CLOSE))
    {
        _epoll_tcp_proc_push_req_close_listener(listener->proc, listener);
    }
}

void _epoll_tcp_socket_close(epoll_tcp_socket* sock_ptr, net_tcp_error error, int sys_error, bool epoll_proc)
{
    if (__sync_bool_compare_and_swap(&sock_ptr->state, SOCKET_STATE_ESTABLISH, SOCKET_STATE_TERMINATE))
    {
        if (epoll_proc)
        {
            _epoll_tcp_socket_on_terminate(sock_ptr->proc, sock_ptr, error, sys_error);
        }
        else
        {
            _epoll_tcp_proc_push_req_terminate(sock_ptr->proc, sock_ptr, error, sys_error);
        }
    }
}

void _epoll_tcp_socket_on_send(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    if (sock_ptr->state != SOCKET_STATE_ESTABLISH &&
        sock_ptr->state != SOCKET_STATE_TERMINATE)
    {
        return;
    }

    size_t send_len = 0;
    char* send_ptr = 0;

    loop_cache_get_data(sock_ptr->send_loop_cache, (void**)&send_ptr, &send_len);

    while (send_len)
    {
        int data_send = send(sock_ptr->sock_fd, send_ptr, send_len, 0);

        if (data_send > 0)
        {
            sock_ptr->data_has_send += data_send;
            loop_cache_pop(sock_ptr->send_loop_cache, data_send);
        }
        else if (data_send < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else if (errno == EAGAIN ||
                errno == EWOULDBLOCK)
            {
                sock_ptr->need_send_active = false;
                return;
            }
            else
            {
                _epoll_tcp_socket_close(sock_ptr, error_system, errno, true);
                return;
            }
        }
        else
        {
            CRUSH_CODE();
        }

        send_len = 0;
        send_ptr = 0;

        loop_cache_get_data(sock_ptr->send_loop_cache, (void**)&send_ptr, &send_len);
    }

    sock_ptr->need_send_active = true;
}

void _epoll_tcp_socket_on_recv(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    if (sock_ptr->state != SOCKET_STATE_ESTABLISH)
    {
        return;
    }

    if (sock_ptr->need_recv_active)
    {
        return;
    }

    char* recv_ptr = 0;
    size_t recv_len = 0;
    unsigned data_push_len = 0;

    loop_cache_get_free(sock_ptr->recv_loop_cache, (void**)&recv_ptr, &recv_len);

    while (recv_len)
    {
        int data_recv = recv(sock_ptr->sock_fd, recv_ptr, recv_len, 0);

        if (data_recv > 0)
        {
            data_push_len += data_recv;
            loop_cache_push(sock_ptr->recv_loop_cache, data_recv);
        }
        else if (data_recv < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else if (errno == EAGAIN ||
                errno == EWOULDBLOCK)
            {
                if (data_push_len)
                {
                    _epoll_tcp_proc_push_evt_data(proc, sock_ptr, data_push_len);
                }
                return;
            }
            else
            {
                if (data_push_len)
                {
                    _epoll_tcp_proc_push_evt_data(proc, sock_ptr, data_push_len);
                }
                _epoll_tcp_socket_close(sock_ptr, error_system, errno, true);
                return;
            }
        }
        else
        {
            if (data_push_len)
            {
                _epoll_tcp_proc_push_evt_data(proc, sock_ptr, data_push_len);
            }
            sock_ptr->try_close_send_count = MAX_TRY_CLOSE_SEND;
            _epoll_tcp_socket_close(sock_ptr, error_ok, 0, true);
            return;
        }

        recv_len = 0;
        recv_ptr = 0;

        loop_cache_get_free(sock_ptr->recv_loop_cache, (void**)&recv_ptr, &recv_len);
    }

    if (data_push_len)
    {
        _epoll_tcp_proc_push_evt_data(proc, sock_ptr, data_push_len);
    }

    sock_ptr->need_recv_active = true;
    _epoll_tcp_proc_push_evt_recv_activate(proc, sock_ptr);
}

void _epoll_tcp_socket_on_ssl_send(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    int bio_ret = 0;
    int ssl_ret;

    net_tcp_error error_type = error_none;
    int error_code = 0;

    if (sock_ptr->state != SOCKET_STATE_ESTABLISH &&
        sock_ptr->state != SOCKET_STATE_TERMINATE)
    {
        return;
    }


    char* data_ptr = 0;
    size_t data_len = 0;

    loop_cache_get_data(sock_ptr->send_loop_cache, (void**)&data_ptr, &data_len);

    while (data_len)
    {
        ssl_ret = SSL_write(sock_ptr->ssl_data_ptr->core.ssl, data_ptr, (int)data_len);

        if (ssl_ret > 0)
        {
            if (!loop_cache_pop(sock_ptr->send_loop_cache, ssl_ret))
            {
                CRUSH_CODE();
            }
            sock_ptr->data_has_send += ssl_ret;
        }
        else
        {
            error_code = SSL_get_error(sock_ptr->ssl_data_ptr->core.ssl, ssl_ret);
            if (is_ssl_error(error_code))
            {
                error_type = error_ssl;
                error_code = ERR_get_error();
            }
            break;
        }

        data_ptr = 0;
        data_len = 0;
        loop_cache_get_data(sock_ptr->send_loop_cache, (void**)&data_ptr, &data_len);
    }

    sock_ptr->need_send_active = true;

    for (;;)
    {
        if (sock_ptr->ssl_data_ptr->ssl_send_buf_size - sock_ptr->ssl_data_ptr->bio_read_left > 0)
        {
            if (BIO_pending(sock_ptr->ssl_data_ptr->core.bio[BIO_SEND]))
            {
                bio_ret = BIO_read(
                    sock_ptr->ssl_data_ptr->core.bio[BIO_SEND],
                    sock_ptr->ssl_data_ptr->ssl_send_buf +
                    sock_ptr->ssl_data_ptr->bio_read_left,
                    sock_ptr->ssl_data_ptr->ssl_send_buf_size -
                    sock_ptr->ssl_data_ptr->bio_read_left);

                if (bio_ret > 0)
                {
                    sock_ptr->ssl_data_ptr->bio_read_left += bio_ret;
                }
                else
                {
                    CRUSH_CODE();
                }
            }
        }

        if (sock_ptr->ssl_data_ptr->bio_read_left > 0)
        {
            int data_send = send(sock_ptr->sock_fd, sock_ptr->ssl_data_ptr->ssl_send_buf, sock_ptr->ssl_data_ptr->bio_read_left, 0);

            if (data_send > 0)
            {
                sock_ptr->ssl_data_ptr->bio_read_left -= data_send;

                if (sock_ptr->ssl_data_ptr->bio_read_left > 0)
                {
                    memcpy(
                        sock_ptr->ssl_data_ptr->ssl_send_buf,
                        sock_ptr->ssl_data_ptr->ssl_send_buf + data_send,
                        sock_ptr->ssl_data_ptr->bio_read_left);
                }
            }
            else if (data_send < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else if (errno == EAGAIN ||
                    errno == EWOULDBLOCK)
                {
                    sock_ptr->need_send_active = false;
                    break;
                }
                else
                {
                    error_type = error_system;
                    error_code = errno;
                    break;
                }
            }
            else
            {
                CRUSH_CODE();
            }
        }
        else
        {
            break;
        }
    }

    if (error_type != error_none)
    {
        _epoll_tcp_socket_close(sock_ptr, error_type, error_code, true);
        return;
    }
}

void _epoll_tcp_socket_on_ssl_recv(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    int bio_ret;
    int ssl_ret;

    net_tcp_error error_type = error_none;
    int error_code = 0;

    unsigned int decrypt_data_push_len = 0;

    bool recv_need_reactive = true;

    if (sock_ptr->state != SOCKET_STATE_ESTABLISH)
    {
        return;
    }

    if (sock_ptr->need_recv_active)
    {
        return;
    }

    for (;;)
    {
        int data_recv = recv(
            sock_ptr->sock_fd,
            sock_ptr->ssl_data_ptr->ssl_recv_buf,
            sock_ptr->ssl_data_ptr->ssl_recv_buf_size, 0);

        if (data_recv > 0)
        {
            bio_ret = BIO_write(
                sock_ptr->ssl_data_ptr->core.bio[BIO_RECV],
                sock_ptr->ssl_data_ptr->ssl_recv_buf,
                data_recv);

            if (bio_ret <= 0)
            {
                CRUSH_CODE();
            }
            else
            {
                if (bio_ret != data_recv)
                {
                    CRUSH_CODE();
                }
            }
        }
        else if (data_recv < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else if (errno == EAGAIN ||
                errno == EWOULDBLOCK)
            {
                recv_need_reactive = false;
                break;
            }
            else
            {
                error_type = error_system;
                error_code = errno;
                break;
            }
        }
        else
        {
            sock_ptr->try_close_send_count = MAX_TRY_CLOSE_SEND;
            error_type = error_ok;
            error_code = 0;
            break;
        }
    }

    char* free_data_ptr = 0;
    size_t free_data_len = 0;

    loop_cache_get_free(sock_ptr->recv_loop_cache, (void**)&free_data_ptr, &free_data_len);

    while (free_data_len)
    {
        ssl_ret = SSL_read(sock_ptr->ssl_data_ptr->core.ssl, free_data_ptr, (int)free_data_len);
        if (ssl_ret > 0)
        {
            if (!loop_cache_push(sock_ptr->recv_loop_cache, ssl_ret))
            {
                CRUSH_CODE();
            }

            decrypt_data_push_len += ssl_ret;
        }
        else
        {
            error_code = SSL_get_error(sock_ptr->ssl_data_ptr->core.ssl, ssl_ret);

            if (is_ssl_error(error_code))
            {
                error_type = error_ssl;
                error_code = ERR_get_error();
            }

            break;
        }


        free_data_ptr = 0;
        free_data_len = 0;
        loop_cache_get_free(sock_ptr->recv_loop_cache, (void**)&free_data_ptr, &free_data_len);
    }

    if (error_type != error_none)
    {
        _epoll_tcp_socket_close(sock_ptr, error_type, error_code, true);
        return;
    }

    if (sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE)
    {
        if (decrypt_data_push_len)
        {
            _epoll_tcp_proc_push_evt_data(proc, sock_ptr, decrypt_data_push_len);
        }
    }
    else
    {
        _epoll_tcp_socket_on_ssl_send(proc, sock_ptr);

        if (SSL_is_init_finished(sock_ptr->ssl_data_ptr->core.ssl))
        {
            sock_ptr->ssl_data_ptr->ssl_state = SSL_HAND_SHAKE;
            _epoll_tcp_proc_push_evt_ssl_establish(proc, sock_ptr->listener, sock_ptr);

            if (decrypt_data_push_len)
            {
                _epoll_tcp_proc_push_evt_data(proc, sock_ptr, decrypt_data_push_len);
            }
        }
    }

    if (!free_data_len || recv_need_reactive)
    {
        sock_ptr->need_recv_active = true;
        _epoll_tcp_proc_push_evt_recv_activate(proc, sock_ptr);
    }
}

void _epoll_tcp_socket_on_connect(epoll_tcp_proc* proc, epoll_tcp_socket* sock_ptr)
{
    int opt_val = 0;
    socklen_t opt_len = sizeof(opt_val);

    if (getsockopt(sock_ptr->sock_fd, SOL_SOCKET, SO_ERROR, &opt_val, &opt_len) < 0)
    {
        opt_val = errno;
    }

    if (opt_val)
    {
        sock_ptr->state = SOCKET_STATE_DELETE;
        _epoll_tcp_proc_push_evt_connect_fail(proc, sock_ptr, opt_val);
        return;
    }

    sock_ptr->state = SOCKET_STATE_ESTABLISH;
    _epoll_tcp_socket_get_sockaddr(sock_ptr);
    _epoll_tcp_proc_push_evt_establish(proc, 0, sock_ptr);

    if (sock_ptr->ssl_data_ptr)
    {
        _epoll_tcp_socket_on_ssl_recv(proc, sock_ptr);
    }
}

void _epoll_tcp_listener_on_accept(epoll_tcp_proc* proc, epoll_tcp_listener* listener)
{
    int accept_sock_fd = accept(listener->sock_fd, 0, 0);

    if (accept_sock_fd > 0)
    {
        listener->accept_push++;
        _epoll_tcp_proc_push_evt_accept(proc, listener, accept_sock_fd);
    }
}

//void _epoll_tcp_listener_on_accept(epoll_tcp_proc* proc, epoll_tcp_listener* listener)
//{
//	int accept_sock_fd = -1;
//
//	for (;;)
//	{
//		accept_sock_fd = accept(listener->sock_fd, 0, 0);
//
//		if (accept_sock_fd == -1)
//		{
//            if (errno == EINTR)
//            {
//                continue;
//            }
//			return;
//		}
//		else
//		{
//			listener->accept_push++;
//			_epoll_tcp_proc_push_evt_accept(proc, listener, accept_sock_fd);
//		}
//	}
//}

void _epoll_tcp_listener_on_close(epoll_tcp_proc* proc, epoll_tcp_listener* listener)
{
	_epoll_tcp_listener_proc_del(proc, listener);
	close(listener->sock_fd);
	listener->sock_fd = -1;
	listener->state = SOCKET_STATE_DELETE;
}

unsigned int _do_epoll_evt(epoll_tcp_proc* proc, int time_out)
{
	unsigned int do_epoll_evt_count = 0;

	int fd_num = epoll_wait(proc->epoll_fd, proc->arry_epoll_events, proc->size_epoll_events, time_out);

	if (fd_num < 0)
	{
		return do_epoll_evt_count;
	}

	for (int i = 0; i < fd_num; i++)
	{
		struct epoll_event* evt = &proc->arry_epoll_events[i];
		epoll_tcp_socket* sock_ptr = (epoll_tcp_socket*)evt->data.ptr;

		if (!_epoll_tcp_manager_is_socket(proc->mgr, sock_ptr))
		{
			sock_ptr = 0;
		}

        if (evt->events & EPOLLOUT)
        {
            do_epoll_evt_count++;

            if (!sock_ptr)
            {
                CRUSH_CODE();
            }

            if (sock_ptr->state == SOCKET_STATE_CONNECT)
            {
                _epoll_tcp_socket_on_connect(proc, sock_ptr);
            }
            else
            {
                if (sock_ptr->ssl_data_ptr)
                {
                    _epoll_tcp_socket_on_ssl_send(proc, sock_ptr);
                }
                else
                {
                    _epoll_tcp_socket_on_send(proc, sock_ptr);
                }
            }
        }

		if (evt->events & EPOLLIN)
		{
			do_epoll_evt_count++;

			if (sock_ptr)
			{
                if (sock_ptr->ssl_data_ptr)
                {
                    _epoll_tcp_socket_on_ssl_recv(proc, sock_ptr);
                }
                else
                {
                    _epoll_tcp_socket_on_recv(proc, sock_ptr);
                }
			}
			else
			{
				_epoll_tcp_listener_on_accept(proc, (epoll_tcp_listener*)evt->data.ptr);
			}
		}

		if (evt->events & EPOLLERR)
		{
			do_epoll_evt_count++;
			if (sock_ptr)
			{
				if (sock_ptr->state == SOCKET_STATE_CONNECT)
				{
					sock_ptr->state = SOCKET_STATE_DELETE;
					_epoll_tcp_proc_push_evt_connect_fail(proc, sock_ptr, errno);
				}
				else
				{
					_epoll_tcp_socket_close(sock_ptr, error_system, errno, true);
				}
			}
		}
	}

	return do_epoll_evt_count;
}

unsigned int _do_net_req(epoll_tcp_proc* proc)
{
	unsigned int do_req_count = 0;
	net_request* req;
	size_t req_len = sizeof(net_request);

	loop_cache_get_data(proc->list_net_req, (void**)&req, &req_len);

	while (req_len == sizeof(net_request))
	{
		do_req_count++;

		switch (req->type)
		{
		case NET_REQUEST_SEND:
		{
			req->sock_ptr->send_ack++;

            if (req->sock_ptr->ssl_data_ptr)
            {
                _epoll_tcp_socket_on_ssl_send(proc, req->sock_ptr);
            }
            else
            {
                _epoll_tcp_socket_on_send(proc, req->sock_ptr);
            }
		}
		break;
		case NET_REQUEST_ACCEPT:
		{
			_epoll_tcp_socket_on_accept(proc, req->sock_ptr);
		}
		break;
		case NET_REQUEST_TERMINATE:
		{
			_epoll_tcp_socket_on_terminate(proc, req->sock_ptr,
				req->req.req_terminate.module_error,
				req->req.req_terminate.system_error);
		}
		break;
		case NET_REQUEST_CLOSE_SOCKET:
		{
			_epoll_tcp_socket_on_close(proc, req->sock_ptr);
		}
		break;
		case NET_REQUEST_CONNECT:
		{
			_epoll_tcp_socket_on_connect_req(proc, req->sock_ptr);
		}
		break;
        case NET_REQUEST_SSL_CONNECT:
        {
            _epoll_tcp_socket_on_ssl_connect_req(proc, req->sock_ptr, req->req.req_ssl_connect.ssl_ctx_data);
        }
        break;
		case NET_REQUEST_RECV_ACTIVATE:
		{
            req->sock_ptr->need_recv_active = false;

            if (req->sock_ptr->ssl_data_ptr)
            {
                _epoll_tcp_socket_on_ssl_recv(proc, req->sock_ptr);
            }
            else
            {
                _epoll_tcp_socket_on_recv(proc, req->sock_ptr);
            }
		}
		break;
		case NET_REQUEST_CLOSE_LISTENER:
		{
			_epoll_tcp_listener_on_close(proc, req->req.req_close_listener.listener);
		}
		break;
		case NET_REQUEST_ACCEPT_FAIL:
		{
			close(req->req.req_accept_fail.accept_fd);
		}
		break;
		default:
		{
			CRUSH_CODE();
		}
		}

		if (!loop_cache_pop(proc->list_net_req, sizeof(net_request)))
		{
			CRUSH_CODE();
		}

		loop_cache_get_data(proc->list_net_req, (void**)&req, &req_len);
	}

	return do_req_count;
}

bool _do_net_evt(epoll_tcp_proc* proc)
{
	timer_update(proc->timer_mgr, 0);

	net_event* evt;
	size_t evt_len = sizeof(net_event);

	loop_cache_get_data(proc->list_net_evt, (void**)&evt, &evt_len);

	if (evt_len < sizeof(net_event))
	{
		return false;
	}

	epoll_tcp_socket* sock_ptr = evt->sock_ptr;

	switch (evt->type)
	{
	case NET_EVENT_DATA:
	{
		char* data_ptr = 0;
		size_t data_len;
		unsigned int parser_len = 0;

		sock_ptr->data_has_recv += evt->evt.evt_data.data_len;

		data_len = sock_ptr->data_has_recv;

		loop_cache_get_data(sock_ptr->recv_loop_cache, (void**)&data_ptr, &data_len);

		if ((unsigned int)data_len < sock_ptr->data_has_recv)
		{
			if (sock_ptr->data_has_recv > proc->mgr->max_pkg_buf_size)
			{
				for (;;)
				{
					proc->mgr->max_pkg_buf_size += 1024;

					if (proc->mgr->max_pkg_buf_size > sock_ptr->data_has_recv)
					{
						free(proc->mgr->max_pkg_buf);
						proc->mgr->max_pkg_buf = (char*)malloc(proc->mgr->max_pkg_buf_size);
						break;
					}
				}
			}

			if (!loop_cache_copy_data(sock_ptr->recv_loop_cache, proc->mgr->max_pkg_buf, sock_ptr->data_has_recv))
			{
				CRUSH_CODE();
			}

			data_ptr = proc->mgr->max_pkg_buf;
		}

		while (sock_ptr->data_has_recv)
		{
			unsigned int pkg_len = 0;
			if (sock_ptr->pkg_parser)
			{
				pkg_len = sock_ptr->pkg_parser(sock_ptr, data_ptr, sock_ptr->data_has_recv);
			}
			else
			{
				pkg_len = sock_ptr->data_has_recv;
			}


			if (pkg_len > 0)
			{
				if (pkg_len > sock_ptr->data_has_recv)
				{
					_epoll_tcp_socket_close(sock_ptr, error_packet, 0, false);
					break;
				}

				sock_ptr->on_recv(sock_ptr, data_ptr, pkg_len);

				data_ptr += pkg_len;
				sock_ptr->data_has_recv -= pkg_len;
				parser_len += pkg_len;
			}
			else
			{
				break;
			}
		}

		if (parser_len)
		{
			if (!loop_cache_pop(sock_ptr->recv_loop_cache, parser_len))
			{
				CRUSH_CODE();
			}
		}
	}
	break;
	case NET_EVENT_ESTABLISH:
	{
		_epoll_tcp_socket_mod_timer_send(sock_ptr, DELAY_SEND_CHECK);

        if (!sock_ptr->ssl_data_ptr)
        {
            sock_ptr->on_establish(evt->evt.evt_establish.listener, sock_ptr);
        }
	}
	break;
    case NET_EVENT_SSL_ESTABLISH:
    {
        if (!sock_ptr->ssl_data_ptr)
        {
            CRUSH_CODE();
        }

        sock_ptr->on_establish(evt->evt.evt_establish.listener, sock_ptr);
    }
    break;
	case NET_EVENT_MODULE_ERROR:
	{
        if ((!sock_ptr->ssl_data_ptr) || (sock_ptr->ssl_data_ptr && sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE))
        {
            sock_ptr->on_error(sock_ptr, evt->evt.evt_module_error.err_code, 0);
            sock_ptr->on_terminate(sock_ptr);
        }

		_epoll_tcp_socket_mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
	}
	break;
	case NET_EVENT_SYSTEM_ERROR:
	{
        if ((!sock_ptr->ssl_data_ptr) || (sock_ptr->ssl_data_ptr && sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE))
        {
            sock_ptr->on_error(sock_ptr, error_system, evt->evt.evt_system_error.err_code);
            sock_ptr->on_terminate(sock_ptr);
        }

		_epoll_tcp_socket_mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
	}
	break;
    case NET_EVENT_SSL_ERROR:
    {
        if ((!sock_ptr->ssl_data_ptr) || (sock_ptr->ssl_data_ptr && sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE))
        {
            sock_ptr->on_error(sock_ptr, error_ssl, evt->evt.evt_system_error.err_code);
            sock_ptr->on_terminate(sock_ptr);
        }

        _epoll_tcp_socket_mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
    }
    break;
	case NET_EVENT_TERMINATE:
	{
        if ((!sock_ptr->ssl_data_ptr) || (sock_ptr->ssl_data_ptr && sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE))
        {
            sock_ptr->on_terminate(sock_ptr);
        }

		_epoll_tcp_socket_mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
	}
	break;
	case NET_EVENT_CONNECT_FAIL:
	{
		sock_ptr->on_error(sock_ptr, error_connect_fail, evt->evt.evt_connect_fail.err_code);

		_epoll_tcp_socket_mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
	}
	break;
	case NET_EVENT_RECV_ACTIVATE:
	{
		if (sock_ptr->state == SOCKET_STATE_ESTABLISH)
		{
			if (loop_cache_free_size(sock_ptr->recv_loop_cache))
			{
				_epoll_tcp_proc_push_req_recv_activate(proc, sock_ptr);
			}
			else
			{
				_epoll_tcp_socket_close(sock_ptr, error_recv_overflow, 0, false);
			}
		}
	}
	break;
	case NET_EVENT_ACCEPT:
	{
		epoll_tcp_listener* listener = evt->evt.evt_accept.listener;

		epoll_tcp_socket* sock_ptr = _epoll_tcp_manager_alloc_socket(
			proc->mgr,
			listener->recv_buf_size,
			listener->send_buf_size);
		if (sock_ptr)
		{
			sock_ptr->sock_fd = evt->evt.evt_accept.accept_socket_fd;

			sock_ptr->on_establish = listener->on_establish;

			sock_ptr->on_terminate = listener->on_terminate;

			sock_ptr->on_error = listener->on_error;

			sock_ptr->on_recv = listener->on_recv;

			sock_ptr->pkg_parser = listener->pkg_parser;

			sock_ptr->proc = _get_idle_net_proc(proc->mgr);

			sock_ptr->listener = listener;

            if (listener->svr_ssl_ctx)
            {
                sock_ptr->ssl_data_ptr = _epoll_tcp_manager_alloc_ssl_data(listener->mgr, DEF_SSL_RECV_CACHE_SIZE, DEF_SSL_SEND_CACHE_SIZE);
            }
            else
            {
                sock_ptr->ssl_data_ptr = 0;
            }

			_epoll_tcp_proc_push_req_accept(sock_ptr->proc, sock_ptr);
		}
		else
		{
			_epoll_tcp_proc_push_req_accept_fail(listener->proc, evt->evt.evt_accept.accept_socket_fd);
		}
		listener->accept_pop++;
	}
	break;
	case NET_EVENT_ACCEPT_FAIL:
	{
		_epoll_tcp_socket_mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
	}
	break;
	default:
	{
		CRUSH_CODE();
	}
	break;
	}

	if (!loop_cache_pop(proc->list_net_evt, sizeof(net_event)))
	{
		CRUSH_CODE();
	}

	return true;
}

void* _epoll_tcp_net_proc_func(void* arg)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);

    sigset_t new_set, old_set;
    sigemptyset(&new_set);
    sigemptyset(&old_set);
    sigaddset(&new_set, SIGHUP);
    sigaddset(&new_set, SIGINT);
    sigaddset(&new_set, SIGQUIT);
    sigaddset(&new_set, SIGTERM);
    sigaddset(&new_set, SIGUSR1);
    sigaddset(&new_set, SIGUSR2);
    sigaddset(&new_set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &new_set, &old_set);

	epoll_tcp_proc* proc = (epoll_tcp_proc*)arg;

	unsigned int cur_do_proc_count = 0;
	unsigned int run_loop_check = 0;

	while (proc->is_running)
	{
		cur_do_proc_count += _do_net_req(proc);
		cur_do_proc_count += _do_epoll_evt(proc, 3);

		++run_loop_check;

		if (run_loop_check >= MAX_RUN_LOOP_CHECK)
		{
			proc->do_proc_count = cur_do_proc_count;
			run_loop_check = 0;
			cur_do_proc_count = 0;
		}
	}

	return 0;
}

void _uninit_epoll_tcp_proc(epoll_tcp_proc* proc)
{
    if (proc->proc_thread)
    {
        proc->is_running = false;
        pthread_join(proc->proc_thread, 0);
    }

    if (proc->timer_mgr)
    {
        destroy_timer_manager(proc->timer_mgr);
    }

    if (proc->list_net_evt)
    {
        destroy_loop_cache(proc->list_net_evt);
    }

    if (proc->list_net_req)
    {
        destroy_loop_cache(proc->list_net_req);
    }

    if (proc->arry_epoll_events)
    {
        free(proc->arry_epoll_events);
    }

    if (proc->epoll_fd != -1)
    {
        close(proc->epoll_fd);
    }

    free(proc);
}



epoll_tcp_proc* _init_epoll_tcp_proc(unsigned int max_socket_num, epoll_tcp_manager* mgr, pfn_proc_func proc_func, pfn_on_timer on_timer)
{
    epoll_tcp_proc* proc = (epoll_tcp_proc*)malloc(sizeof(epoll_tcp_proc));

    proc->mgr = mgr;

    proc->epoll_fd = epoll_create(max_socket_num);
    proc->arry_epoll_events = (struct epoll_event*)malloc(sizeof(struct epoll_event)*max_socket_num);
    proc->size_epoll_events = max_socket_num;

    proc->list_net_evt = create_loop_cache(0, sizeof(net_event)*max_socket_num * 16);
    proc->list_net_req = create_loop_cache(0, sizeof(net_request)*max_socket_num * 16);

    proc->timer_mgr = create_timer_manager(on_timer);
    //proc->do_net_evt = do_net_evt;

    proc->do_proc_count = 0;

    if (pthread_create(&proc->proc_thread, 0, proc_func, proc))
    {
        proc->proc_thread = 0;
        proc->is_running = false;
    }
    else
    {
        proc->is_running = true;
    }

    if (proc->epoll_fd < 0 ||
        !proc->arry_epoll_events ||
        !proc->list_net_evt ||
        !proc->list_net_req ||
        !proc->timer_mgr ||
        !proc->proc_thread)
    {
        _uninit_epoll_tcp_proc(proc);
        return 0;
    }

    return proc;
}

bool _epoll_tcp_listener_listen(epoll_tcp_listener* listener, const char* ip, unsigned short port, int reuse_addr)
{
    char sz_port[64];
    struct addrinfo hints;
    struct addrinfo* result = 0;

    listener->state = SOCKET_STATE_NONE;

    if (!ulltostr(port, sz_port, sizeof(sz_port), 10))
    {
        return false;
    }

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(ip, sz_port, &hints, &result))
    {
        return false;
    }

    listener->sock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    if (-1 == listener->sock_fd)
    {
        freeaddrinfo(result);
        return false;
    }

    if (reuse_addr)
    {
        reuse_addr = 1;
        setsockopt(listener->sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
    }
    
    if (-1 == bind(listener->sock_fd, result->ai_addr, result->ai_addrlen))
    {
        freeaddrinfo(result);
        close(listener->sock_fd);
        listener->sock_fd = -1;
        return false;
    }

    listener->proc = _get_idle_net_proc(listener->mgr);

    if (!_epoll_tcp_listener_proc_add(listener->proc, listener))
    {
        freeaddrinfo(result);
        close(listener->sock_fd);
        listener->sock_fd = -1;
        return false;
    }

    if (-1 == listen(listener->sock_fd, 256))
    {
        freeaddrinfo(result);
        close(listener->sock_fd);
        listener->sock_fd = -1;
        return false;
    }

    listener->state = SOCKET_STATE_LISTEN;

    freeaddrinfo(result);
    return true;
}

void destroy_net_tcp(epoll_tcp_manager* mgr)
{
    for (unsigned int i = 0; i < mgr->net_proc_num; i++)
    {
        if (mgr->tcp_proc_array[i])
        {
            _uninit_epoll_tcp_proc(mgr->tcp_proc_array[i]);
        }
    }

    if (mgr->max_pkg_buf)
    {
        free(mgr->max_pkg_buf);
    }

    if (mgr->memory_mgr)
    {
        HRBNODE memory_node = rb_first(mgr->memory_mgr);
        while (memory_node)
        {
            destroy_memory_unit((HMEMORYUNIT)rb_node_value_user(memory_node));
            memory_node = rb_next(memory_node);
        }

        destroy_memory_unit(rb_node_unit(mgr->memory_mgr));
        destroy_memory_unit(rb_tree_unit(mgr->memory_mgr));
    }

    if (mgr->socket_pool)
    {
        destroy_memory_unit(mgr->socket_pool);
    }

    free(mgr);
}

epoll_tcp_manager* create_net_tcp(pfn_on_establish func_on_establish, pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error, pfn_on_recv func_on_recv, pfn_parse_packet func_parse_packet,
    unsigned int max_socket_num, unsigned int max_io_thread_num, unsigned int max_accept_ex_num)
{
    epoll_tcp_socket** arry_socket_ptr = 0;

    epoll_tcp_manager* mgr = (epoll_tcp_manager*)malloc(sizeof(epoll_tcp_manager));

    mgr->def_on_establish = func_on_establish;
    mgr->def_on_terminate = func_on_terminate;
    mgr->def_on_error = func_on_error;
    mgr->def_on_recv = func_on_recv;
    mgr->def_parse_packet = func_parse_packet;

    mgr->socket_pool = create_memory_unit(sizeof(epoll_tcp_socket));
    memory_unit_set_grow_count(mgr->socket_pool, max_socket_num);

    arry_socket_ptr = (epoll_tcp_socket**)malloc(sizeof(epoll_tcp_socket*)*max_socket_num);

    for (unsigned int i = 0; i < max_socket_num; i++)
    {
        arry_socket_ptr[i] = _main_thread_alloc(mgr->socket_pool);
        arry_socket_ptr[i]->mgr = mgr;
        arry_socket_ptr[i]->recv_loop_cache = 0;
        arry_socket_ptr[i]->send_loop_cache = 0;
    }
    memory_unit_set_grow_count(mgr->socket_pool, 0);

    for (unsigned int i = 0; i < max_socket_num; i++)
    {
        void** ptr_mem_unit = memory_unit_get_sign(arry_socket_ptr[i]);
        _main_thread_free(mgr->socket_pool, ptr_mem_unit);
    }

    free(arry_socket_ptr);
    arry_socket_ptr = 0;

    mgr->memory_mgr = create_rb_tree_ex(0,
        create_memory_unit(sizeof_rb_tree()), create_memory_unit(sizeof_rb_node()));

    if (!max_io_thread_num)
    {
        mgr->net_proc_num = sysconf(_SC_NPROCESSORS_ONLN);
    }
	else
	{
		mgr->net_proc_num = max_io_thread_num;
	}

    mgr->cur_proc_idx = 0;

    mgr->tcp_proc_array = (epoll_tcp_proc**)malloc(sizeof(epoll_tcp_proc*)*mgr->net_proc_num);

    mgr->max_pkg_buf_size = 64 * 1024;
    mgr->max_pkg_buf = (char*)malloc(mgr->max_pkg_buf_size);

	for (unsigned int i = 0; i < mgr->net_proc_num; i++)
	{
		mgr->tcp_proc_array[i] = _init_epoll_tcp_proc(
			max_socket_num, mgr,
			_epoll_tcp_net_proc_func,
			_epoll_tcp_net_on_timer);
	}

    if (!mgr->socket_pool ||
        !mgr->memory_mgr ||
        !mgr->max_pkg_buf)
    {
        destroy_net_tcp(mgr);
        return 0;
    }

    for (unsigned int i = 0; i < mgr->net_proc_num; i++)
    {
        if (!mgr->tcp_proc_array[i])
        {
            destroy_net_tcp(mgr);
            return 0;
        }
    }

    return mgr;
}

epoll_tcp_socket* net_tcp_connect(
    epoll_tcp_manager* mgr,
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
    pfn_parse_packet func_parse_packet)
{
    epoll_tcp_socket* sock_ptr = _epoll_tcp_manager_alloc_socket(mgr, recv_buf_size, send_buf_size);

    if (!sock_ptr)
    {
        return 0;
    }

    if (func_on_establish)
    {
        sock_ptr->on_establish = func_on_establish;
    }
    else
    {
        sock_ptr->on_establish = mgr->def_on_establish;
    }

    if (func_on_terminate)
    {
        sock_ptr->on_terminate = func_on_terminate;
    }
    else
    {
        sock_ptr->on_terminate = mgr->def_on_terminate;
    }

    if (func_on_error)
    {
        sock_ptr->on_error = func_on_error;
    }
    else
    {
        sock_ptr->on_error = mgr->def_on_error;
    }

    if (func_on_recv)
    {
        sock_ptr->on_recv = func_on_recv;
    }
    else
    {
        sock_ptr->on_recv = mgr->def_on_recv;
    }

    if (func_parse_packet)
    {
        sock_ptr->pkg_parser = func_parse_packet;
    }
    else
    {
        sock_ptr->pkg_parser = mgr->def_parse_packet;
    }

    sock_ptr->proc = _get_idle_net_proc(mgr);
    sock_ptr->listener = 0;
    sock_ptr->state = SOCKET_STATE_CONNECT;

	size_t ip_len = 0;
	size_t bind_ip_len = 0;

    if (ip)
    {
        if ((strlen(ip) + 1) > (MAX_IP_LEN - 1))
        {
            ip_len = (MAX_IP_LEN - 1);
        }
        else
        {
            ip_len = (strlen(ip) + 1);
        }
    }

    if (bind_ip)
    {
        if ((strlen(bind_ip) + 1) > (MAX_IP_LEN - 1))
        {
            ip_len = (MAX_IP_LEN - 1);
        }
        else
        {
            ip_len = (strlen(bind_ip) + 1);
        }
    }

	loop_cache_push_data(sock_ptr->recv_loop_cache, &ip_len, sizeof(ip_len));
	loop_cache_push_data(sock_ptr->recv_loop_cache, &port, sizeof(port));
	loop_cache_push_data(sock_ptr->recv_loop_cache, ip, ip_len);

	loop_cache_push_data(sock_ptr->recv_loop_cache, &bind_ip_len, sizeof(bind_ip_len));
	if (bind_ip_len)
	{
		loop_cache_push_data(sock_ptr->recv_loop_cache, &reuse_addr, sizeof(reuse_addr));
		loop_cache_push_data(sock_ptr->recv_loop_cache, &bind_port, sizeof(bind_port));
		loop_cache_push_data(sock_ptr->recv_loop_cache, bind_ip, bind_ip_len);
	}

    _epoll_tcp_proc_push_req_connect(sock_ptr->proc, sock_ptr);

    return sock_ptr;
}

extern SSL_CTX* def_client_ssl_ctx;

epoll_tcp_socket* net_ssl_connect(
    epoll_tcp_manager* mgr,
    const char* ip,
    unsigned short port,
    unsigned int recv_buf_size,
    unsigned int send_buf_size,
    bool reuse_addr,
    const char* bind_ip,
    unsigned short bind_port,
    SSL_CTX* cli_ssl_ctx,
    pfn_on_establish func_on_establish,
    pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error,
    pfn_on_recv func_on_recv,
    pfn_parse_packet func_parse_packet)
{
    epoll_tcp_socket* sock_ptr = _epoll_tcp_manager_alloc_socket(mgr, recv_buf_size, send_buf_size);

    if (!sock_ptr)
    {
        return 0;
    }

    sock_ptr->ssl_data_ptr = _epoll_tcp_manager_alloc_ssl_data(mgr, DEF_SSL_RECV_CACHE_SIZE, DEF_SSL_SEND_CACHE_SIZE);

    if (func_on_establish)
    {
        sock_ptr->on_establish = func_on_establish;
    }
    else
    {
        sock_ptr->on_establish = mgr->def_on_establish;
    }

    if (func_on_terminate)
    {
        sock_ptr->on_terminate = func_on_terminate;
    }
    else
    {
        sock_ptr->on_terminate = mgr->def_on_terminate;
    }

    if (func_on_error)
    {
        sock_ptr->on_error = func_on_error;
    }
    else
    {
        sock_ptr->on_error = mgr->def_on_error;
    }

    if (func_on_recv)
    {
        sock_ptr->on_recv = func_on_recv;
    }
    else
    {
        sock_ptr->on_recv = mgr->def_on_recv;
    }

    if (func_parse_packet)
    {
        sock_ptr->pkg_parser = func_parse_packet;
    }
    else
    {
        sock_ptr->pkg_parser = mgr->def_parse_packet;
    }

    sock_ptr->proc = _get_idle_net_proc(mgr);
    sock_ptr->listener = 0;
    sock_ptr->state = SOCKET_STATE_CONNECT;

    size_t ip_len = 0;
    size_t bind_ip_len = 0;

    if (ip)
    {
        if ((strlen(ip) + 1) > (MAX_IP_LEN - 1))
        {
            ip_len = (MAX_IP_LEN - 1);
        }
        else
        {
            ip_len = (strlen(ip) + 1);
        }
    }

    if (bind_ip)
    {
        if ((strlen(bind_ip) + 1) > (MAX_IP_LEN - 1))
        {
            ip_len = (MAX_IP_LEN - 1);
        }
        else
        {
            ip_len = (strlen(bind_ip) + 1);
        }
    }

    loop_cache_push_data(sock_ptr->recv_loop_cache, &ip_len, sizeof(ip_len));
    loop_cache_push_data(sock_ptr->recv_loop_cache, &port, sizeof(port));
    loop_cache_push_data(sock_ptr->recv_loop_cache, ip, ip_len);

    loop_cache_push_data(sock_ptr->recv_loop_cache, &bind_ip_len, sizeof(bind_ip_len));
    if (bind_ip_len)
    {
        loop_cache_push_data(sock_ptr->recv_loop_cache, &reuse_addr, sizeof(reuse_addr));
        loop_cache_push_data(sock_ptr->recv_loop_cache, &bind_port, sizeof(bind_port));
        loop_cache_push_data(sock_ptr->recv_loop_cache, bind_ip, bind_ip_len);
    }

    if (!cli_ssl_ctx)
    {
        cli_ssl_ctx = def_client_ssl_ctx;
    }

    _epoll_tcp_proc_push_req_ssl_connect(sock_ptr->proc, sock_ptr, cli_ssl_ctx);

    return sock_ptr;
}

epoll_tcp_listener* net_tcp_listen(
    epoll_tcp_manager* mgr,
    const char * ip,
    unsigned short port,
    unsigned int recv_buf_size,
    unsigned int send_buf_size,
    bool reuse_addr,
    pfn_on_establish func_on_establish,
    pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error,
    pfn_on_recv func_on_recv,
    pfn_parse_packet func_parse_packet)
{
    epoll_tcp_listener* listener = (epoll_tcp_listener*)malloc(sizeof(epoll_tcp_listener));

    listener->recv_buf_size = recv_buf_size;
    listener->send_buf_size = send_buf_size;

    listener->accept_pop = 0;
    listener->accept_push = 0;

    listener->user_data = 0;
    listener->mgr = mgr;
	listener->svr_ssl_ctx = 0;

    if (func_on_establish)
    {
        listener->on_establish = func_on_establish;
    }
    else
    {
        listener->on_establish = mgr->def_on_establish;
    }

    if (func_on_terminate)
    {
        listener->on_terminate = func_on_terminate;
    }
    else
    {
        listener->on_terminate = mgr->def_on_terminate;
    }

    if (func_on_error)
    {
        listener->on_error = func_on_error;
    }
    else
    {
        listener->on_error = mgr->def_on_error;
    }

    if (func_on_recv)
    {
        listener->on_recv = func_on_recv;
    }
    else
    {
        listener->on_recv = mgr->def_on_recv;
    }

    if (func_parse_packet)
    {
        listener->pkg_parser = func_parse_packet;
    }
    else
    {
        listener->pkg_parser = mgr->def_parse_packet;
    }

    if (!_epoll_tcp_listener_listen(listener, ip, port, reuse_addr))
    {
        free(listener);
        return 0;
    }

    return listener;
}

SSL_CTX* (net_ssl_change_ctx)(epoll_tcp_listener* ssl_listener,
    SSL_CTX* new_svr_ssl_ctx)
{
    SSL_CTX* old_svr_ssl_ctx = ssl_listener->svr_ssl_ctx;
    ssl_listener->svr_ssl_ctx = new_svr_ssl_ctx;
    return old_svr_ssl_ctx;
}

epoll_tcp_listener* net_ssl_listen(
    epoll_tcp_manager* mgr,
    const char* ip,
    unsigned short port,
    unsigned int recv_buf_size,
    unsigned int send_buf_size,
    bool reuse_addr,
    SSL_CTX* svr_ssl_ctx,
    pfn_on_establish func_on_establish,
    pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error,
    pfn_on_recv func_on_recv,
    pfn_parse_packet func_parse_packet)
{
    epoll_tcp_listener* listener = (epoll_tcp_listener*)malloc(sizeof(epoll_tcp_listener));

    listener->recv_buf_size = recv_buf_size;
    listener->send_buf_size = send_buf_size;

    listener->accept_pop = 0;
    listener->accept_push = 0;

    listener->user_data = 0;
    listener->mgr = mgr;
    listener->svr_ssl_ctx = svr_ssl_ctx;

    if (func_on_establish)
    {
        listener->on_establish = func_on_establish;
    }
    else
    {
        listener->on_establish = mgr->def_on_establish;
    }

    if (func_on_terminate)
    {
        listener->on_terminate = func_on_terminate;
    }
    else
    {
        listener->on_terminate = mgr->def_on_terminate;
    }

    if (func_on_error)
    {
        listener->on_error = func_on_error;
    }
    else
    {
        listener->on_error = mgr->def_on_error;
    }

    if (func_on_recv)
    {
        listener->on_recv = func_on_recv;
    }
    else
    {
        listener->on_recv = mgr->def_on_recv;
    }

    if (func_parse_packet)
    {
        listener->pkg_parser = func_parse_packet;
    }
    else
    {
        listener->pkg_parser = mgr->def_parse_packet;
    }

    if (!_epoll_tcp_listener_listen(listener, ip, port, reuse_addr))
    {
        free(listener);
        return 0;
    }

    return listener;
}

bool net_tcp_send(epoll_tcp_socket* sock_ptr, const void* data, unsigned int len)
{
    if (!len)
    {
        return true;
    }

    if (sock_ptr->state != SOCKET_STATE_ESTABLISH)
    {
        return false;
    }

    if (!loop_cache_push_data(sock_ptr->send_loop_cache, data, len))
    {
        _epoll_tcp_socket_close(sock_ptr, error_send_overflow, 0, false);
        return false;
    }

    sock_ptr->data_need_send += len;

    if ((sock_ptr->data_need_send - sock_ptr->data_has_send) < sock_ptr->data_delay_send_size)
    {
        return true;
    }

    if (sock_ptr->send_req == sock_ptr->send_ack)
    {
        if (sock_ptr->need_send_active)
        {
            _epoll_tcp_proc_push_req_send(sock_ptr->proc, sock_ptr);
        }
    }

    return true;
}

bool net_tcp_run(epoll_tcp_manager* mgr, unsigned int run_time)
{
	unsigned int unbusy_proc_count = 0;

	if (run_time)
	{
		unsigned int tick = get_tick();

		for (;;)
		{
			if (_do_net_evt(mgr->tcp_proc_array[mgr->cur_proc_idx]))
			{
				unbusy_proc_count = 0;
			}
			else
			{
				unbusy_proc_count++;
			}

			mgr->cur_proc_idx++;

			if (mgr->cur_proc_idx >= mgr->net_proc_num)
			{
				mgr->cur_proc_idx = 0;
			}

			if (unbusy_proc_count >= mgr->net_proc_num)
			{
				return false;
			}

			if (get_tick() - tick >= run_time)
			{
				return true;
			}
		}
	}
	else
	{
		for (;;)
		{
			if (_do_net_evt(mgr->tcp_proc_array[mgr->cur_proc_idx]))
			{
				unbusy_proc_count = 0;
			}
			else
			{
				unbusy_proc_count++;
			}

			mgr->cur_proc_idx++;

			if (mgr->cur_proc_idx >= mgr->net_proc_num)
			{
				mgr->cur_proc_idx = 0;
			}

			if (unbusy_proc_count >= mgr->net_proc_num)
			{
				return false;
			}
		}
	}
}

void net_tcp_close_listener(epoll_tcp_listener* listener)
{
    if (listener)
    {
        _epoll_tcp_listener_close(listener);

        while (listener->accept_pop != listener->accept_push)
        {
            usleep(10000);
        }

        while (listener->state != SOCKET_STATE_DELETE)
        {
            usleep(10000);
        }

        free(listener);
    }
}

void net_tcp_close_session(epoll_tcp_socket* sock_ptr)
{
    _epoll_tcp_socket_close(sock_ptr, error_ok, 0, false);
}

int net_tcp_session_socket(epoll_tcp_socket* sock_ptr)
{
    return sock_ptr->sock_fd;
}

int epoll_tcp_listener_socket(epoll_tcp_listener* listener)
{
    return listener->sock_fd;
}

void net_tcp_set_listener_data(epoll_tcp_listener* listener, void* user_data)
{
    listener->user_data = user_data;
}

void net_tcp_set_session_data(epoll_tcp_socket* sock_ptr, void* user_data)
{
    sock_ptr->user_data = user_data;
}

void* net_tcp_get_listener_data(epoll_tcp_listener* listener)
{
    return listener->user_data;
}

void* net_tcp_get_session_data(epoll_tcp_socket* sock_ptr)
{
    return sock_ptr->user_data;
}

bool net_tcp_get_peer_ip_port(epoll_tcp_socket* sock_ptr, ip_info* info)
{
	if (sock_ptr->peer_sockaddr.sin6_family == AF_INET)
	{
		info->ip_type = ip_v4;
		struct sockaddr_in* sockaddr_v4 = (struct sockaddr_in*)&sock_ptr->peer_sockaddr;
		inet_ntop(sockaddr_v4->sin_family,
			&sockaddr_v4->sin_addr,
			info->ip_str,
			sizeof(info->ip_str));
		info->port = ntohs(sockaddr_v4->sin_port);

		return true;
	}
	else if (sock_ptr->peer_sockaddr.sin6_family == AF_INET6)
	{
		info->ip_type = ip_v6;
		inet_ntop(sock_ptr->peer_sockaddr.sin6_family,
			&sock_ptr->peer_sockaddr.sin6_addr,
			info->ip_str,
			sizeof(info->ip_str));

		info->port = ntohs(sock_ptr->peer_sockaddr.sin6_port);

		return true;
	}
	else
	{
		info->ip_type = ip_unknow;
	}

	return false;
}

bool net_tcp_get_local_ip_port(epoll_tcp_socket* sock_ptr, ip_info* info)
{
	if (sock_ptr->local_sockaddr.sin6_family == AF_INET)
	{
		info->ip_type = ip_v4;
		struct sockaddr_in* sockaddr_v4 = (struct sockaddr_in*)&sock_ptr->local_sockaddr;
		inet_ntop(sockaddr_v4->sin_family,
			&sockaddr_v4->sin_addr,
			info->ip_str,
			sizeof(info->ip_str));
		info->port = ntohs(sockaddr_v4->sin_port);

		return true;
	}
	else if (sock_ptr->local_sockaddr.sin6_family == AF_INET6)
	{
		info->ip_type = ip_v6;
		inet_ntop(sock_ptr->local_sockaddr.sin6_family,
			&sock_ptr->local_sockaddr.sin6_addr,
			info->ip_str,
			sizeof(info->ip_str));

		info->port = ntohs(sock_ptr->local_sockaddr.sin6_port);

		return true;
	}
	else
	{
		info->ip_type = ip_unknow;
		info->port = 0;
	}

	return false;
}

bool net_tcp_get_peer_sock_addr(epoll_tcp_socket* sock_ptr, addr_info* info)
{
	if (sock_ptr->peer_sockaddr.sin6_family == AF_INET)
	{
		info->addr_type = addr_v4;
		info->sock_addr_ptr.v4 = (struct sockaddr_in*)&sock_ptr->peer_sockaddr;

		return true;
	}
	else if (sock_ptr->peer_sockaddr.sin6_family == AF_INET6)
	{
		info->addr_type = addr_v6;
		info->sock_addr_ptr.v6 = &sock_ptr->peer_sockaddr;

		return true;
	}
	else
	{
		info->addr_type = addr_unknow;
		info->sock_addr_ptr.v4 = 0;
	}

	return false;
}

bool net_tcp_get_local_sock_addr(epoll_tcp_socket* sock_ptr, addr_info* info)
{
	if (sock_ptr->local_sockaddr.sin6_family == AF_INET)
	{
		info->addr_type = addr_v4;
		info->sock_addr_ptr.v4 = (struct sockaddr_in*)&sock_ptr->local_sockaddr;

		return true;
	}
	else if (sock_ptr->local_sockaddr.sin6_family == AF_INET6)
	{
		info->addr_type = addr_v6;
		info->sock_addr_ptr.v6 = &sock_ptr->local_sockaddr;

		return true;
	}
	else
	{
		info->addr_type = addr_unknow;
		info->sock_addr_ptr.v4 = 0;
	}

	return false;
}

unsigned int net_tcp_get_send_free_size(epoll_tcp_socket* sock_ptr)
{
    return (unsigned int)loop_cache_free_size(sock_ptr->send_loop_cache);
}

void net_tcp_set_send_control(epoll_tcp_socket* sock_ptr, unsigned int pkg_size, unsigned int delay_time)
{
    sock_ptr->data_delay_send_size = pkg_size;
    _epoll_tcp_socket_mod_timer_send(sock_ptr, delay_time);
}
#elif _MSC_VER
#include "./lib_svr_common_def.h"
#else
#error "unknown compiler"
#endif
