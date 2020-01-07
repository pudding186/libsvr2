#pragma once
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include "./lib_svr_def.h"

#ifdef  __cplusplus
extern "C" {
#endif

extern void ssl_tcp_init(void);

extern SSL_CTX* create_server_ssl_ctx(const char* certificate_file, const char* private_key_file);

extern SSL_CTX* create_client_ssl_ctx(void);

extern void destroy_ssl_ctx(SSL_CTX* ssl_ctx);

extern void get_ssl_err_msg(unsigned long err, char* buf, size_t len);


extern HLISTENER(net_ssl_listen)(HTCPMANAGER mgr,
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
    pfn_parse_packet func_parse_packet);

extern HSESSION(net_ssl_connect)(HTCPMANAGER mgr,
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
    pfn_parse_packet func_parse_packet);

extern SSL_CTX* (net_ssl_change_ctx)(HLISTENER ssl_listener, 
    SSL_CTX* new_svr_ssl_ctx);

#ifdef  __cplusplus
}
#endif