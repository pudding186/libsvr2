#ifdef _MSC_VER
#include <WinSock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>
#include <process.h>
#include "./lib_svr_common_def.h"
#include "../include/loop_cache.h"
#include "../include/timer.h"
#include "../include/memory_pool.h"
#include "../include/rb_tree.h"
#include "../include/net_tcp.h"
#include "../include/net_ssl.h"

#define MAX_BACKLOG     256
#define MAX_ADDR_SIZE   ((sizeof(struct sockaddr_in6)+16)*2)
#define MAX_IP_LEN      2048

//socket state
#define SOCKET_STATE_NONE       0
#define SOCKET_STATE_CONNECT    1
#define SOCKET_STATE_LISTEN     2
#define SOCKET_STATE_ESTABLISH  3
#define SOCKET_STATE_ERROR      4
#define SOCKET_STATE_TERMINATE  5
//#define SOCKET_STATE_DELETE     6
#define SOCKET_STATE_ACCEPT     7
#define SOCKET_STATE_CLOSE      8

#define IOCP_OPT_RECV           0
#define IOCP_OPT_SEND           1
#define IOCP_OPT_ACCEPT         2
#define IOCP_OPT_CONNECT        3
#define IOCP_OPT_CONNECT_REQ    4
#define IOCP_OPT_ACCEPT_REQ     5
#define IOCP_OPT_CLOSE_REQ      6

#define NET_EVENT_ESTABLISH     1
#define NET_EVENT_TERMINATE     2
#define NET_EVENT_DATA          3
#define NET_EVENT_SYSTEM_ERROR  4
#define NET_EVENT_MODULE_ERROR  5
#define NET_EVENT_RECV_ACTIVE   6
#define NET_EVENT_CONNECT_FAIL  7
#define NET_EVENT_SSL_ERROR     8
#define NET_EVENT_SSL_ESTABLISH 9

#define DELAY_CLOSE_SOCKET      15
#define DELAY_SEND_CHECK        5

extern void* _multi_thread_alloc(mem_unit* unit);

typedef struct st_event_establish
{
    struct st_iocp_tcp_listener* listener;
}event_establish;

typedef struct st_evt_data
{
    unsigned int data_len;
}evt_data;

typedef struct st_event_system_error
{
    unsigned int err_code;
}event_system_error;

typedef struct st_event_ssl_error
{
    unsigned int err_code;
}event_ssl_error;

typedef struct st_event_module_error
{
    unsigned int err_code;
}event_module_error;

typedef struct st_event_connect_fail
{
    unsigned int err_code;
}event_connect_fail;

typedef struct st_event_terminate
{
    struct st_iocp_tcp_socket* socket;
}event_terminate;

typedef struct st_net_event
{
    struct st_iocp_tcp_socket*  socket_ptr;
    int                         type;
    union
    {
        struct st_event_establish       evt_establish;
        struct st_evt_data              evt_data;
        struct st_event_system_error    evt_system_error;
        struct st_event_ssl_error       evt_ssl_error;
        struct st_event_module_error    evt_module_error;
        struct st_event_terminate       evt_terminate;
        struct st_event_connect_fail    evt_connect_fail;
    }evt;
}net_event;

typedef struct st_iocp_data
{
    OVERLAPPED  over_lapped;
    WSABUF      wsa_buf;
    int         operation;
    union
    {
        struct st_iocp_tcp_listener*    listener;
        struct st_iocp_tcp_socket*      sock_ptr;
    }pt;
}iocp_data;

typedef struct st_iocp_listen_data
{
    struct st_iocp_data data;
    SOCKET              accept_socket;
}iocp_listen_data;

typedef struct st_iocp_tcp_listener
{
    SOCKET                      tcp_socket;
    unsigned int                send_buf_size;
    unsigned int                recv_buf_size;
    unsigned int                max_accept_ex_num;
    pfn_parse_packet            pkg_parser;
    pfn_on_establish            on_establish;
    pfn_on_terminate            on_terminate;
    pfn_on_error                on_error;
    pfn_on_recv                 on_recv;
    char*                       arry_addr_cache;
    struct st_iocp_listen_data* arry_iocp_data;
    LONG                        state;
    void*                       user_data;
    struct st_iocp_tcp_manager* mgr;
    struct sockaddr_in6         listen_sockaddr;
    SSL_CTX*                    svr_ssl_ctx;
}iocp_tcp_listener;

typedef struct st_iocp_ssl_data
{
    net_ssl_core        core;
    char*               ssl_recv_buf;
    unsigned int        ssl_recv_buf_size;
    char*               ssl_send_buf;
    unsigned int        ssl_send_buf_size;
    unsigned int        bio_read_left;
    unsigned int        ssl_state;
    union
    {
        iocp_tcp_listener*  ssl_listener;
        SSL_CTX*            ssl_client_ctx;
    }ssl_pt;
    
    CRITICAL_SECTION    ssl_lock;
}iocp_ssl_data;

union un_ip
{
    unsigned long   ip4;
    unsigned short  ip6[8];
};

typedef struct st_iocp_tcp_socket
{
    SOCKET                      tcp_socket;
    void*                       user_data;

    struct st_iocp_data         iocp_recv_data;
    struct st_iocp_data         iocp_send_data;

    HLOOPCACHE                  recv_loop_cache;
    HLOOPCACHE                  send_loop_cache;

    unsigned int                recv_req;
    unsigned int                recv_ack;
    LONG                        send_req;
    LONG                        send_ack;

    LONG                        state;

    unsigned int                data_need_send;
    unsigned int                data_has_send;
    unsigned int                data_has_recv;

    unsigned int                data_delay_send_size;

    struct sockaddr_in6         local_sockaddr;
    struct sockaddr_in6         peer_sockaddr;

    HTIMERINFO                  timer_send;
    HTIMERINFO                  timer_close;

    pfn_parse_packet            pkg_parser;

    pfn_on_establish            on_establish;
    pfn_on_terminate            on_terminate;
    pfn_on_error                on_error;
    pfn_on_recv                 on_recv;
    net_tcp_error               error;
    int                         err_code;
    struct st_iocp_tcp_manager* mgr;
    struct st_iocp_ssl_data*    ssl_data_ptr;
}iocp_tcp_socket;

typedef struct st_iocp_tcp_manager
{
    pfn_on_establish            def_on_establish;
    pfn_on_terminate            def_on_terminate;
    pfn_on_error                def_on_error;
    pfn_on_recv                 def_on_recv;
    pfn_parse_packet            def_parse_packet;

    HANDLE                      iocp_port;

    unsigned int                work_thread_num;
    HANDLE*                     work_threads;

    LPFN_CONNECTEX              func_connectex;
    LPFN_ACCEPTEX               func_acceptex;
    LPFN_GETACCEPTEXSOCKADDRS   func_getacceptexsockaddrs;

    CRITICAL_SECTION            evt_lock;
    CRITICAL_SECTION            mem_lock;

    HLOOPCACHE                  evt_queue;
    HTIMERMANAGER               timer_mgr;

    //iocp_tcp_socket*            socket_array;
    //HLOCKFREEPTRQUEUE           socket_pool;
    HMEMORYUNIT                 socket_pool;
    HRBTREE                     memory_mgr;

    unsigned int                max_socket_num;
    unsigned int                max_accept_ex_num;

    char*                       max_pkg_buf;
    unsigned int                max_pkg_buf_size;
}iocp_tcp_manager;

extern bool init_server_ssl_data(net_ssl_core* core, SSL_CTX* ssl_ctx_data);
extern bool init_client_ssl_data(net_ssl_core* core, SSL_CTX* ssl_ctx_data);
extern void uninit_ssl_data(net_ssl_core* core);
extern bool is_ssl_error(int ssl_error);

bool init_iocp_server_ssl_data(iocp_ssl_data* data, SSL_CTX* ssl_ctx_data)
{
    InitializeCriticalSection(&data->ssl_lock);
    return init_server_ssl_data(&data->core, ssl_ctx_data);
}

bool init_iocp_client_ssl_data(iocp_ssl_data* data, SSL_CTX* ssl_ctx_data)
{
    InitializeCriticalSection(&data->ssl_lock);
    return init_client_ssl_data(&data->core, ssl_ctx_data);
}

void uninit_iocp_ssl_data(iocp_ssl_data* data)
{
    DeleteCriticalSection(&data->ssl_lock);
    uninit_ssl_data(&data->core);
}
//////////////////////////////////////////////////////////////////////////

void* _iocp_tcp_manager_alloc_memory(iocp_tcp_manager* mgr, unsigned int buffer_size)
{
    HMEMORYUNIT unit;
    HRBNODE mem_node = rb_tree_find_integer(mgr->memory_mgr, buffer_size);

    if (!mem_node)
    {
        EnterCriticalSection(&mgr->mem_lock);
        mem_node = rb_tree_find_integer(mgr->memory_mgr, buffer_size);
        if (!mem_node)
        {
            unit = create_memory_unit(buffer_size);
            mem_node = rb_tree_insert_integer(mgr->memory_mgr, buffer_size, unit);
        }
        LeaveCriticalSection(&mgr->mem_lock);
    }

    unit = (HMEMORYUNIT)rb_node_value_user(mem_node);

    return _multi_thread_alloc(unit);
}

void _iocp_tcp_manager_free_memory(iocp_tcp_manager* mgr, void* mem, unsigned int buffer_size)
{
    HRBNODE mem_node = rb_tree_find_integer(mgr->memory_mgr, buffer_size);
    if (!mem_node)
    {
        EnterCriticalSection(&mgr->mem_lock);
        mem_node = rb_tree_find_integer(mgr->memory_mgr, buffer_size);
        LeaveCriticalSection(&mgr->mem_lock);

        if (!mem_node)
        {
            CRUSH_CODE();
        }
    }
    HMEMORYUNIT unit = (HMEMORYUNIT)rb_node_value_user(mem_node);

    memory_unit_free(unit, mem);
}

void _iocp_tcp_manager_free_ssl_data(iocp_tcp_manager* mgr, iocp_ssl_data* data)
{
    _iocp_tcp_manager_free_memory(mgr, data, sizeof(iocp_ssl_data) + data->ssl_recv_buf_size + data->ssl_send_buf_size);
}

