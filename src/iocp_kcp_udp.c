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
#include "../include/lock_free_queue.hpp"
#include "../include/net_kcp.h"


//0               4   5   6       8 (BYTE)
//+---------------+---+---+-------+
//|      conv     |cmd|frg|  wnd  |
//+---------------+---+---+-------+8
//|      ts       |      sn       |
//+---------------+---------------+16
//|      una      |      len      |
//+---------------+---------------+24
//|                               |
//|        DATA(optional)         |
//|                               |
//+-------------------------------+

#define KCP_SOCKET_STATE_NONE       0

typedef struct st_iocp_data
{
    OVERLAPPED  over_lapped;
    WSABUF      wsa_buf;
    int         operation;
    union
    {
        struct st_iocp_kcp_udp_listener*    listener;
        struct st_iocp_kcp_udp_socket*      sock_ptr;
    }pt;
}iocp_data;

typedef struct st_iocp_kcp_udp_listener
{
    SOCKET                          udp_socket;
    unsigned int                    send_buf_size;
    unsigned int                    recv_buf_size;
    unsigned int                    max_accept_ex_num;
    pfn_parse_packet                pkg_parser;
    pfn_on_establish                on_establish;
    pfn_on_terminate                on_terminate;
    pfn_on_error                    on_error;
    pfn_on_recv                     on_recv;
    char*                           arry_addr_cache;
    struct st_iocp_data*            arry_iocp_data;
    LONG                            state;
    void*                           user_data;
    struct st_iocp_kcp_udp_manager* mgr;
    struct sockaddr_in6             listen_sockaddr;

}iocp_kcp_udp_listener;

typedef struct st_iocp_kcp_udp_socket
{
    SOCKET                          udp_socket;
    void*                           user_data;

    struct st_iocp_data             iocp_recv_data;
    struct st_iocp_data             iocp_send_data;

    HLOOPCACHE                      recv_loop_cache;
    HLOOPCACHE                      send_loop_cache;

    unsigned int                    recv_req;
    unsigned int                    recv_ack;
    LONG                            send_req;
    LONG                            send_ack;

    LONG                            state;

    unsigned int                    data_need_send;
    unsigned int                    data_has_send;
    unsigned int                    data_has_recv;

    unsigned int                    data_delay_send_size;

    struct sockaddr_in6             local_sockaddr;
    struct sockaddr_in6             peer_sockaddr;

    HTIMERINFO                      timer_send;
    HTIMERINFO                      timer_close;

    pfn_parse_kcp_packet            kcp_pkg_parser;
    pfn_on_connect                  on_connect;
    pfn_on_disconnect               on_disconnect;
    pfn_on_wrong                    on_wrong;
    pfn_on_data                     on_data;

    net_kcp_error                   error;
    int                             err_code;
    struct st_iocp_kcp_dup_manager* mgr;
}iocp_kcp_udp_socket;

typedef struct st_iocp_kcp_udp_manager
{
    pfn_parse_kcp_packet            kcp_pkg_parser;
    pfn_on_connect                  on_connect;
    pfn_on_disconnect               on_disconnect;
    pfn_on_wrong                    on_wrong;
    pfn_on_data                     on_data;

    HANDLE                      iocp_port;

    unsigned int                work_thread_num;
    HANDLE*                     work_threads;

    CRITICAL_SECTION            evt_lock;
    CRITICAL_SECTION            mem_lock;

    HLOOPCACHE                  evt_queue;
    HTIMERMANAGER               timer_mgr;

    //HMEMORYUNIT                 socket_pool;
    iocp_kcp_udp_socket*        socket_array;
    HLOCKFREEPTRQUEUE           socket_pool;
    HRBTREE                     memory_mgr;

    unsigned int                max_socket_num;
    unsigned int                max_accept_ex_num;

    char*                       max_pkg_buf;
    unsigned int                max_pkg_buf_size;

}iocp_kcp_udp_manager;

void* _iocp_kcp_udp_manager_alloc_memory(iocp_kcp_udp_manager* mgr, unsigned int buffer_size)
{
    HMEMORYUNIT unit;
    HRBNODE mem_node = rb_tree_find_integer(mgr->memory_mgr, buffer_size);

    if (!mem_node)
    {
        EnterCriticalSection(&mgr->mem_lock);
        mem_node = rb_tree_find_integer(mgr->memory_mgr, buffer_size);
        if (!mem_node)
        {
            HLOCKFREEPTRQUEUE unit_queue = create_lock_free_ptr_queue(1);
            unit = create_memory_unit(buffer_size);

            lock_free_ptr_queue_push(unit_queue, unit);

            mem_node = rb_tree_insert_integer(mgr->memory_mgr, buffer_size, unit_queue);
        }
        LeaveCriticalSection(&mgr->mem_lock);
    }

    unit = lock_free_ptr_queue_pop((HLOCKFREEPTRQUEUE)rb_node_value_user(mem_node));
    while (!unit)
    {
        unit = lock_free_ptr_queue_pop((HLOCKFREEPTRQUEUE)rb_node_value_user(mem_node));
    }

    void* buffer = memory_unit_alloc(unit);

    lock_free_ptr_queue_push((HLOCKFREEPTRQUEUE)rb_node_value_user(mem_node), unit);

    return buffer;
}

void _iocp_kcp_udp_manager_free_memory(iocp_kcp_udp_manager* mgr, void* mem, unsigned int buffer_size)
{
    HRBNODE mem_node = rb_tree_find_integer(mgr->memory_mgr, buffer_size);
    HMEMORYUNIT unit = lock_free_ptr_queue_pop((HLOCKFREEPTRQUEUE)rb_node_value_user(mem_node));
    while (!unit)
    {
        unit = lock_free_ptr_queue_pop((HLOCKFREEPTRQUEUE)rb_node_value_user(mem_node));
    }

    if (!memory_unit_check(unit, mem))
    {
        CRUSH_CODE();
    }

    memory_unit_free(unit, mem);

    lock_free_ptr_queue_push((HLOCKFREEPTRQUEUE)rb_node_value_user(mem_node), unit);
}

void _iocp_kcp_udp_socket_reset(iocp_kcp_udp_socket* sock_ptr)
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

    //sock_ptr->local_addr_info = 0;AF_INET6
    //sock_ptr->peer_addr_info = 0;
    memset(&sock_ptr->local_sockaddr, 0, sizeof(sock_ptr->local_sockaddr));
    memset(&sock_ptr->peer_sockaddr, 0, sizeof(sock_ptr->peer_sockaddr));

    sock_ptr->timer_send = 0;
    sock_ptr->timer_close = 0;
}

#endif