iocp_ssl_data* _iocp_tcp_manager_alloc_ssl_data(iocp_tcp_manager* mgr, unsigned int ssl_recv_cache_size, unsigned int ssl_send_cache_size)
{
    iocp_ssl_data* data = _iocp_tcp_manager_alloc_memory(mgr, sizeof(iocp_ssl_data) + ssl_recv_cache_size + ssl_send_cache_size);

    data->ssl_state = SSL_UN_HAND_SHAKE;

    data->ssl_recv_buf = ((char*)data) + sizeof(iocp_ssl_data);
    data->ssl_send_buf = ((char*)data) + sizeof(iocp_ssl_data) + ssl_recv_cache_size;

    data->ssl_recv_buf_size = ssl_recv_cache_size;
    data->ssl_send_buf_size = ssl_send_cache_size;

    data->bio_read_left = 0;

    data->core.ssl = 0;
    data->core.bio[BIO_RECV] = 0;
    data->core.bio[BIO_SEND] = 0;

    return data;
}

void _iocp_tcp_socket_reset(iocp_tcp_socket* sock_ptr)
{
    sock_ptr->user_data = 0;
    sock_ptr->state = SOCKET_STATE_NONE;

    sock_ptr->recv_req = 0;
    sock_ptr->recv_ack = 0;

    sock_ptr->send_req = 0;
    sock_ptr->send_ack = 0;

    sock_ptr->data_need_send = 0;
    sock_ptr->data_has_send = 0;

    sock_ptr->data_has_recv = 0;

    sock_ptr->data_delay_send_size = 0;

    memset(&sock_ptr->local_sockaddr, 0, sizeof(sock_ptr->local_sockaddr));
    memset(&sock_ptr->peer_sockaddr, 0, sizeof(sock_ptr->peer_sockaddr));

    sock_ptr->timer_send = 0;
    sock_ptr->timer_close = 0;

    sock_ptr->ssl_data_ptr = 0;
}

iocp_tcp_socket* _iocp_tcp_manager_alloc_socket(iocp_tcp_manager* mgr, unsigned int recv_buf_size, unsigned int send_buf_size)
{
    if (recv_buf_size < 1024)
    {
        recv_buf_size = 1024;
    }

    if (send_buf_size < 1024)
    {
        send_buf_size = 1024;
    }

    iocp_tcp_socket* sock_ptr = (iocp_tcp_socket*)_multi_thread_alloc(mgr->socket_pool);

    if (sock_ptr)
    {
        if (sock_ptr->mgr != mgr)
        {
            CRUSH_CODE();
        }

        _iocp_tcp_socket_reset(sock_ptr);

        if (!sock_ptr->recv_loop_cache || !sock_ptr->send_loop_cache)
        {
            if (sock_ptr->send_loop_cache || sock_ptr->recv_loop_cache)
            {
                CRUSH_CODE();
            }

            sock_ptr->recv_loop_cache = create_loop_cache(_iocp_tcp_manager_alloc_memory(mgr, recv_buf_size), recv_buf_size);
            sock_ptr->send_loop_cache = create_loop_cache(_iocp_tcp_manager_alloc_memory(mgr, send_buf_size), send_buf_size);
        }
        else
        {
            if ((unsigned int)loop_cache_size(sock_ptr->recv_loop_cache) != recv_buf_size)
            {
                _iocp_tcp_manager_free_memory(mgr, loop_cache_get_cache(sock_ptr->recv_loop_cache), (unsigned int)loop_cache_size(sock_ptr->recv_loop_cache));
                loop_cache_reset(sock_ptr->recv_loop_cache, recv_buf_size, (char*)_iocp_tcp_manager_alloc_memory(mgr, recv_buf_size));
            }

            if ((unsigned int)loop_cache_size(sock_ptr->send_loop_cache) != send_buf_size)
            {
                _iocp_tcp_manager_free_memory(mgr, loop_cache_get_cache(sock_ptr->send_loop_cache), (unsigned int)loop_cache_size(sock_ptr->send_loop_cache));
                loop_cache_reset(sock_ptr->send_loop_cache, send_buf_size, (char*)_iocp_tcp_manager_alloc_memory(mgr, send_buf_size));
            }

            loop_cache_reinit(sock_ptr->recv_loop_cache);
            loop_cache_reinit(sock_ptr->send_loop_cache);
        }
    }

    return sock_ptr;
}

void _iocp_tcp_manager_free_socket(iocp_tcp_manager* mgr, iocp_tcp_socket* sock_ptr)
{
    memory_unit_free(mgr->socket_pool, sock_ptr);
}

//////////////////////////////////////////////////////////////////////////

bool _iocp_tcp_socket_bind_iocp_port(iocp_tcp_socket* sock_ptr)
{
    if (CreateIoCompletionPort((HANDLE)sock_ptr->tcp_socket, sock_ptr->mgr->iocp_port, (ULONG_PTR)sock_ptr, 0))
    {
        return true;
    }

    return false;
}

#define EVENT_LOCK EnterCriticalSection(&sock_ptr->mgr->evt_lock)
#define EVENT_UNLOCK LeaveCriticalSection(&sock_ptr->mgr->evt_lock)

void _push_data_event(iocp_tcp_socket* sock_ptr, unsigned int data_len)
{
    if (sock_ptr->state == SOCKET_STATE_ESTABLISH)
    {
        struct st_net_event* evt;
        size_t evt_len = sizeof(struct st_net_event);

        EVENT_LOCK;
        loop_cache_get_free(sock_ptr->mgr->evt_queue, &evt, &evt_len);

        if (evt_len != sizeof(struct st_net_event))
        {
            CRUSH_CODE();
        }

        evt->socket_ptr = sock_ptr;
        evt->type = NET_EVENT_DATA;
        evt->evt.evt_data.data_len = data_len;

        loop_cache_push(sock_ptr->mgr->evt_queue, evt_len);
        EVENT_UNLOCK;
    }
}

void _push_establish_event(iocp_tcp_listener* listener, iocp_tcp_socket* sock_ptr)
{
    struct st_net_event* evt;
    size_t evt_len = sizeof(struct st_net_event);
    EVENT_LOCK;
    loop_cache_get_free(sock_ptr->mgr->evt_queue, &evt, &evt_len);

    if (evt_len != sizeof(struct st_net_event))
    {
        CRUSH_CODE();
    }

    evt->socket_ptr = sock_ptr;
    evt->type = NET_EVENT_ESTABLISH;
    evt->evt.evt_establish.listener = listener;

    loop_cache_push(sock_ptr->mgr->evt_queue, evt_len);
    EVENT_UNLOCK;
}

void _push_ssl_establish_event(iocp_tcp_socket* sock_ptr)
{
    struct st_net_event* evt;
    size_t evt_len = sizeof(struct st_net_event);
    EVENT_LOCK;
    loop_cache_get_free(sock_ptr->mgr->evt_queue, &evt, &evt_len);

    if (evt_len != sizeof(struct st_net_event))
    {
        CRUSH_CODE();
    }

    evt->socket_ptr = sock_ptr;
    evt->type = NET_EVENT_SSL_ESTABLISH;
    evt->evt.evt_establish.listener = sock_ptr->ssl_data_ptr->ssl_pt.ssl_listener;

    loop_cache_push(sock_ptr->mgr->evt_queue, evt_len);
    EVENT_UNLOCK;
}

void _push_system_error_event(iocp_tcp_socket* sock_ptr, int err_code)
{
    struct st_net_event* evt;
    size_t evt_len = sizeof(struct st_net_event);
    EVENT_LOCK;
    loop_cache_get_free(sock_ptr->mgr->evt_queue, &evt, &evt_len);

    if (evt_len != sizeof(struct st_net_event))
    {
        CRUSH_CODE();
    }

    evt->socket_ptr = sock_ptr;
    evt->type = NET_EVENT_SYSTEM_ERROR;
    evt->evt.evt_system_error.err_code = err_code;

    loop_cache_push(sock_ptr->mgr->evt_queue, evt_len);
    EVENT_UNLOCK;
}

void _push_ssl_error_event(iocp_tcp_socket* sock_ptr, int err_code)
{
    struct st_net_event* evt;
    size_t evt_len = sizeof(struct st_net_event);
    EVENT_LOCK;
    loop_cache_get_free(sock_ptr->mgr->evt_queue, &evt, &evt_len);

    if (evt_len != sizeof(struct st_net_event))
    {
        CRUSH_CODE();
    }

    evt->socket_ptr = sock_ptr;
    evt->type = NET_EVENT_SSL_ERROR;
    evt->evt.evt_ssl_error.err_code = err_code;

    loop_cache_push(sock_ptr->mgr->evt_queue, evt_len);
    EVENT_UNLOCK;
}

void _push_module_error_event(iocp_tcp_socket* sock_ptr, int err_code)
{
    struct st_net_event* evt;
    size_t evt_len = sizeof(struct st_net_event);
    EVENT_LOCK;
    loop_cache_get_free(sock_ptr->mgr->evt_queue, &evt, &evt_len);

    if (evt_len != sizeof(struct st_net_event))
    {
        CRUSH_CODE();
    }

    evt->socket_ptr = sock_ptr;
    evt->type = NET_EVENT_MODULE_ERROR;
    evt->evt.evt_module_error.err_code = err_code;

    loop_cache_push(sock_ptr->mgr->evt_queue, evt_len);
    EVENT_UNLOCK;
}

void _push_terminate_event(iocp_tcp_socket* sock_ptr)
{
    struct st_net_event* evt;
    size_t evt_len = sizeof(struct st_net_event);
    EVENT_LOCK;
    loop_cache_get_free(sock_ptr->mgr->evt_queue, &evt, &evt_len);

    if (evt_len != sizeof(struct st_net_event))
    {
        CRUSH_CODE();
    }

    evt->socket_ptr = sock_ptr;
    evt->type = NET_EVENT_TERMINATE;

    loop_cache_push(sock_ptr->mgr->evt_queue, evt_len);
    EVENT_UNLOCK;
}

void _push_connect_fail_event(iocp_tcp_socket* sock_ptr, int err_code)
{
    struct st_net_event* evt;
    size_t evt_len = sizeof(struct st_net_event);
    EVENT_LOCK;
    loop_cache_get_free(sock_ptr->mgr->evt_queue, &evt, &evt_len);

    if (evt_len != sizeof(struct st_net_event))
    {
        CRUSH_CODE();
    }

    evt->socket_ptr = sock_ptr;
    evt->type = NET_EVENT_CONNECT_FAIL;
    evt->evt.evt_connect_fail.err_code = err_code;

    loop_cache_push(sock_ptr->mgr->evt_queue, evt_len);
    EVENT_UNLOCK;
}

void _push_recv_active_event(iocp_tcp_socket* sock_ptr)
{
    if (sock_ptr->state == SOCKET_STATE_ESTABLISH)
    {
        struct st_net_event* evt;
        size_t evt_len = sizeof(struct st_net_event);
        EVENT_LOCK;
        loop_cache_get_free(sock_ptr->mgr->evt_queue, &evt, &evt_len);

        if (evt_len != sizeof(struct st_net_event))
        {
            CRUSH_CODE();
        }

        evt->socket_ptr = sock_ptr;
        evt->type = NET_EVENT_RECV_ACTIVE;

        loop_cache_push(sock_ptr->mgr->evt_queue, evt_len);
        EVENT_UNLOCK;
    }
}

void _iocp_tcp_socket_on_close_req(iocp_tcp_socket* sock_ptr)
{
    if (sock_ptr->tcp_socket != INVALID_SOCKET)
    {
        closesocket(sock_ptr->tcp_socket);
        sock_ptr->tcp_socket = INVALID_SOCKET;
    }

    if (sock_ptr->ssl_data_ptr)
    {
        uninit_iocp_ssl_data(sock_ptr->ssl_data_ptr);

        if (!sock_ptr->ssl_data_ptr->ssl_pt.ssl_listener)
        {
            if (sock_ptr->ssl_data_ptr->ssl_state == SSL_UN_HAND_SHAKE)
            {
                _push_connect_fail_event(sock_ptr, sock_ptr->err_code);
                return;
            }
        }
    }

    switch (sock_ptr->error)
    {
    case error_system:
    {
        _push_system_error_event(sock_ptr, sock_ptr->err_code);
    }
    break;
    case error_ssl:
    {
        _push_ssl_error_event(sock_ptr, sock_ptr->err_code);
    }
    break;
    case error_send_overflow:
    case error_recv_overflow:
    case error_packet:
    {
        _push_module_error_event(sock_ptr, sock_ptr->error);
    }
    break;
    case error_ok:
    {
        _push_terminate_event(sock_ptr);
    }
    break;
    default:
    {
        CRUSH_CODE();
    }
    }
}

void _mod_timer_close(iocp_tcp_socket* sock_ptr, unsigned int elapse)
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

    sock_ptr->timer_close = timer_add(sock_ptr->mgr->timer_mgr, elapse, -1, sock_ptr);

    if (!sock_ptr->timer_close)
    {
        CRUSH_CODE();
    }
}

void _iocp_tcp_socket_close(iocp_tcp_socket* sock_ptr, net_tcp_error error, int err_code, bool is_iocp_thread)
{
    if (SOCKET_STATE_ESTABLISH == InterlockedCompareExchange(&sock_ptr->state, SOCKET_STATE_TERMINATE, SOCKET_STATE_ESTABLISH))
    {
        sock_ptr->error = error;
        sock_ptr->err_code = err_code;

        if (is_iocp_thread)
        {
            _iocp_tcp_socket_on_close_req(sock_ptr);
        }
        else
        {
            sock_ptr->state = SOCKET_STATE_CLOSE;
            _mod_timer_close(sock_ptr, 1);
        }
    }
}

bool _iocp_tcp_socket_post_close_req(iocp_tcp_socket* sock_ptr)
{
    ZeroMemory(&sock_ptr->iocp_send_data.over_lapped, sizeof(sock_ptr->iocp_send_data.over_lapped));

    ++sock_ptr->send_req;

    sock_ptr->iocp_send_data.operation = IOCP_OPT_CLOSE_REQ;

    if (PostQueuedCompletionStatus(sock_ptr->mgr->iocp_port, 0, (ULONG_PTR)sock_ptr, &sock_ptr->iocp_send_data.over_lapped))
    {
        return true;
    }

    --sock_ptr->send_req;
    return false;
}

bool _iocp_tcp_socket_post_recv_req(iocp_tcp_socket* sock_ptr)
{
    ZeroMemory(&sock_ptr->iocp_recv_data.over_lapped, sizeof(sock_ptr->iocp_recv_data.over_lapped));

    ++sock_ptr->recv_req;

    if (PostQueuedCompletionStatus(sock_ptr->mgr->iocp_port, 0xffffffff, (ULONG_PTR)sock_ptr, &sock_ptr->iocp_recv_data.over_lapped))
    {
        return true;
    }

    --sock_ptr->recv_req;
    return false;
}

bool _iocp_tcp_socket_post_ssl_recv(iocp_tcp_socket* sock_ptr)
{
    DWORD byte_received;
    DWORD flags = 0;

    ZeroMemory(&sock_ptr->iocp_recv_data.over_lapped, sizeof(sock_ptr->iocp_recv_data.over_lapped));

    sock_ptr->iocp_recv_data.wsa_buf.buf = sock_ptr->ssl_data_ptr->ssl_recv_buf;
    sock_ptr->iocp_recv_data.wsa_buf.len = (ULONG)sock_ptr->ssl_data_ptr->ssl_recv_buf_size;

    ++sock_ptr->recv_req;

    if (WSARecv(sock_ptr->tcp_socket, &sock_ptr->iocp_recv_data.wsa_buf, 1, &byte_received, &flags, &sock_ptr->iocp_recv_data.over_lapped, NULL))
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            --sock_ptr->recv_req;
            return false;
        }
    }

    return true;
}

bool _iocp_tcp_socket_post_recv(iocp_tcp_socket* sock_ptr)
{
    DWORD byte_received;
    DWORD flags = 0;

    char* recv_ptr = 0;
    size_t recv_len = 0;

    ZeroMemory(&sock_ptr->iocp_recv_data.over_lapped, sizeof(sock_ptr->iocp_recv_data.over_lapped));

    loop_cache_get_free(sock_ptr->recv_loop_cache, &recv_ptr, &recv_len);

    sock_ptr->iocp_recv_data.wsa_buf.buf = recv_ptr;
    sock_ptr->iocp_recv_data.wsa_buf.len = (ULONG)recv_len;

    ++sock_ptr->recv_req;

    if (WSARecv(sock_ptr->tcp_socket, &sock_ptr->iocp_recv_data.wsa_buf, 1, &byte_received, &flags, &sock_ptr->iocp_recv_data.over_lapped, NULL))
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            --sock_ptr->recv_req;
            return false;
        }
    }

    return true;
}

bool _iocp_tcp_socket_post_send(iocp_tcp_socket* sock_ptr)
{
    DWORD number_of_byte_sent = 0;

    ZeroMemory(&sock_ptr->iocp_send_data.over_lapped, sizeof(sock_ptr->iocp_send_data.over_lapped));

    ++sock_ptr->send_req;

    if (WSASend(sock_ptr->tcp_socket, &sock_ptr->iocp_send_data.wsa_buf, 1, &number_of_byte_sent, 0, &sock_ptr->iocp_send_data.over_lapped, NULL))
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            --sock_ptr->send_req;
            return false;
        }
    }

    return true;
}

bool _iocp_tcp_socket_post_send_req(iocp_tcp_socket* sock_ptr)
{
    ZeroMemory(&sock_ptr->iocp_send_data.over_lapped, sizeof(sock_ptr->iocp_send_data.over_lapped));

    ++sock_ptr->send_req;

    if (PostQueuedCompletionStatus(sock_ptr->mgr->iocp_port, 0xffffffff, (ULONG_PTR)socket, &sock_ptr->iocp_send_data.over_lapped))
    {
        return true;
    }

    --sock_ptr->send_req;

    return false;
}

void _iocp_tcp_socket_on_ssl_recv(iocp_tcp_socket* sock_ptr, BOOL ret, DWORD trans_byte)
{
    int bio_ret;
    int ssl_ret;
    unsigned int decrypt_data_push_len = 0;

    ++sock_ptr->recv_ack;

    if (!ret)
    {
        _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
        return;
    }

    if (sock_ptr->state != SOCKET_STATE_ESTABLISH)
    {
        return;
    }

    if (trans_byte == 0)
    {
        _iocp_tcp_socket_close(sock_ptr, error_ok, 0, true);
        return;
    }
    else if (trans_byte == 0xffffffff)
    {
        trans_byte = 0;
    }

    if (trans_byte)
    {
        bio_ret = BIO_write(sock_ptr->ssl_data_ptr->core.bio[BIO_RECV], sock_ptr->ssl_data_ptr->ssl_recv_buf, trans_byte);

        if (bio_ret <= 0)
        {
            CRUSH_CODE();
        }
        else
        {
            if ((DWORD)bio_ret != trans_byte)
            {
                CRUSH_CODE();
            }
        }
    }

    char* free_data_ptr = 0;
    size_t free_data_len = 0;

    loop_cache_get_free(sock_ptr->recv_loop_cache, (void**)&free_data_ptr, &free_data_len);

    while (free_data_len)
    {
        EnterCriticalSection(&sock_ptr->ssl_data_ptr->ssl_lock);
        ssl_ret = SSL_read(sock_ptr->ssl_data_ptr->core.ssl, free_data_ptr, (int)free_data_len);
        LeaveCriticalSection(&sock_ptr->ssl_data_ptr->ssl_lock);

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
            int error_code = SSL_get_error(sock_ptr->ssl_data_ptr->core.ssl, ssl_ret);
            if (is_ssl_error(error_code))
            {
                _iocp_tcp_socket_close(sock_ptr, error_ssl, ERR_get_error(), true);
                return;
            }
            break;
        }

        free_data_ptr = 0;
        free_data_len = 0;
        loop_cache_get_free(sock_ptr->recv_loop_cache, (void**)&free_data_ptr, &free_data_len);
    }

    if (sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE)
    {
        if (decrypt_data_push_len)
        {
            _push_data_event(sock_ptr, decrypt_data_push_len);
        }
    }
    else
    {
        if (sock_ptr->send_req == sock_ptr->send_ack)
        {
            if (!_iocp_tcp_socket_post_send_req(sock_ptr))
            {
                _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
                return;
            }
        }

        if (SSL_is_init_finished(sock_ptr->ssl_data_ptr->core.ssl))
        {
            sock_ptr->ssl_data_ptr->ssl_state = SSL_HAND_SHAKE;
            _push_ssl_establish_event(sock_ptr);
        }
    }

    if (free_data_len)
    {
        if (!_iocp_tcp_socket_post_ssl_recv(sock_ptr))
        {
            _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
        }
    }
    else
    {
        _push_recv_active_event(sock_ptr);
    }
}

void _iocp_tcp_socket_on_ssl_send(iocp_tcp_socket* sock_ptr, BOOL ret, DWORD trans_byte)
{
    int ssl_ret = 0;
    int bio_ret = 0;

    if (!ret)
    {
        _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);

        InterlockedIncrement(&sock_ptr->send_ack);
        return;
    }

    if ((sock_ptr->state != SOCKET_STATE_ESTABLISH) &&
        (sock_ptr->state != SOCKET_STATE_CLOSE))
    {
        InterlockedIncrement(&sock_ptr->send_ack);
        return;
    }

    if (0 == trans_byte)
    {
        _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);

        InterlockedIncrement(&sock_ptr->send_ack);
        return;
    }
    else if (0xffffffff == trans_byte)
    {
        trans_byte = 0;
    }

    char* data_ptr = 0;
    size_t data_len = 0;

    loop_cache_get_data(sock_ptr->send_loop_cache, &data_ptr, &data_len);

    while (data_len)
    {
        EnterCriticalSection(&sock_ptr->ssl_data_ptr->ssl_lock);
        ssl_ret = SSL_write(sock_ptr->ssl_data_ptr->core.ssl, data_ptr, (int)data_len);
        LeaveCriticalSection(&sock_ptr->ssl_data_ptr->ssl_lock);

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
            int error_code = SSL_get_error(sock_ptr->ssl_data_ptr->core.ssl, ssl_ret);

            if (is_ssl_error(error_code))
            {
                _iocp_tcp_socket_close(sock_ptr, error_ssl, ERR_get_error(), true);
                InterlockedIncrement(&sock_ptr->send_ack);
                return;
            }

            break;
        }

        data_ptr = 0;
        data_len = 0;
        loop_cache_get_data(sock_ptr->send_loop_cache, &data_ptr, &data_len);
    }

    if (trans_byte)
    {
        if (trans_byte > sock_ptr->ssl_data_ptr->bio_read_left)
        {
            CRUSH_CODE();
        }

        sock_ptr->ssl_data_ptr->bio_read_left -= trans_byte;

        if (sock_ptr->ssl_data_ptr->bio_read_left)
        {
            memcpy(
                sock_ptr->ssl_data_ptr->ssl_send_buf,
                sock_ptr->ssl_data_ptr->ssl_send_buf + trans_byte,
                sock_ptr->ssl_data_ptr->bio_read_left);
        }
    }

    if (BIO_pending(sock_ptr->ssl_data_ptr->core.bio[BIO_SEND]))
    {
        bio_ret = BIO_read(
            sock_ptr->ssl_data_ptr->core.bio[BIO_SEND],
            sock_ptr->ssl_data_ptr->ssl_send_buf + sock_ptr->ssl_data_ptr->bio_read_left,
            sock_ptr->ssl_data_ptr->ssl_send_buf_size - sock_ptr->ssl_data_ptr->bio_read_left);

        if (bio_ret > 0)
        {
            sock_ptr->ssl_data_ptr->bio_read_left += bio_ret;
        }
        else
        {
            CRUSH_CODE();
        }
    }

    if (sock_ptr->ssl_data_ptr->bio_read_left)
    {
        sock_ptr->iocp_send_data.wsa_buf.buf = sock_ptr->ssl_data_ptr->ssl_send_buf;
        sock_ptr->iocp_send_data.wsa_buf.len = sock_ptr->ssl_data_ptr->bio_read_left;

        if (!_iocp_tcp_socket_post_send(sock_ptr))
        {
            _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
        }
    }

    InterlockedIncrement(&sock_ptr->send_ack);
}

void _iocp_tcp_socket_on_recv(iocp_tcp_socket* sock_ptr, BOOL ret, DWORD trans_byte)
{
    ++sock_ptr->recv_ack;
    if (!ret)
    {
        _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
        return;
    }

    if (sock_ptr->state != SOCKET_STATE_ESTABLISH)
    {
        return;
    }

    switch (trans_byte)
    {
    case 0:
    {
        _iocp_tcp_socket_close(sock_ptr, error_ok, 0, true);
        return;
    }
    break;
    case 0xffffffff:
    {
        trans_byte = 0;
    }
    break;
    }

    if (trans_byte)
    {
        if (!loop_cache_push(sock_ptr->recv_loop_cache, trans_byte))
        {
            CRUSH_CODE();
        }

        _push_data_event(sock_ptr, trans_byte);
    }

    if (loop_cache_free_size(sock_ptr->recv_loop_cache))
    {
        if (!_iocp_tcp_socket_post_recv(sock_ptr))
        {
            _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
        }
    }
    else
    {
        _push_recv_active_event(sock_ptr);
    }
}

void _iocp_tcp_socket_on_send(iocp_tcp_socket* sock_ptr, BOOL ret, DWORD trans_byte)
{
    char* send_ptr = 0;
    size_t send_len = 0;

    if (!ret)
    {
        _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);

        InterlockedIncrement(&sock_ptr->send_ack);
        return;
    }

    if ((sock_ptr->state != SOCKET_STATE_ESTABLISH) &&
        (sock_ptr->state != SOCKET_STATE_CLOSE) &&
        (sock_ptr->state != SOCKET_STATE_TERMINATE))
    {
        InterlockedIncrement(&sock_ptr->send_ack);
        return;
    }

    if (0 == trans_byte)
    {
        _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);

        InterlockedIncrement(&sock_ptr->send_ack);
        return;
    }

    if (0xffffffff == trans_byte)
    {
        trans_byte = 0;
        sock_ptr->iocp_send_data.wsa_buf.len = 0;
    }
    else
    {
        sock_ptr->data_has_send += trans_byte;

        if (!loop_cache_pop(sock_ptr->send_loop_cache, trans_byte))
        {
            CRUSH_CODE();
        }
    }

    loop_cache_get_data(sock_ptr->send_loop_cache, &send_ptr, &send_len);

    if (send_len)
    {
        sock_ptr->iocp_send_data.wsa_buf.buf = send_ptr;
        sock_ptr->iocp_send_data.wsa_buf.len = (ULONG)send_len;

        if (!_iocp_tcp_socket_post_send(sock_ptr))
        {
            _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
        }
    }

    InterlockedIncrement(&sock_ptr->send_ack);
}

bool _iocp_tcp_socket_post_connect_req(iocp_tcp_socket* sock_ptr)
{
    ZeroMemory(&sock_ptr->iocp_recv_data.over_lapped, sizeof(sock_ptr->iocp_recv_data.over_lapped));

    ++sock_ptr->recv_req;

    sock_ptr->iocp_recv_data.operation = IOCP_OPT_CONNECT_REQ;

    if (PostQueuedCompletionStatus(sock_ptr->mgr->iocp_port, 0, (ULONG_PTR)sock_ptr, &sock_ptr->iocp_recv_data.over_lapped))
    {
        return true;
    }

    --sock_ptr->recv_req;
    return false;
}

void _iocp_tcp_socket_on_connect_req(iocp_tcp_socket* socket_ptr)
{
    ++socket_ptr->recv_ack;

    bool reuse_addr = false;
    char ip_data[MAX_IP_LEN];
    char bind_ip_data[MAX_IP_LEN];
    size_t ip_len = 0;
    size_t bind_ip_len = 0;
    unsigned short port = 0;
    unsigned short bind_port = 0;

    struct addrinfo hints;
    struct addrinfo* result = 0;
    char sz_port[64];

    ZeroMemory(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

	if (!loop_cache_pop_data(socket_ptr->recv_loop_cache, &ip_len, sizeof(ip_len)))
	{
		CRUSH_CODE();
	}

    if (!loop_cache_pop_data(socket_ptr->recv_loop_cache, &port, sizeof(port)))
    {
        CRUSH_CODE();
    }

	if (!loop_cache_pop_data(socket_ptr->recv_loop_cache, ip_data, ip_len))
	{
		CRUSH_CODE();
	}


	if (!loop_cache_pop_data(socket_ptr->recv_loop_cache, &bind_ip_len, sizeof(bind_ip_len)))
	{
		CRUSH_CODE();
	}

	if (bind_ip_len)
	{
		if (!loop_cache_pop_data(socket_ptr->recv_loop_cache, &reuse_addr, sizeof(reuse_addr)))
		{
			CRUSH_CODE();
		}

		if (!loop_cache_pop_data(socket_ptr->recv_loop_cache, &bind_port, sizeof(bind_port)))
		{
			CRUSH_CODE();
		}

		if (!loop_cache_pop_data(socket_ptr->recv_loop_cache, bind_ip_data, bind_ip_len))
		{
			CRUSH_CODE();
		}

		hints.ai_flags = AI_PASSIVE;

		_itoa_s(bind_port, sz_port, sizeof(sz_port), 10);

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
			if (result->ai_addrlen >= sizeof(socket_ptr->local_sockaddr))
			{
				freeaddrinfo(result);
				goto ERROR_DEAL;
			}

			memcpy(&socket_ptr->local_sockaddr, result->ai_addr, result->ai_addrlen);

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
        _itoa_s(port, sz_port, sizeof(sz_port), 10);

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
            if (result->ai_addrlen >= sizeof(socket_ptr->peer_sockaddr))
            {
                freeaddrinfo(result);
                goto ERROR_DEAL;
            }

            memcpy(&socket_ptr->peer_sockaddr, result->ai_addr, result->ai_addrlen);
            freeaddrinfo(result);
            result = 0;
        }
    }

    if (bind_ip_len)
    {
        if (socket_ptr->local_sockaddr.sin6_family != socket_ptr->peer_sockaddr.sin6_family)
        {
            goto ERROR_DEAL;
        }
    }

    socket_ptr->tcp_socket = socket(socket_ptr->peer_sockaddr.sin6_family, SOCK_STREAM, IPPROTO_TCP);

    if (INVALID_SOCKET == socket_ptr->tcp_socket)
    {
        goto ERROR_DEAL;
    }

    if (reuse_addr)
    {
        INT32 nReuse = 1;
        if (setsockopt(socket_ptr->tcp_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof(nReuse)) == SOCKET_ERROR)
        {
            goto ERROR_DEAL;
        }
    }

    int local_sockaddr_len = sizeof(struct sockaddr_in);
    int peer_sockaddr_len = sizeof(struct sockaddr_in);
    

    if (socket_ptr->peer_sockaddr.sin6_family == AF_INET6)
    {
        peer_sockaddr_len = sizeof(struct sockaddr_in6);
        local_sockaddr_len = sizeof(struct sockaddr_in6);
    }

    if (!socket_ptr->local_sockaddr.sin6_family)
    {
        if (socket_ptr->peer_sockaddr.sin6_family == AF_INET)
        {
            struct sockaddr_in* addr = (struct sockaddr_in*)&socket_ptr->local_sockaddr;
            addr->sin_family = AF_INET;
            addr->sin_addr.S_un.S_addr = htonl(INADDR_ANY);
            addr->sin_port = htons(0);
        }
        else
        {
            socket_ptr->local_sockaddr.sin6_family = AF_INET6;
            socket_ptr->local_sockaddr.sin6_addr = in6addr_any;
            socket_ptr->local_sockaddr.sin6_port = htons(0);
        }
    }

    if (SOCKET_ERROR ==
        bind(socket_ptr->tcp_socket,
        (struct sockaddr*)&socket_ptr->local_sockaddr,
            local_sockaddr_len))
    {
        goto ERROR_DEAL;
    }

    if (!_iocp_tcp_socket_bind_iocp_port(socket_ptr))
    {
        goto ERROR_DEAL;
    }

    socket_ptr->iocp_recv_data.operation = IOCP_OPT_CONNECT;

    ++socket_ptr->recv_req;

    if (!socket_ptr->mgr->func_connectex(
        socket_ptr->tcp_socket, 
        (struct sockaddr*)&socket_ptr->peer_sockaddr,
        peer_sockaddr_len,
        0, 0, 0, &socket_ptr->iocp_recv_data.over_lapped))
    {
        if (WSAGetLastError() != ERROR_IO_PENDING)
        {
            --socket_ptr->recv_req;
            goto ERROR_DEAL;
        }
    }

    return;

ERROR_DEAL:
    socket_ptr->state = SOCKET_STATE_TERMINATE;

    if (socket_ptr->tcp_socket != INVALID_SOCKET)
    {
        closesocket(socket_ptr->tcp_socket);
        socket_ptr->tcp_socket = INVALID_SOCKET;
    }

    _push_connect_fail_event(socket_ptr, WSAGetLastError());
}

void _iocp_tcp_socket_on_connect(iocp_tcp_socket* sock_ptr, BOOL ret)
{
    struct sockaddr_storage addr = { 0 };
    int addr_len = sizeof(addr);

    ++sock_ptr->recv_ack;

    if (!ret)
    {
        goto ERROR_DEAL;
    }

    if (SOCKET_ERROR == setsockopt(sock_ptr->tcp_socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0))
    {
        goto ERROR_DEAL;
    }

    if (SOCKET_ERROR == getsockname(sock_ptr->tcp_socket, (struct sockaddr*)&addr, &addr_len))
    {
        goto ERROR_DEAL;
    }

    memcpy(&sock_ptr->local_sockaddr, &addr, addr_len);

    sock_ptr->state = SOCKET_STATE_ESTABLISH;

    sock_ptr->iocp_recv_data.operation = IOCP_OPT_RECV;
    sock_ptr->iocp_send_data.operation = IOCP_OPT_SEND;

    int no_delay = 1;
    setsockopt(sock_ptr->tcp_socket, IPPROTO_TCP,
        TCP_NODELAY, (char*)&no_delay, sizeof(no_delay));

    _push_establish_event(0, sock_ptr);

    if (sock_ptr->ssl_data_ptr)
    {
        SSL_CTX* client_ssl_ctx = sock_ptr->ssl_data_ptr->ssl_pt.ssl_client_ctx;
        sock_ptr->ssl_data_ptr->ssl_pt.ssl_listener = 0;
        if (!init_iocp_client_ssl_data(sock_ptr->ssl_data_ptr, client_ssl_ctx))
        {
            _iocp_tcp_socket_close(sock_ptr, error_ssl, ERR_get_error(), true);
        }
        else
        {
            if (!_iocp_tcp_socket_post_recv_req(sock_ptr))
            {
                _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
            }
        }
    }
    else
    {
        if (!_iocp_tcp_socket_post_recv(sock_ptr))
        {
            _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
        }
    }

    return;

ERROR_DEAL:

    sock_ptr->state = SOCKET_STATE_TERMINATE;
    closesocket(sock_ptr->tcp_socket);
    sock_ptr->tcp_socket = INVALID_SOCKET;

    _push_connect_fail_event(sock_ptr, WSAGetLastError());
}

void _mod_timer_send(iocp_tcp_socket* sock_ptr, unsigned int elapse)
{
    if (sock_ptr->timer_send)
    {
        timer_mod(sock_ptr->timer_send, elapse, -1, sock_ptr);
        return;
    }

    sock_ptr->timer_send = timer_add(sock_ptr->mgr->timer_mgr, elapse, -1, sock_ptr);

    if (!sock_ptr->timer_send)
    {
        CRUSH_CODE();
    }
}

//////////////////////////////////////////////////////////////////////////
bool _iocp_tcp_listener_bind_iocp_port(iocp_tcp_listener* listener)
{
    if (CreateIoCompletionPort((HANDLE)listener->tcp_socket, listener->mgr->iocp_port, (ULONG_PTR)listener, 0))
    {
        return true;
    }

    return false;
}

bool _iocp_tcp_listener_post_accept_ex(iocp_tcp_listener* listener, struct st_iocp_listen_data* iocp_listen_data_ptr)
{
    DWORD bytes = 0;

    BOOL ret;

    SOCKET accept_socket = WSASocketW(
        listener->listen_sockaddr.sin6_family,
        SOCK_STREAM,
        IPPROTO_TCP,
        NULL,
        0,
        WSA_FLAG_OVERLAPPED);

    if (INVALID_SOCKET == accept_socket)
    {
        goto ERROR_DEAL;
    }

    memset(&(iocp_listen_data_ptr->data.over_lapped), 0, sizeof(iocp_listen_data_ptr->data.over_lapped));

    iocp_listen_data_ptr->accept_socket = accept_socket;
    iocp_listen_data_ptr->data.pt.listener = listener;

    ret = listener->mgr->func_acceptex(
        listener->tcp_socket,
        accept_socket,
        iocp_listen_data_ptr->data.wsa_buf.buf,
        0,
        sizeof(struct sockaddr_in6) + 16,
        sizeof(struct sockaddr_in6) + 16,
        &bytes,
        &(iocp_listen_data_ptr->data.over_lapped));

    if (!ret)
    {
        if (WSA_IO_PENDING != WSAGetLastError())
        {
            goto ERROR_DEAL;
        }
    }

    return true;

ERROR_DEAL:

    iocp_listen_data_ptr->data.pt.listener = 0;
    if (accept_socket != INVALID_SOCKET)
    {
        closesocket(accept_socket);
    }

    return false;
}

void _iocp_tcp_listener_on_accept(iocp_tcp_listener* listener, BOOL ret, struct st_iocp_listen_data* iocp_listen_data_ptr)
{
    iocp_tcp_socket* sock_ptr;

    SOCKET accept_socket;

    char addr_cache[MAX_ADDR_SIZE];

    struct sockaddr_in6* peer_addr = NULL;
    struct sockaddr_in6* local_addr = NULL;

    INT32 remote_addr_len = 0;
    INT32 local_addr_len = 0;

    iocp_listen_data_ptr->data.pt.listener = 0;

    if (listener->state != SOCKET_STATE_LISTEN)
    {
        closesocket(iocp_listen_data_ptr->accept_socket);
        return;
    }

    memcpy(addr_cache, iocp_listen_data_ptr->data.wsa_buf.buf, MAX_ADDR_SIZE);

    accept_socket = iocp_listen_data_ptr->accept_socket;

    if (!_iocp_tcp_listener_post_accept_ex(listener, iocp_listen_data_ptr))
    {
    }

    if (!ret)
    {
        closesocket(accept_socket);
        return;
    }

    sock_ptr = _iocp_tcp_manager_alloc_socket(listener->mgr, listener->recv_buf_size, listener->send_buf_size);

    if (!sock_ptr)
    {
        closesocket(accept_socket);
        return;
    }

    listener->mgr->func_getacceptexsockaddrs(
        addr_cache,
        0,
        sizeof(struct sockaddr_in6) + 16,
        sizeof(struct sockaddr_in6) + 16,
        (SOCKADDR**)&local_addr,
        &local_addr_len,
        (SOCKADDR**)&peer_addr,
        &remote_addr_len);

    sock_ptr->tcp_socket = accept_socket;

    sock_ptr->on_establish = listener->on_establish;
    sock_ptr->on_terminate = listener->on_terminate;
    sock_ptr->on_error = listener->on_error;
    sock_ptr->on_recv = listener->on_recv;
    sock_ptr->pkg_parser = listener->pkg_parser;
    //sock_ptr->ssl_data = 0;

    

    if (!_iocp_tcp_socket_bind_iocp_port(sock_ptr))
    {
        closesocket(accept_socket);
        _iocp_tcp_manager_free_socket(listener->mgr, sock_ptr);
        return;
    }

    memcpy(&sock_ptr->local_sockaddr, local_addr, 
        min(local_addr_len, sizeof(sock_ptr->local_sockaddr)));
    memcpy(&sock_ptr->peer_sockaddr, peer_addr, 
        min(local_addr_len, sizeof(sock_ptr->peer_sockaddr)));

    sock_ptr->iocp_recv_data.operation = IOCP_OPT_RECV;
    sock_ptr->iocp_send_data.operation = IOCP_OPT_SEND;

    sock_ptr->state = SOCKET_STATE_ESTABLISH;

    int no_delay = 1;
    setsockopt(sock_ptr->tcp_socket, IPPROTO_TCP,
        TCP_NODELAY, (char*)&no_delay, sizeof(no_delay));

    _push_establish_event(listener, sock_ptr);

    if (listener->svr_ssl_ctx)
    {
        sock_ptr->ssl_data_ptr = _iocp_tcp_manager_alloc_ssl_data(sock_ptr->mgr, DEF_SSL_RECV_CACHE_SIZE, DEF_SSL_SEND_CACHE_SIZE);
        sock_ptr->ssl_data_ptr->ssl_pt.ssl_listener = listener;

        if (!init_iocp_server_ssl_data(sock_ptr->ssl_data_ptr, listener->svr_ssl_ctx))
        {
            _iocp_tcp_socket_close(sock_ptr, error_ssl, ERR_get_error(), true);
        }
        else
        {
            if (!_iocp_tcp_socket_post_ssl_recv(sock_ptr))
            {
                _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
            }
        }
    }
    else
    {
        if (!_iocp_tcp_socket_post_recv(sock_ptr))
        {
            _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), true);
        }
    }
}

void net_tcp_close_listener(iocp_tcp_listener* listener)
{
    unsigned int i;
    listener->state = SOCKET_STATE_NONE;

    if (listener->tcp_socket != INVALID_SOCKET)
    {
        closesocket(listener->tcp_socket);
        listener->tcp_socket = INVALID_SOCKET;
    }

CHECK_POST_ACCEPT_BEGIN:

    for (i = 0; i < listener->max_accept_ex_num; i++)
    {
        if (listener->arry_iocp_data[i].data.pt.listener)
        {
            Sleep(50);
            goto CHECK_POST_ACCEPT_BEGIN;
        }
    }

    if (listener->arry_addr_cache)
    {
        free(listener->arry_addr_cache);
        listener->arry_addr_cache = 0;
    }

    if (listener->arry_iocp_data)
    {
        free(listener->arry_iocp_data);
        listener->arry_iocp_data = 0;
    }

    free(listener);
}

bool _iocp_tcp_listener_listen(iocp_tcp_listener* listener, unsigned int max_accept_ex_num, const char* ip, UINT16 port, bool bReUseAddr)
{
    char sz_port[64];
    unsigned int i;
    struct addrinfo hints;
    struct addrinfo* result = 0;

    

    listener->arry_addr_cache = NULL;
    listener->arry_iocp_data = NULL;
    listener->max_accept_ex_num = 0;

    ZeroMemory(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    _itoa_s(port, sz_port, sizeof(sz_port), 10);

    if (getaddrinfo(ip, sz_port, &hints, &result))
    {
        goto ERROR_DEAL;
    }

    listener->tcp_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    memcpy(&listener->listen_sockaddr, result->ai_addr, min(sizeof(listener->listen_sockaddr), result->ai_addrlen));

    if (INVALID_SOCKET == listener->tcp_socket)
    {
        goto ERROR_DEAL;
    }

    if (bReUseAddr)
    {
        INT32 nReuse = 1;
        if (setsockopt(listener->tcp_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof(nReuse)) == SOCKET_ERROR)
        {
            goto ERROR_DEAL;
        }
    }

    if (SOCKET_ERROR == bind(listener->tcp_socket, result->ai_addr, (int)result->ai_addrlen))
    {
        goto ERROR_DEAL;
    }

    if (SOCKET_ERROR == listen(listener->tcp_socket, MAX_BACKLOG))
    {
        goto ERROR_DEAL;
    }

    if (!_iocp_tcp_listener_bind_iocp_port(listener))
    {
        goto ERROR_DEAL;
    }

    listener->max_accept_ex_num = max(max_accept_ex_num, 256);

    listener->arry_addr_cache = (char*)malloc(listener->max_accept_ex_num*MAX_ADDR_SIZE);
    if (!listener->arry_addr_cache)
    {
        goto ERROR_DEAL;
    }

    listener->arry_iocp_data = (struct st_iocp_listen_data*)malloc(sizeof(struct st_iocp_listen_data)*listener->max_accept_ex_num);
    if (!listener->arry_iocp_data)
    {
        goto ERROR_DEAL;
    }

    listener->state = SOCKET_STATE_LISTEN;

    for (i = 0; i < listener->max_accept_ex_num; i++)
    {
        listener->arry_iocp_data[i].data.wsa_buf.buf = listener->arry_addr_cache + i * MAX_ADDR_SIZE;
        listener->arry_iocp_data[i].data.wsa_buf.len = MAX_ADDR_SIZE;
        listener->arry_iocp_data[i].data.operation = IOCP_OPT_ACCEPT;
        listener->arry_iocp_data[i].data.pt.listener = 0;


        if (!_iocp_tcp_listener_post_accept_ex(listener, listener->arry_iocp_data + i))
        {
            return false;
        }
    }

    if (result)
    {
        freeaddrinfo(result);
        result = 0;
    }

    return true;

ERROR_DEAL:

    if (result)
    {
        freeaddrinfo(result);
        result = 0;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////

bool _proc_net_event(iocp_tcp_manager* mgr)
{
    struct st_net_event* evt;
    size_t evt_len = sizeof(struct st_net_event);

    loop_cache_get_data(mgr->evt_queue, &evt, &evt_len);

    if (evt_len < sizeof(struct st_net_event))
    {
        return false;
    }

    if (evt->socket_ptr->mgr != mgr)
    {
        CRUSH_CODE();
    }

    iocp_tcp_socket* sock_ptr = evt->socket_ptr;

    switch (evt->type)
    {
    case NET_EVENT_DATA:
    {
        char* data_ptr = 0;
        size_t data_len;
        unsigned int parser_len = 0;

        sock_ptr->data_has_recv += evt->evt.evt_data.data_len;

        data_len = sock_ptr->data_has_recv;

        loop_cache_get_data(sock_ptr->recv_loop_cache, &data_ptr, &data_len);

        if ((unsigned int)data_len < sock_ptr->data_has_recv)
        {
            if (sock_ptr->data_has_recv > mgr->max_pkg_buf_size)
            {
                for (;;)
                {
                    mgr->max_pkg_buf_size += 1024;

                    if (mgr->max_pkg_buf_size > sock_ptr->data_has_recv)
                    {
                        free(mgr->max_pkg_buf);
                        mgr->max_pkg_buf = (char*)malloc(mgr->max_pkg_buf_size);
                        break;
                    }
                }
            }

            if (!loop_cache_copy_data(sock_ptr->recv_loop_cache, mgr->max_pkg_buf, sock_ptr->data_has_recv))
            {
                CRUSH_CODE();
            }

            data_ptr = mgr->max_pkg_buf;
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
                    _iocp_tcp_socket_close(sock_ptr, error_packet, 0, false);
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
        _mod_timer_send(sock_ptr, DELAY_SEND_CHECK);
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
        if ((!sock_ptr->ssl_data_ptr) || (sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE))
        {
            sock_ptr->on_error(sock_ptr, evt->evt.evt_module_error.err_code, 0);
            sock_ptr->on_terminate(sock_ptr);
        }

        _mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
    }
    break;
    case NET_EVENT_SSL_ERROR:
    {
        if (sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE)
        {
            sock_ptr->on_error(sock_ptr, error_ssl, evt->evt.evt_ssl_error.err_code);
            sock_ptr->on_terminate(sock_ptr);
        }

        _mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
    }
    break;
    case NET_EVENT_SYSTEM_ERROR:
    {
        if ((!sock_ptr->ssl_data_ptr) || (sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE))
        {
            sock_ptr->on_error(sock_ptr, error_system, evt->evt.evt_system_error.err_code);
            sock_ptr->on_terminate(sock_ptr);
        }

        _mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
    }
    break;
    case NET_EVENT_TERMINATE:
    {
        if ((!sock_ptr->ssl_data_ptr) || (sock_ptr->ssl_data_ptr->ssl_state == SSL_HAND_SHAKE))
        {
            sock_ptr->on_terminate(sock_ptr);
        }

        _mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
    }
    break;
    case NET_EVENT_CONNECT_FAIL:
    {
        sock_ptr->on_error(sock_ptr, error_connect_fail, evt->evt.evt_connect_fail.err_code);

        _mod_timer_close(sock_ptr, DELAY_CLOSE_SOCKET);
    }
    break;
    case NET_EVENT_RECV_ACTIVE:
    {
        if (sock_ptr->state == SOCKET_STATE_ESTABLISH)
        {
            if (loop_cache_free_size(sock_ptr->recv_loop_cache))
            {
                if (!_iocp_tcp_socket_post_recv_req(sock_ptr))
                {
                    _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), false);
                }
            }
            else
            {
                _iocp_tcp_socket_close(sock_ptr, error_recv_overflow, 0, false);
            }
        }
    }
    break;
    }

    if (!loop_cache_pop(mgr->evt_queue, sizeof(struct st_net_event)))
    {
        CRUSH_CODE();
    }

    return true;
}

unsigned WINAPI _iocp_thread_func(LPVOID param)
{
    iocp_tcp_manager* mgr = (iocp_tcp_manager*)param;

    struct st_iocp_data* iocp_data_ptr;
    iocp_tcp_socket* iocp_tcp_socket_ptr;
    BOOL ret;
    DWORD byte_transferred;

    for (;;)
    {
        iocp_data_ptr = 0;
        iocp_tcp_socket_ptr = 0;
        byte_transferred = 0;

        ret = GetQueuedCompletionStatus(
            mgr->iocp_port,
            &byte_transferred,
            (PULONG_PTR)&iocp_tcp_socket_ptr,
            (LPOVERLAPPED*)&iocp_data_ptr,
            INFINITE);

        if (!iocp_data_ptr)
        {
            return 0;
        }

        switch (iocp_data_ptr->operation)
        {
        case IOCP_OPT_RECV:
        {
            if (iocp_data_ptr->pt.sock_ptr->ssl_data_ptr)
            {
                _iocp_tcp_socket_on_ssl_recv(iocp_data_ptr->pt.sock_ptr, ret, byte_transferred);
            }
            else
            {
                _iocp_tcp_socket_on_recv(iocp_data_ptr->pt.sock_ptr, ret, byte_transferred);
            }
        }
        break;
        case IOCP_OPT_SEND:
        {
            if (iocp_data_ptr->pt.sock_ptr->ssl_data_ptr)
            {
                _iocp_tcp_socket_on_ssl_send(iocp_data_ptr->pt.sock_ptr, ret, byte_transferred);
            }
            else
            {
                _iocp_tcp_socket_on_send(iocp_data_ptr->pt.sock_ptr, ret, byte_transferred);
            }
        }
        break;
        case IOCP_OPT_ACCEPT:
        {
            struct st_iocp_listen_data* iocp_listen_data_ptr = (struct st_iocp_listen_data*)iocp_data_ptr;
            _iocp_tcp_listener_on_accept(iocp_listen_data_ptr->data.pt.listener, ret, iocp_listen_data_ptr);
        }
        break;
        case IOCP_OPT_CONNECT_REQ:
        {
            _iocp_tcp_socket_on_connect_req(iocp_data_ptr->pt.sock_ptr);
        }
        break;
        case IOCP_OPT_CONNECT:
        {
            _iocp_tcp_socket_on_connect(iocp_data_ptr->pt.sock_ptr, ret);
        }
        break;
        case IOCP_OPT_ACCEPT_REQ:
            break;
        case IOCP_OPT_CLOSE_REQ:
        {
            iocp_data_ptr->pt.sock_ptr->send_ack++;
            _iocp_tcp_socket_on_close_req(iocp_data_ptr->pt.sock_ptr);
        }
        break;
        }
    }
}

bool _start_iocp_thread(iocp_tcp_manager* mgr)
{
    unsigned int i;

    if (!mgr->work_thread_num)
    {
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);

        mgr->work_thread_num = sys_info.dwNumberOfProcessors * 2 + 2;
    }

    mgr->work_threads = (HANDLE*)malloc(sizeof(HANDLE)*mgr->work_thread_num);

    for (i = 0; i < mgr->work_thread_num; i++)
    {
        mgr->work_threads[i] = NULL;
    }

    for (i = 0; i < mgr->work_thread_num; i++)
    {
        unsigned thread_id = 0;

        mgr->work_threads[i] = (HANDLE)_beginthreadex(NULL,
            0,
            _iocp_thread_func,
            mgr,
            0,
            &thread_id);

        if (!mgr->work_threads[i])
        {
            return false;
        }
    }

    return true;
}

void _stop_iocp_thread(iocp_tcp_manager* mgr)
{
    unsigned int i;
    ULONG_PTR ptr = 0;
    for (i = 0; i < mgr->work_thread_num; i++)
    {
        if (mgr->work_threads[i])
        {

            PostQueuedCompletionStatus(mgr->iocp_port, 0, ptr, NULL);
        }
    }

    for (i = 0; i < mgr->work_thread_num; i++)
    {
        if (mgr->work_threads[i])
        {
            WaitForSingleObject(mgr->work_threads[i], INFINITE);
            CloseHandle(mgr->work_threads[i]);
            mgr->work_threads[i] = NULL;
        }
    }

    free(mgr->work_threads);
}

void _iocp_tcp_socket_on_timer_send(iocp_tcp_socket* sock_ptr)
{
    if (sock_ptr->state == SOCKET_STATE_ESTABLISH)
    {
        if (sock_ptr->data_need_send != sock_ptr->data_has_send)
        {
            if (sock_ptr->send_req == sock_ptr->send_ack)
            {
                if (!_iocp_tcp_socket_post_send_req(sock_ptr))
                {
                    _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), false);
                }
            }
        }
    }
}

void _iocp_tcp_socket_on_timer_close(iocp_tcp_socket* sock_ptr)
{
    switch (sock_ptr->state)
    {
    case SOCKET_STATE_CLOSE:
    {
        if (sock_ptr->error == error_ok)
        {
            if (sock_ptr->data_need_send != sock_ptr->data_has_send)
            {
                if (sock_ptr->send_req == sock_ptr->send_ack)
                {
                    if (!_iocp_tcp_socket_post_send_req(sock_ptr))
                    {
                        sock_ptr->data_need_send = sock_ptr->data_has_send;
                    }
                }

                return;
            }
        }

        if (sock_ptr->send_req == sock_ptr->send_ack)
        {
            if (_iocp_tcp_socket_post_close_req(sock_ptr))
            {
                sock_ptr->state = SOCKET_STATE_TERMINATE;
                timer_del(sock_ptr->timer_close);
                sock_ptr->timer_close = 0;
            }
        }
    }
    break;
    case SOCKET_STATE_TERMINATE:
    {
        if (sock_ptr->tcp_socket != INVALID_SOCKET)
        {
            CRUSH_CODE();
        }

        if ((sock_ptr->recv_req == sock_ptr->recv_ack) && (sock_ptr->send_req == sock_ptr->send_ack))
        {
            timer_del(sock_ptr->timer_close);
            sock_ptr->timer_close = 0;
            if (sock_ptr->ssl_data_ptr)
            {
                _iocp_tcp_manager_free_ssl_data(sock_ptr->mgr, sock_ptr->ssl_data_ptr);
            }
            _iocp_tcp_manager_free_socket(sock_ptr->mgr, sock_ptr);
        }
    }
    break;
    }
}

void _iocp_tcp_on_timer(HTIMERINFO timer)
{
    iocp_tcp_socket* sock_ptr = (iocp_tcp_socket*)timer_get_data(timer);

    if (!sock_ptr)
    {
        CRUSH_CODE();
    }

    if (sock_ptr->timer_send == timer)
    {
        _iocp_tcp_socket_on_timer_send(sock_ptr);
    }
    else if (sock_ptr->timer_close == timer)
    {
        _iocp_tcp_socket_on_timer_close(sock_ptr);
    }
    else
    {
        CRUSH_CODE();
    }
}

bool _set_wsa_function(iocp_tcp_manager* mgr)
{
    DWORD bytes;

    GUID func_guid = WSAID_CONNECTEX;

    GUID func_guid1 = WSAID_ACCEPTEX;

    GUID func_guid2 = WSAID_GETACCEPTEXSOCKADDRS;

    SOCKET tmp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (INVALID_SOCKET == tmp_sock)
    {
        return false;
    }

    if (WSAIoctl(tmp_sock,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &func_guid,
        sizeof(func_guid),
        &mgr->func_connectex,
        sizeof(mgr->func_connectex),
        &bytes, 0, 0))
    {
        goto ERROR_DEAL;
    }

    if (WSAIoctl(tmp_sock,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &func_guid1,
        sizeof(func_guid1),
        &mgr->func_acceptex,
        sizeof(mgr->func_acceptex),
        &bytes, 0, 0))
    {
        goto ERROR_DEAL;
    }

    if (WSAIoctl(tmp_sock,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &func_guid2,
        sizeof(func_guid2),
        &mgr->func_getacceptexsockaddrs,
        sizeof(mgr->func_getacceptexsockaddrs),
        &bytes, 0, 0))
    {
        goto ERROR_DEAL;
    }

    closesocket(tmp_sock);

    return true;

ERROR_DEAL:

    closesocket(tmp_sock);

    return false;
}

void destroy_net_tcp(iocp_tcp_manager* mgr)
{
    _stop_iocp_thread(mgr);

    if (mgr->max_pkg_buf)
    {
        free(mgr->max_pkg_buf);
        mgr->max_pkg_buf_size = 0;
    }

    if (mgr->memory_mgr)
    {
        HRBNODE mem_node = rb_first(mgr->memory_mgr);
        while (mem_node)
        {
            destroy_memory_unit((HMEMORYUNIT)rb_node_value_user(mem_node));

            mem_node = rb_next(mem_node);
        }

        destroy_memory_unit(rb_node_unit(mgr->memory_mgr));
        destroy_memory_unit(rb_tree_unit(mgr->memory_mgr));
        mgr->memory_mgr = 0;
    }

    if (mgr->timer_mgr)
    {
        destroy_timer_manager(mgr->timer_mgr);
        mgr->timer_mgr = 0;
    }

    if (mgr->socket_pool)
    {
        destroy_memory_unit(mgr->socket_pool);
        mgr->socket_pool = 0;
    }

    if (mgr->iocp_port)
    {
        CloseHandle(mgr->iocp_port);
        mgr->iocp_port = 0;
    }

    WSACleanup();

    DeleteCriticalSection(&mgr->evt_lock);
    DeleteCriticalSection(&mgr->mem_lock);

    free(mgr);
}

iocp_tcp_manager* create_net_tcp(pfn_on_establish func_on_establish, pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error, pfn_on_recv func_on_recv, pfn_parse_packet func_parse_packet,
    unsigned int max_socket_num, unsigned int max_io_thread_num, unsigned int max_accept_ex_num)
{
    WORD version_requested;
    WSADATA wsa_data;

    struct st_iocp_tcp_socket** sock_ptr_array = 0;

    iocp_tcp_manager* mgr = (iocp_tcp_manager*)malloc(sizeof(struct st_iocp_tcp_manager));

    mgr->max_socket_num = max_socket_num;
    mgr->work_thread_num = max_io_thread_num;
    mgr->max_accept_ex_num = max_accept_ex_num;

    mgr->def_on_establish = func_on_establish;
    mgr->def_on_terminate = func_on_terminate;
    mgr->def_on_error = func_on_error;
    mgr->def_on_recv = func_on_recv;
    mgr->def_parse_packet = func_parse_packet;

    InitializeCriticalSection(&mgr->evt_lock);
    InitializeCriticalSection(&mgr->mem_lock);

    version_requested = MAKEWORD(2, 2);

    if (WSAStartup(version_requested, &wsa_data))
    {
        goto ERROR_DEAL;
    }

    if (!_set_wsa_function(mgr))
    {
        goto ERROR_DEAL;
    }

    mgr->iocp_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    if (!mgr->iocp_port)
    {
        goto ERROR_DEAL;
    }

    mgr->socket_pool = create_memory_unit(sizeof(struct st_iocp_tcp_socket));
    memory_unit_set_grow_count(mgr->socket_pool, mgr->max_socket_num);

    sock_ptr_array = (struct st_iocp_tcp_socket**)malloc(sizeof(struct st_iocp_tcp_socket*) * mgr->max_socket_num);
    for (unsigned int i = 0; i < mgr->max_socket_num; i++)
    {
        sock_ptr_array[i] = (struct st_iocp_tcp_socket*)_multi_thread_alloc(mgr->socket_pool);
        sock_ptr_array[i]->mgr = mgr;
        sock_ptr_array[i]->recv_loop_cache = 0;
        sock_ptr_array[i]->send_loop_cache = 0;
        sock_ptr_array[i]->iocp_recv_data.pt.sock_ptr = sock_ptr_array[i];
        sock_ptr_array[i]->iocp_send_data.pt.sock_ptr = sock_ptr_array[i];
    }

    memory_unit_set_grow_count(mgr->socket_pool, 0);

    for (unsigned int i = 0; i < mgr->max_socket_num; i++)
    {
        memory_unit_free(mgr->socket_pool, sock_ptr_array[i]);
    }
    free(sock_ptr_array);
    sock_ptr_array = 0;


    mgr->evt_queue = create_loop_cache(0, mgr->max_socket_num * 5 * sizeof(struct st_net_event));
    if (!mgr->evt_queue)
    {
        goto ERROR_DEAL;
    }

    mgr->timer_mgr = create_timer_manager(_iocp_tcp_on_timer);
    if (!mgr->timer_mgr)
    {
        goto ERROR_DEAL;
    }

    mgr->memory_mgr = create_rb_tree_ex(0,
        create_memory_unit(sizeof_rb_tree()), create_memory_unit(sizeof_rb_node()));
    if (!mgr->memory_mgr)
    {
        goto ERROR_DEAL;
    }

    mgr->max_pkg_buf_size = 64*1024;
    mgr->max_pkg_buf = (char*)malloc(mgr->max_pkg_buf_size);

    if (!_start_iocp_thread(mgr))
    {
        goto ERROR_DEAL;
    }

    return mgr;

ERROR_DEAL:
    destroy_net_tcp(mgr);
    return 0;
}

bool _iocp_tcp_connector_connect(iocp_tcp_socket* socket_ptr,
    const char * ip,
    unsigned short port,
    bool reuse_addr,
    const char * bind_ip,
    unsigned short bind_port)
{

	size_t ip_len = 0;
	size_t bind_ip_len = 0;

	if (ip)
	{
        ip_len = min((strlen(ip) + 1), (MAX_IP_LEN - 1));
	}

	if (bind_ip)
	{
        bind_ip_len = min((strlen(bind_ip) + 1), (MAX_IP_LEN - 1));
	}

	loop_cache_push_data(socket_ptr->recv_loop_cache, &ip_len, sizeof(ip_len));
    loop_cache_push_data(socket_ptr->recv_loop_cache, &port, sizeof(port));
	loop_cache_push_data(socket_ptr->recv_loop_cache, ip, ip_len);

	loop_cache_push_data(socket_ptr->recv_loop_cache, &bind_ip_len, sizeof(bind_ip_len));
    if (bind_ip_len)
    {
		loop_cache_push_data(socket_ptr->recv_loop_cache, &reuse_addr, sizeof(reuse_addr));
		loop_cache_push_data(socket_ptr->recv_loop_cache, &bind_port, sizeof(bind_port));
        loop_cache_push_data(socket_ptr->recv_loop_cache, bind_ip, bind_ip_len);
    }

    socket_ptr->state = SOCKET_STATE_CONNECT;

    if (!_iocp_tcp_socket_post_connect_req(socket_ptr))
    {
        return false;
    }

    return true;
}

iocp_tcp_socket* net_tcp_connect(
    iocp_tcp_manager* mgr,
    const char * ip,
    unsigned short port,
    unsigned int recv_buf_size,
    unsigned int send_buf_size,
    bool reuse_addr,
    const char * bind_ip,
    unsigned short bind_port,
    pfn_on_establish func_on_establish,
    pfn_on_terminate func_on_terminate,
    pfn_on_error func_on_error,
    pfn_on_recv func_on_recv,
    pfn_parse_packet func_parse_packet)
{
    iocp_tcp_socket* socket_ptr = _iocp_tcp_manager_alloc_socket(mgr, recv_buf_size, send_buf_size);

    if (!socket_ptr)
    {
        return 0;
    }
    else
    {
        if (mgr != socket_ptr->mgr)
        {
            CRUSH_CODE();
        }
    }

    if (func_on_establish)
    {
        socket_ptr->on_establish = func_on_establish;
    }
    else
    {
        socket_ptr->on_establish = mgr->def_on_establish;
    }

    if (func_on_terminate)
    {
        socket_ptr->on_terminate = func_on_terminate;
    }
    else
    {
        socket_ptr->on_terminate = mgr->def_on_terminate;
    }

    if (func_on_error)
    {
        socket_ptr->on_error = func_on_error;
    }
    else
    {
        socket_ptr->on_error = mgr->def_on_error;
    }

    if (func_on_recv)
    {
        socket_ptr->on_recv = func_on_recv;
    }
    else
    {
        socket_ptr->on_recv = mgr->def_on_recv;
    }

    if (func_parse_packet)
    {
        socket_ptr->pkg_parser = func_parse_packet;
    }
    else
    {
        socket_ptr->pkg_parser = mgr->def_parse_packet;
    }

    if (!_iocp_tcp_connector_connect(socket_ptr, ip, port, reuse_addr, bind_ip, bind_port))
    {
        _iocp_tcp_manager_free_socket(mgr, socket_ptr);

        return 0;
    }

    return socket_ptr;
}

extern SSL_CTX* def_client_ssl_ctx;

iocp_tcp_socket* net_ssl_connect(
    iocp_tcp_manager* mgr,
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
    iocp_tcp_socket* socket_ptr = _iocp_tcp_manager_alloc_socket(mgr, recv_buf_size, send_buf_size);
    if (!socket_ptr)
    {
        return 0;
    }
    else
    {
        if (mgr != socket_ptr->mgr)
        {
            CRUSH_CODE();
        }
    }

    socket_ptr->ssl_data_ptr = _iocp_tcp_manager_alloc_ssl_data(mgr, DEF_SSL_RECV_CACHE_SIZE, DEF_SSL_SEND_CACHE_SIZE);
    if (!socket_ptr->ssl_data_ptr)
    {
        _iocp_tcp_manager_free_socket(mgr, socket_ptr);
        return 0;
    }
    else
    {
        if (cli_ssl_ctx)
        {
            socket_ptr->ssl_data_ptr->ssl_pt.ssl_client_ctx = cli_ssl_ctx;
        }
        else
        {
            socket_ptr->ssl_data_ptr->ssl_pt.ssl_client_ctx = def_client_ssl_ctx;
        }
        
    }

    if (func_on_establish)
    {
        socket_ptr->on_establish = func_on_establish;
    }
    else
    {
        socket_ptr->on_establish = mgr->def_on_establish;
    }

    if (func_on_terminate)
    {
        socket_ptr->on_terminate = func_on_terminate;
    }
    else
    {
        socket_ptr->on_terminate = mgr->def_on_terminate;
    }

    if (func_on_error)
    {
        socket_ptr->on_error = func_on_error;
    }
    else
    {
        socket_ptr->on_error = mgr->def_on_error;
    }

    if (func_on_recv)
    {
        socket_ptr->on_recv = func_on_recv;
    }
    else
    {
        socket_ptr->on_recv = mgr->def_on_recv;
    }

    if (func_parse_packet)
    {
        socket_ptr->pkg_parser = func_parse_packet;
    }
    else
    {
        socket_ptr->pkg_parser = mgr->def_parse_packet;
    }

    if (!_iocp_tcp_connector_connect(socket_ptr, ip, port, reuse_addr, bind_ip, bind_port))
    {
        _iocp_tcp_manager_free_ssl_data(mgr, socket_ptr->ssl_data_ptr);
        _iocp_tcp_manager_free_socket(mgr, socket_ptr);

        return 0;
    }

    return socket_ptr;
}


iocp_tcp_listener* net_tcp_listen(
    iocp_tcp_manager* mgr,
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
    iocp_tcp_listener* listener = (iocp_tcp_listener*)malloc(sizeof(struct st_iocp_tcp_listener));

    listener->recv_buf_size = recv_buf_size;
    listener->send_buf_size = send_buf_size;
    listener->svr_ssl_ctx = 0;

    listener->mgr = mgr;

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

    if (!_iocp_tcp_listener_listen(listener, mgr->max_accept_ex_num, ip, port, reuse_addr))
    {
        net_tcp_close_listener(listener);

        return 0;
    }

    return listener;
}

iocp_tcp_listener* net_ssl_listen(
	iocp_tcp_manager* mgr,
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
    iocp_tcp_listener* listener = (iocp_tcp_listener*)malloc(sizeof(struct st_iocp_tcp_listener));

    listener->recv_buf_size = recv_buf_size;
    listener->send_buf_size = send_buf_size;
    listener->svr_ssl_ctx = svr_ssl_ctx;

    listener->mgr = mgr;

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

    if (!_iocp_tcp_listener_listen(listener, mgr->max_accept_ex_num, ip, port, reuse_addr))
    {
        net_tcp_close_listener(listener);

        return 0;
    }

    return listener;
}



bool net_tcp_send(iocp_tcp_socket* sock_ptr, const void* data, unsigned int len)
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
        _iocp_tcp_socket_close(sock_ptr, error_send_overflow, 0, false);
        return false;
    }

    sock_ptr->data_need_send += len;

    if ((sock_ptr->data_need_send - sock_ptr->data_has_send) < sock_ptr->data_delay_send_size)
    {
        return true;
    }

    if (sock_ptr->send_req == sock_ptr->send_ack)
    {
        if (!_iocp_tcp_socket_post_send_req(sock_ptr))
        {
            _iocp_tcp_socket_close(sock_ptr, error_system, WSAGetLastError(), false);
            return false;
        }
    }

    return true;
}

void net_tcp_close_session(iocp_tcp_socket* sock_ptr)
{
    _iocp_tcp_socket_close(sock_ptr, error_ok, 0, false);
}

bool net_tcp_run(iocp_tcp_manager* mgr, unsigned int run_time)
{
    unsigned int tick = get_tick();

    for (;;)
    {
        timer_update(mgr->timer_mgr, 0);

        if (!_proc_net_event(mgr))
        {
            return false;
        }

        if (run_time)
        {
            if (get_tick() - tick > run_time)
            {
                break;
            }
        }
    }

    return true;
}

SOCKET net_tcp_session_socket(iocp_tcp_socket* sock_ptr)
{
    return sock_ptr->tcp_socket;
}

SOCKET net_tcp_listener_socket(iocp_tcp_listener* listener)
{
    return listener->tcp_socket;
}

void net_tcp_set_listener_data(iocp_tcp_listener* listener, void* user_data)
{
    listener->user_data = user_data;
}

void net_tcp_set_session_data(iocp_tcp_socket* sock_ptr, void* user_data)
{
    sock_ptr->user_data = user_data;
}

void* net_tcp_get_listener_data(iocp_tcp_listener* listener)
{
    return listener->user_data;
}

void* net_tcp_get_session_data(iocp_tcp_socket* sock_ptr)
{
    return sock_ptr->user_data;
}

bool net_tcp_get_peer_ip_port(iocp_tcp_socket* sock_ptr, ip_info* info)
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

bool net_tcp_get_local_ip_port(iocp_tcp_socket* sock_ptr, ip_info* info)
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

bool net_tcp_get_peer_sock_addr(iocp_tcp_socket* sock_ptr, addr_info* info)
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

bool net_tcp_get_local_sock_addr(iocp_tcp_socket* sock_ptr, addr_info* info)
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

unsigned int net_tcp_get_send_free_size(iocp_tcp_socket* sock_ptr)
{
    return (unsigned int)loop_cache_free_size(sock_ptr->send_loop_cache);
}

void net_tcp_set_send_control(iocp_tcp_socket* sock_ptr, unsigned int pkg_size, unsigned int delay_time)
{
    sock_ptr->data_delay_send_size = pkg_size;
    _mod_timer_send(sock_ptr, delay_time);
}

#endif
