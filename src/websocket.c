#include "openssl/sha.h"
#include "zlib.h"
#include "./lib_svr_common_def.h"
#include "../include/memory_pool.h"
#include "../include/websocket.h"
#include "../include/utility.hpp"
#include <stdint.h>
#include <stdio.h>

// 0                 1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//+-+-+-+-+-------+-+-------------+-------------------------------+
//|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
//|I|S|S|S|   (4) |A|     (7)     |           (16 / 64)           |
//|N|V|V|V|       |S|             | (if payload len == 126 / 127) |
//| |1|2|3|       |K|             |                               |
//+-+-+-+-+-------+-+-------------+-------------------------------+
//|   Extended payload length continued, if payload len == 127    |
//+-------------------------------+-------------------------------+
//|                               |Masking - key, if MASK set to 1:
//+-------------------------------+-------------------------------+
//:    Masking - key(continued)   |          Payload Data         |
//+---------------------------------------------------------------+
//:                     Payload Data continued ...                :
//+---------------------------------------------------------------+
//:                     Payload Data continued ...                |
//+---------------------------------------------------------------+


#define MAX_HTTP_HEADER_SIZE    32
#define ZLIB_CACHE_EXTEND_SIZE  262144
#define FRAGMENT_EXTEND_SIZE    4096
#define MAX_HTTP_RESPONSE_SIZE  1024
#define MAX_HTTP_REQUEST_SIZE   1024
#define CLIENT_MASK_CACHE_SIZE  8192

typedef enum e_ws_sock_state
{
    ws_client_tcp = 1,
    ws_client_http,
    ws_client_websocket,

    ws_server_tcp,
    ws_server_http,
    ws_server_websocket,
}ws_sock_state;

typedef enum e_extension_options
{
    ws_ext_null = 0,
    ws_ext_defalte = 1,
    ws_ext_server_no_context_takeover = 2,
    ws_ext_client_no_context_takeover = 4,
    ws_ext_sliding_deflate_window = 16
}extension_options;

typedef struct st_ws_manager
{
    HTCPMANAGER         net_mgr;

    pfn_on_open         func_on_open;
    pfn_on_close        func_on_close;
    pfn_on_fail         func_on_fail;
    pfn_on_message      func_on_message;
    pfn_on_http_upgrade func_on_http_upgrade;

    HMEMORYUNIT         ws_socket_unit;
    HMEMORYMANAGER      ws_fragment_mem_mgr;

    z_stream            inflation_stream;
    z_stream            deflation_stream;

    char*               deflate_cache;
    unsigned int        deflate_cache_size;

    char*               inflate_cache;
    unsigned int        inflate_cache_size;

    char*               mask_buffer;
    unsigned int        mask_buffer_size;
}ws_manager;

typedef struct st_ws_listener
{
    HLISTENER   listener;
    ws_manager* mgr;
    void*       user_data;
}ws_listener;

typedef struct st_ws_socket
{
    HSESSION            session;
    ws_listener*        listener;
    ws_manager*         mgr;
    ws_sock_state       state;
    unsigned int        extension_options;
    ws_error            error_type;
    int                 error_code;
    char*               fragment_data;
    unsigned int        fragment_size;
    ws_op_code          fragment_op_code;
    void*               user_data;
}ws_socket;

unsigned int _ws_deflate(ws_manager* mgr, const char* data, unsigned int length)
{
    int err;
    unsigned int deflate_length = 0;

    z_stream* dfs = &mgr->deflation_stream;

    dfs->next_in = (Bytef*)data;
    dfs->avail_in = length;


    unsigned int last_deflate_cache_size = 0;
    for (;;)
    {
        last_deflate_cache_size = mgr->deflate_cache_size - deflate_length;
        dfs->next_out = (Bytef*)(mgr->deflate_cache + deflate_length);
        dfs->avail_out = last_deflate_cache_size;

        err = deflate(dfs, Z_SYNC_FLUSH);

        if (Z_OK == err && dfs->avail_out == 0)
        {
            deflate_length += last_deflate_cache_size - dfs->avail_out;
            mgr->deflate_cache_size += ZLIB_CACHE_EXTEND_SIZE;
            //mgr->deflate_cache = libsvr_memory_manager_realloc(mgr->deflate_cache, mgr->deflate_cache_size);
            mgr->deflate_cache = realloc(mgr->deflate_cache, mgr->deflate_cache_size);
        }
        else
        {
            break;
        }

    }

    deflate_length += last_deflate_cache_size - dfs->avail_out - 4;

    deflateReset(dfs);

    return deflate_length;
}

unsigned int _ws_inflate(ws_manager* mgr, const char* data, unsigned int length)
{
    int err;
    unsigned int inflate_length = 0;

    z_stream* ifs = &mgr->inflation_stream;

    ifs->next_in = (Bytef*)data;
    ifs->avail_in = length;

    unsigned int last_inflate_cache_size = 0;
    for (;;)
    {
        last_inflate_cache_size = mgr->inflate_cache_size - inflate_length;
        ifs->next_out = (Bytef *)(mgr->inflate_cache + inflate_length);
        ifs->avail_out = last_inflate_cache_size;

        err = inflate(ifs, Z_FINISH);

        if (!ifs->avail_in)
        {
            break;
        }

        inflate_length += last_inflate_cache_size - ifs->avail_out;
        mgr->inflate_cache_size += ZLIB_CACHE_EXTEND_SIZE;
        //mgr->inflate_cache = libsvr_memory_manager_realloc(mgr->inflate_cache, mgr->inflate_cache_size);
        mgr->inflate_cache = realloc(mgr->inflate_cache, mgr->inflate_cache_size);

        if (err != Z_BUF_ERROR)
        {
            break;
        }
    }

    inflateReset(ifs);

    if (err != Z_BUF_ERROR && err != Z_OK)
    {
        return 0;
    }

    inflate_length += last_inflate_cache_size - ifs->avail_out;

    return inflate_length;
}

static inline void _unmask_data_overflow(char* dst, const char* src, char* mask, unsigned int length)
{
    for (unsigned int n = (length >> 2) + 1; n; n--)
    {
        *(dst++) = *(src++) ^ mask[0];
        *(dst++) = *(src++) ^ mask[1];
        *(dst++) = *(src++) ^ mask[2];
        *(dst++) = *(src++) ^ mask[3];
    }
}

static inline void _unmask_data_copy_mask(char *dst, char *src, char *mask_ptr, unsigned int length)
{
    char mask[4];
    mask[0] = mask_ptr[0];
    mask[1] = mask_ptr[1];
    mask[2] = mask_ptr[2];
    mask[3] = mask_ptr[3];

    _unmask_data_overflow(dst, src, mask, length);
}

bool rsv1(const char* data) { return *((const unsigned char*)data) & 64; }
bool rsv2(const char* data) { return *((const unsigned char*)data) & 32; }
bool rsv3(const char* data) { return *((const unsigned char*)data) & 16; }
bool rsv123(const char* data) { return *((const unsigned char*)data) & 112; }
bool rsv23(const char* data) { return *((const unsigned char*)data) & 48; }

bool _ws_server_ping(ws_socket* ws_session, const char* message, unsigned int length)
{
    if (length > 125)
    {
        return false;
    }

    unsigned char web_socket_frame[256];

    web_socket_frame[0] = 1;
    web_socket_frame[0] <<= 7;
    web_socket_frame[0] |= ((unsigned char)op_ping & 0xF);

    web_socket_frame[1] = (unsigned char)length;

    memcpy(web_socket_frame + 2, message, length);

    return net_tcp_send(ws_session->session, web_socket_frame, length + 2);
}

bool _ws_client_ping(ws_socket* ws_session, const char* message, unsigned int length)
{
    if (length > 125)
    {
        return false;
    }

    char web_socket_frame[256];

    web_socket_frame[0] = 1;
    web_socket_frame[0] <<= 7;
    web_socket_frame[0] |= ((unsigned char)op_ping & 0xF);

    web_socket_frame[1] = (unsigned char)length;
    web_socket_frame[1] |= 0x80;
    unsigned int mask = (unsigned int)rand_integer(1, 0xFFFFFFFF);

    memcpy(web_socket_frame + 2, &mask, 4);

    _unmask_data_overflow(web_socket_frame + 6, message, (char*)&mask, length);

    return net_tcp_send(ws_session->session, web_socket_frame, length + 6);
}

bool _ws_server_pong(ws_socket* ws_session, const char* message, unsigned int length)
{
    if (length > 125)
    {
        return false;
    }

    unsigned char web_socket_frame[256];

    web_socket_frame[0] = 1;
    web_socket_frame[0] <<= 7;
    web_socket_frame[0] |= ((unsigned char)op_pong & 0xF);

    web_socket_frame[1] = (unsigned char)length;

    memcpy(web_socket_frame + 2, message, length);

    return net_tcp_send(ws_session->session, web_socket_frame, length + 2);
}

bool _ws_client_pong(ws_socket* ws_session, const char* message, unsigned int length)
{
    if (length > 125)
    {
        return false;
    }

    char web_socket_frame[256];

    web_socket_frame[0] = 1;
    web_socket_frame[0] <<= 7;
    web_socket_frame[0] |= ((unsigned char)op_pong & 0xF);

    web_socket_frame[1] = (unsigned char)length;
    web_socket_frame[1] |= 0x80;
    unsigned int mask = (unsigned int)rand_integer(1, 0xFFFFFFFF);

    memcpy(web_socket_frame + 2, &mask, 4);

    _unmask_data_overflow(web_socket_frame + 6, message, (char*)&mask, length);

    return net_tcp_send(ws_session->session, web_socket_frame, length + 6);
}

bool _ws_server_data(ws_socket* ws_session, ws_op_code code, const char* data, unsigned int length, bool compress)
{
    char ws_proto_header[16];
    unsigned int ws_proto_header_length = 0;

    ws_proto_header[0] = 1;

    ws_proto_header[0] <<= 7;
    ws_proto_header[0] |= ((unsigned char)code & 0xF);

    if (compress)
    {
        if (ws_session->extension_options & ws_ext_defalte)
        {
            ws_proto_header[0] |= 64;

            length = _ws_deflate(ws_session->mgr, data, length);
            data = ws_session->mgr->deflate_cache;
        }
    }

    if (length < 126)
    {
        ws_proto_header[1] = (char)length;
        ws_proto_header_length = 2;
    }
    else if (length < 0xFFFF)
    {
        ws_proto_header[1] = 126;
        *(unsigned short*)(ws_proto_header + 2) = htons((unsigned short)length);
        ws_proto_header_length = 4;
    }
    else
    {
        ws_proto_header[1] = 127;
        *(unsigned long long*)(ws_proto_header + 2) = htonll(length);
        ws_proto_header_length = 10;
    }

    if (net_tcp_send(ws_session->session, ws_proto_header, ws_proto_header_length))
    {
        return net_tcp_send(ws_session->session, data, length);
    }

    return false;
}

bool _ws_client_data(ws_socket* ws_session, ws_op_code code, const char* data, unsigned int length, bool compress)
{
    char ws_proto_header[16];
    unsigned int ws_proto_header_length = 0;

    ws_proto_header[0] = 1;

    ws_proto_header[0] <<= 7;
    ws_proto_header[0] |= ((unsigned char)code & 0xF);

    if (compress)
    {
        if (ws_session->extension_options & ws_ext_defalte)
        {
            ws_proto_header[0] |= 64;

            length = _ws_deflate(ws_session->mgr, data, length);
            data = ws_session->mgr->deflate_cache;
        }
    }

    if (length < 126)
    {
        ws_proto_header[1] = (char)length;
        ws_proto_header_length = 6;
    }
    else if (length < 0xFFFF)
    {
        ws_proto_header[1] = 126;
        *(unsigned short*)(ws_proto_header + 2) = htons((unsigned short)length);
        ws_proto_header_length = 8;
    }
    else
    {
        ws_proto_header[1] = 127;
        *(unsigned long long*)(ws_proto_header + 2) = htonll(length);
        ws_proto_header_length = 14;
    }

    ws_proto_header[1] |= 0x80;
    unsigned int mask = (unsigned int)rand_integer(1, 0xFFFFFFFF);

    memcpy(ws_proto_header + ws_proto_header_length - 4, &mask, 4);

    if (ws_session->mgr->mask_buffer_size < length + 4)
    {
        //ws_session->mgr->mask_buffer = libsvr_memory_manager_realloc(ws_session->mgr->mask_buffer, length + 4);
        ws_session->mgr->mask_buffer = realloc(ws_session->mgr->mask_buffer, length + 4);
    }

    _unmask_data_overflow(ws_session->mgr->mask_buffer, data, (char*)&mask, length);

    if (net_tcp_send(ws_session->session, ws_proto_header, ws_proto_header_length))
    {
        return net_tcp_send(ws_session->session, ws_session->mgr->mask_buffer, length);
    }

    return false;
}

bool _ws_server_close(ws_socket* ws_session, unsigned short status_code, const char* message, unsigned int length)
{
    if (length > 125)
    {
        return false;
    }

    unsigned char web_socket_frame[256];

    web_socket_frame[0] = 1;
    web_socket_frame[0] <<= 7;
    web_socket_frame[0] |= ((unsigned char)op_close & 0xF);

    web_socket_frame[1] = (unsigned char)length + 2;
    *(unsigned short*)(web_socket_frame + 2) = htons(status_code);

    memcpy(web_socket_frame + 4, message, length);

    return net_tcp_send(ws_session->session, web_socket_frame, length + 4);
}

bool _ws_client_close(ws_socket* ws_session, unsigned short status_code, const char* message, unsigned int length)
{
    if (length > 125)
    {
        return false;
    }

    char web_socket_frame[256];

    web_socket_frame[0] = 1;
    web_socket_frame[0] <<= 7;
    web_socket_frame[0] |= ((unsigned char)op_close & 0xF);

    web_socket_frame[1] = (unsigned char)length + 2;
    web_socket_frame[1] |= 0x80;
    unsigned int mask = (unsigned int)rand_integer(1, 0xFFFFFFFF);

    memcpy(web_socket_frame + 2, &mask, 4);

    status_code = htons(status_code);

    _unmask_data_overflow(web_socket_frame + 6, (const char*)&status_code, (char*)&mask, 2);
    _unmask_data_overflow(web_socket_frame + 8, message, (char*)&mask, length);

    return net_tcp_send(ws_session->session, web_socket_frame, length + 8);
}

void _ws_close_socket(ws_socket* ws_session, ws_error type, int code, const char* reason, unsigned int length)
{
    if (ws_session->error_type == ws_error_ok)
    {
        ws_session->error_type = type;
        ws_session->error_code = code;

        switch (ws_session->state)
        {
        case ws_client_tcp:
            return;
            break;
        case ws_client_http:
            break;
        case ws_client_websocket:
            _ws_client_close(ws_session, (unsigned short)code, reason, length);
            break;
        case ws_server_tcp:
            break;
        case ws_server_http:
            break;
        case ws_server_websocket:
            _ws_server_close(ws_session, (unsigned short)code, reason, length);
            break;
        }
    }

    net_tcp_close_session(ws_session->session);
}

unsigned int _parser_http_packet(ws_socket* ws_session, const char* data, const unsigned int len)
{
#ifdef _MSC_VER
	UNREFERENCED_PARAMETER(ws_session);
#endif
    if (len <= 4)
    {
        return 0;
    }

    if (!memcmp(&data[len - 4], "\r\n\r\n", 4))
        return len;

    return 0;
}

unsigned int _parser_web_socket_packet(ws_socket* ws_session, const char* data, const unsigned int len)
{
#ifdef _MSC_VER
	UNREFERENCED_PARAMETER(ws_session);
#endif
    if (len < 2)
    {
        return 0;
    }

    unsigned int payload_length = (unsigned char)data[1] & 0x7f;
    unsigned int packet_length = 2;

    if ((unsigned char)data[1] >> 7)
    {
        packet_length += 4;
    }

    if (payload_length < 126)
    {
    }
    else if (payload_length == 126)
    {
        if (len < 4)
        {
            return 0;
        }

        packet_length += 2;

        payload_length = ntohs(*(unsigned short*)(data + 2));
    }
    else if (payload_length == 127)
    {
        if (len < 10)
        {
            return 0;
        }

        packet_length += 8;

        payload_length = (unsigned int)ntohll(*(unsigned long long*)(data + 2));
    }
    else
    {
        return (unsigned int)-1;
    }

    packet_length += payload_length;

    if (packet_length > len)
    {
        return 0;
    }

    return packet_length;
}

unsigned int _ws_parser_packet(HSESSION session, const char* data, const unsigned int len)
{
    ws_socket* ws_session = (ws_socket*)net_tcp_get_session_data(session);

    switch (ws_session->state)
    {
    case ws_client_http:
    case ws_server_http:
        return _parser_http_packet(ws_session, data, len);
        break;
    case ws_client_websocket:
    case ws_server_websocket:
        return _parser_web_socket_packet(ws_session, data, len);
        break;
    default:
        return (unsigned int)-1;
    }
}

typedef struct st_http_header
{
    const char* key;
    const char* value;
    unsigned int key_length;
    unsigned int value_length;
}http_header;

char* _parser_http_headers(char* data, unsigned int len, http_header* headers, unsigned int header_size)
{
    char* end = data + len;
    for (unsigned int i = 0; i < header_size; i++)
    {
        for (headers->key = data;
            (*data != ':') & (*data > 32);
            *(data++) |= 32);
        if (*data == '\r') {
            if ((data != end) & (data[1] == '\n') & (i > 0)) {
                headers->key = 0;
                return data + 2;
            }
            else {
                return 0;
            }
        }
        else {
            headers->key_length = (unsigned int)(data - headers->key);
            for (data++; (*data == ':' || *data < 33) && *data != '\r'; data++);
            headers->value = data;
            data = (char *)memchr(data, '\r', end - data);
            if (data /*!= end*/ && data[1] == '\n') {
                headers->value_length = (unsigned int)(data - headers->value);
                data += 2;
                headers++;
            }
            else {
                return 0;
            }
        }
    }
    return 0;
}

mem_seg _response_extensions_deflate(ws_socket* ws_session, http_header* header_extensions)
{
    mem_seg seg;
    seg.mem = 0;
    seg.mem_size = 0;

    if (header_extensions)
    {
        if (memmem_s(header_extensions->value, header_extensions->value_length, "permessage-deflate", strlen("permessage-deflate")))
        {
            ws_session->extension_options |= ws_ext_defalte;

            seg.mem = "permessage-deflate; client_no_context_takeover";
            seg.mem_size = strlen(seg.mem);
        }
    }

    return seg;
}

void _on_client_http_data(ws_socket* ws_session, const char* data, const unsigned int len)
{
    bool check_head_upgrade = false;
    http_header headers[MAX_HTTP_HEADER_SIZE];

    if (!_parser_http_headers((char*)data, len, headers, MAX_HTTP_HEADER_SIZE))
    {
        _ws_close_socket(ws_session, ws_error_http, 500, "", 0);
        return;
    }

    for (http_header* h = headers; h->key; ++h)
    {
        if (memmem_s(h->key, h->key_length, "upgrade", strlen("upgrade")))
        {
            if (memmem_s(h->value, h->value_length, "websocket", strlen("websocket")))
            {
                check_head_upgrade = true;
            }
        }

        if (memmem_s(h->key, h->key_length, "sec-websocket-extensions", strlen("sec-websocket-extensions")))
        {
            _response_extensions_deflate(ws_session, h);
        }
    }

    if (!check_head_upgrade)
    {
        _ws_close_socket(ws_session, ws_error_http, 406, "", 0);
        return;
    }

    ws_session->state = ws_client_websocket;
    ws_session->mgr->func_on_open(ws_session->listener, ws_session, "", 0);
}

void _on_server_http_data(ws_socket* ws_session, const char* data, const unsigned int len)
{
    bool check_head_upgrade = false;
    bool check_head_key = false;
    bool check_head_extensions = false;

    http_header* head_key = 0;
    http_header* head_extensions = 0;
    

    http_header headers[MAX_HTTP_HEADER_SIZE];
    
    char upgrade_response[MAX_HTTP_RESPONSE_SIZE];
    unsigned int upgrade_response_length = 0;

    const char* req_line_begin = data;
    const char* req_line_end = memmem_s(data, len, "\r\n", 2);

    mem_seg req_line_seg[3];

    if (split_mem_to_segments(req_line_begin, req_line_end - req_line_begin, " ", 1, req_line_seg, 3) != 3)
    {
        _ws_close_socket(ws_session, ws_error_http, 400, "", 0);
        return;
    }

    if (ws_session->mgr->func_on_http_upgrade)
    {
        upgrade_response_length = ws_session->mgr->func_on_http_upgrade(ws_session->listener, data, len, upgrade_response, MAX_HTTP_RESPONSE_SIZE);
    }
    else
    {
        if (!_parser_http_headers((char*)data, len, headers, MAX_HTTP_HEADER_SIZE))
        {
            _ws_close_socket(ws_session, ws_error_http, 400, "", 0);
            return;
        }

        for (http_header* h = headers; h->key; ++h)
        {
            if (!check_head_upgrade)
            {
                if (memmem_s(h->key, h->key_length, "upgrade", strlen("upgrade")))
                {
                    if (memmem_s(h->value, h->value_length, "websocket", strlen("websocket")))
                    {
                        check_head_upgrade = true;
                        continue;
                    }
                }
            }

            if (!check_head_key)
            {
                if (memmem_s(h->key, h->key_length, "sec-websocket-key", strlen("sec-websocket-key")))
                {
                    head_key = h;

                    check_head_key = true;
                    continue;
                }
            }

            if (!check_head_extensions)
            {
                if (memmem_s(h->key, h->key_length, "sec-websocket-extensions", strlen("sec-websocket-extensions")))
                {
                    head_extensions = h;

                    check_head_extensions = true;
                    continue;
                }
            }
        }

        if (!check_head_upgrade || !check_head_key)
        {
            _ws_close_socket(ws_session, ws_error_http, 406, "", 0);
            return;
        }

        unsigned char sha_input[] = "XXXXXXXXXXXXXXXXXXXXXXXX258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        if (head_key->value_length != 24)
        {
            _ws_close_socket(ws_session, ws_error_http, 400, "", 0);
            return;
        }
        memcpy(sha_input, head_key->value, 24);
        unsigned char sha_digest[SHA_DIGEST_LENGTH];
        SHA1(sha_input, sizeof(sha_input) - 1, sha_digest);

        upgrade_response_length = 127;
        memcpy(upgrade_response, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ", 97);
        if (base64_encode((char*)sha_digest, SHA_DIGEST_LENGTH, upgrade_response + 97, 1024 - 97) != 28)
        {
            _ws_close_socket(ws_session, ws_error_http, 400, "", 0);
            return;
        }
        memcpy(upgrade_response + 125, "\r\n", 2);

        mem_seg extensions_deflate = _response_extensions_deflate(ws_session, head_extensions);

        if (extensions_deflate.mem_size)
        {
            memcpy(upgrade_response + upgrade_response_length, "Sec-WebSocket-Extensions: ", 26);
            memcpy(upgrade_response + upgrade_response_length + 26, extensions_deflate.mem, extensions_deflate.mem_size);
            memcpy(upgrade_response + upgrade_response_length + 26 + extensions_deflate.mem_size, "\r\n", 2);
            upgrade_response_length += 26 + (unsigned int)extensions_deflate.mem_size + 2;
        }


        static char stamp[] = "Sec-WebSocket-Version: 13\r\nWebSocket-Server: uWebSockets\r\n\r\n";
        memcpy(upgrade_response + upgrade_response_length, stamp, sizeof(stamp) - 1);
        upgrade_response_length += sizeof(stamp) - 1;
    }

    if (upgrade_response_length)
    {
        net_tcp_send(ws_session->session, upgrade_response, upgrade_response_length);
    }
    else
    {
        _ws_close_socket(ws_session, ws_error_http, 406, "", 0);
        return;
    }

    ws_session->state = ws_server_websocket;
    *(char*)((char*)req_line_seg[1].mem + req_line_seg[1].mem_size) = 0;
    ws_session->mgr->func_on_open(ws_session->listener, ws_session, (const char*)req_line_seg[1].mem, (unsigned int)req_line_seg[1].mem_size);
}

void _on_server_web_socket_data(ws_socket* ws_session, const char* data, const unsigned int len)
{
    unsigned char fin = (unsigned char)data[0] >> 7;
    unsigned char op_code = (unsigned char)data[0] & 0x0f;
    unsigned char is_mask = (unsigned char)data[1] >> 7;

    unsigned char pay_load = (unsigned char)data[1] & 0x7f;

    unsigned char message_header_length = 0;

    bool is_compress = false;

    if (!is_mask)
    {
        _ws_close_socket(ws_session, ws_error_websocket, 1002, "mask no set", (unsigned int)strlen("mask no set"));
        return;
    }

    if (rsv123(data))
    {
        if (ws_session->extension_options & ws_ext_defalte)
        {
            if (rsv23(data))
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "rsv set", (unsigned int)strlen("rsv set"));
                return;
            }
        }
        else
        {
            _ws_close_socket(ws_session, ws_error_websocket, 1002, "rsv set", (unsigned int)strlen("rsv set"));
            return;
        }

        is_compress = true;
    }


    if (pay_load < 126)
    {
        message_header_length = 6;
    }
    else if (pay_load == 126)
    {
        message_header_length = 8;
    }
    else if (pay_load == 127)
    {
        message_header_length = 14;
    }

    _unmask_data_copy_mask(
        (char*)data + message_header_length - 4,
        (char*)data + message_header_length,
        (char*)data + message_header_length - 4,
        len - message_header_length);

    const char* pay_load_data = data + message_header_length - 4;
    unsigned int pay_load_length = len - message_header_length;

    if (fin)
    {
        switch (op_code)
        {
        case 0x0://end frame 
        {
            if (!ws_session->fragment_data)
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "end fragment no begin", (unsigned int)strlen("middle fragment no begin"));
                return;
            }

            if (is_compress)
            {
                pay_load_length = _ws_inflate(ws_session->mgr, pay_load_data, pay_load_length);
                if (!pay_load_length)
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1002, "inflate fail", (unsigned int)strlen("inflate fail"));
                    return;
                }

                pay_load_data = ws_session->mgr->inflate_cache;
            }

            ws_session->fragment_data = libsvr_memory_manager_realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            //ws_session->fragment_data = realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            memcpy(ws_session->fragment_data + ws_session->fragment_size, pay_load_data, pay_load_length);
            ws_session->fragment_size += pay_load_length;

            if (ws_session->fragment_op_code == op_text)
            {
                if (is_valid_utf8((unsigned char*)ws_session->fragment_data, ws_session->fragment_size))
                {
                    ws_session->mgr->func_on_message(ws_session, ws_session->fragment_op_code, ws_session->fragment_data, ws_session->fragment_size);
                }
                else
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1007, "text not utf8", (unsigned int)strlen("text not utf8"));
                }
            }
            else
            {
                ws_session->mgr->func_on_message(ws_session, ws_session->fragment_op_code, ws_session->fragment_data, ws_session->fragment_size);
            }

            libsvr_memory_manager_free(ws_session->fragment_data);
            //free(ws_session->fragment_data);
            ws_session->fragment_data = 0;
            ws_session->fragment_size = 0;
        }
        break;
        case 0x1://data frame
        case 0x2:
        {
            if (ws_session->fragment_data)
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "recv data in fragment", (unsigned int)strlen("recv data in fragment"));
                return;
            }

            if (is_compress)
            {
                pay_load_length = _ws_inflate(ws_session->mgr, pay_load_data, pay_load_length);
                if (!pay_load_length)
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1002, "inflate fail", (unsigned int)strlen("inflate fail"));
                    return;
                }
                pay_load_data = ws_session->mgr->inflate_cache;
            }

            if (op_code == op_text)
            {
                if (!is_valid_utf8((const unsigned char*)pay_load_data, pay_load_length))
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1007, "text not utf8", (unsigned int)strlen("text not utf8"));
                    return;
                }
            }

            ws_session->mgr->func_on_message(ws_session, (ws_op_code)op_code, pay_load_data, pay_load_length);
        }
        break;
        case 0x3://reserve for no control frame
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
            break;
        case 0x8://close frame
        {
            unsigned short status_code = 1005;
            const char* message = 0;
            unsigned int length = 0;

            if (pay_load_length >= 2)
            {
                status_code = ntohs(*(unsigned short*)pay_load_data);

                if (pay_load_length > 2)
                {
                    if (is_valid_utf8((unsigned char*)pay_load_data + 2, pay_load_length - 2))
                    {
                        message = pay_load_data + 2;
                        length = pay_load_length - 2;
                    }
                }
            }

            _ws_close_socket(ws_session, ws_error_websocket, status_code, message, length);
        }
        break;
        case 0x9://ping frame
        {
            _ws_server_pong(ws_session, pay_load_data, pay_load_length);
            ws_session->mgr->func_on_message(ws_session, (ws_op_code)op_code, pay_load_data, pay_load_length);
        }
        break;
        case 0xA://pong frame
        {
            ws_session->mgr->func_on_message(ws_session, (ws_op_code)op_code, pay_load_data, pay_load_length);
        }
        break;
        case 0xB://reserve for control frame
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
            break;
        default:
            return;
        }
    }
    else
    {
        switch (op_code)
        {
        case 0x0://middle frame 
        {
            if (!ws_session->fragment_data)
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "middle fragment no begin", (unsigned int)strlen("middle fragment no begin"));
                return;
            }

            if (is_compress)
            {
                pay_load_length = _ws_inflate(ws_session->mgr, pay_load_data, pay_load_length);
                if (!pay_load_length)
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1002, "inflate fail", (unsigned int)strlen("inflate fail"));
                    return;
                }
                pay_load_data = ws_session->mgr->inflate_cache;
            }

            ws_session->fragment_data = libsvr_memory_manager_realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            //ws_session->fragment_data = realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            memcpy(ws_session->fragment_data + ws_session->fragment_size, pay_load_data, pay_load_length);
            ws_session->fragment_size += pay_load_length;
        }
        break;
        case 0x1://begin frame
        case 0x2:
        {
            if (ws_session->fragment_data)
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "fragment begin in fragment", (unsigned int)strlen("fragment begin in fragment"));
                return;
            }

            ws_session->fragment_op_code = (ws_op_code)op_code;

            if (is_compress)
            {
                pay_load_length = _ws_inflate(ws_session->mgr, pay_load_data, pay_load_length);
                if (!pay_load_length)
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1002, "inflate fail", (unsigned int)strlen("inflate fail"));
                    return;
                }

                pay_load_data = ws_session->mgr->inflate_cache;
            }

            ws_session->fragment_data = libsvr_memory_manager_realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            //ws_session->fragment_data = realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            memcpy(ws_session->fragment_data + ws_session->fragment_size, pay_load_data, pay_load_length);
            ws_session->fragment_size += pay_load_length;
        }
        break;
        default:
            return;
        }
    }
}

void _on_client_web_socket_data(ws_socket* ws_session, const char* data, const unsigned int len)
{
    unsigned char fin = (unsigned char)data[0] >> 7;
    unsigned char op_code = (unsigned char)data[0] & 0x0f;
    unsigned char is_mask = (unsigned char)data[1] >> 7;

    unsigned char pay_load = (unsigned char)data[1] & 0x7f;

    unsigned char message_header_length = 0;

    bool is_compress = false;

    if (is_mask)
    {
        _ws_close_socket(ws_session, ws_error_websocket, 1002, "mask set", (unsigned int)strlen("mask no set"));
        return;
    }

    if (rsv123(data))
    {
        if (ws_session->extension_options & ws_ext_defalte)
        {
            if (rsv23(data))
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "rsv set", (unsigned int)strlen("rsv set"));
                return;
            }
        }
        else
        {
            _ws_close_socket(ws_session, ws_error_websocket, 1002, "rsv set", (unsigned int)strlen("rsv set"));
            return;
        }

        is_compress = true;
    }


    if (pay_load < 126)
    {
        message_header_length = 2;
    }
    else if (pay_load == 126)
    {
        message_header_length = 4;
    }
    else if (pay_load == 127)
    {
        message_header_length = 10;
    }

    const char* pay_load_data = data + message_header_length;
    unsigned int pay_load_length = len - message_header_length;

    if (fin)
    {
        switch (op_code)
        {
        case 0x0://end frame 
        {
            if (!ws_session->fragment_data)
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "end fragment no begin", (unsigned int)strlen("middle fragment no begin"));
                return;
            }

            if (is_compress)
            {
                pay_load_length = _ws_inflate(ws_session->mgr, pay_load_data, pay_load_length);
                if (!pay_load_length)
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1002, "inflate fail", (unsigned int)strlen("inflate fail"));
                    return;
                }

                pay_load_data = ws_session->mgr->inflate_cache;
            }

            ws_session->fragment_data = libsvr_memory_manager_realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            //ws_session->fragment_data = realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            memcpy(ws_session->fragment_data + ws_session->fragment_size, pay_load_data, pay_load_length);
            ws_session->fragment_size += pay_load_length;

            if (ws_session->fragment_op_code == op_text)
            {
                if (is_valid_utf8((unsigned char*)ws_session->fragment_data, ws_session->fragment_size))
                {
                    ws_session->mgr->func_on_message(ws_session, ws_session->fragment_op_code, ws_session->fragment_data, ws_session->fragment_size);
                }
                else
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1007, "text not utf8", (unsigned int)strlen("text not utf8"));
                }
            }
            else
            {
                ws_session->mgr->func_on_message(ws_session, ws_session->fragment_op_code, ws_session->fragment_data, ws_session->fragment_size);
            }

            libsvr_memory_manager_free(ws_session->fragment_data);
            //free(ws_session->fragment_data);
            ws_session->fragment_data = 0;
            ws_session->fragment_size = 0;
        }
        break;
        case 0x1://data frame
        case 0x2:
        {
            if (ws_session->fragment_data)
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "recv data in fragment", (unsigned int)strlen("recv data in fragment"));
                return;
            }

            if (is_compress)
            {
                pay_load_length = _ws_inflate(ws_session->mgr, pay_load_data, pay_load_length);
                if (!pay_load_length)
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1002, "inflate fail", (unsigned int)strlen("inflate fail"));
                    return;
                }
                pay_load_data = ws_session->mgr->inflate_cache;
            }

            if (op_code == op_text)
            {
                if (!is_valid_utf8((const unsigned char*)pay_load_data, pay_load_length))
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1007, "text not utf8", (unsigned int)strlen("text not utf8"));
                    return;
                }
            }

            ws_session->mgr->func_on_message(ws_session, (ws_op_code)op_code, pay_load_data, pay_load_length);
        }
        break;
        case 0x3://reserve for no control frame
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
            break;
        case 0x8://close frame
        {
            unsigned short status_code = 1005;
            const char* message = 0;
            unsigned int length = 0;

            if (pay_load_length >= 2)
            {
                status_code = ntohs(*(unsigned short*)pay_load_data);

                if (pay_load_length > 2)
                {
                    if (is_valid_utf8((unsigned char*)pay_load_data + 2, pay_load_length - 2))
                    {
                        message = pay_load_data + 2;
                        length = pay_load_length - 2;
                    }
                }
            }

            _ws_close_socket(ws_session, ws_error_websocket, status_code, message, length);
        }
        break;
        case 0x9://ping frame
        {
            _ws_client_pong(ws_session, pay_load_data, pay_load_length);
            ws_session->mgr->func_on_message(ws_session, (ws_op_code)op_code, pay_load_data, pay_load_length);
        }
        break;
        case 0xA://pong frame
        {
            ws_session->mgr->func_on_message(ws_session, (ws_op_code)op_code, pay_load_data, pay_load_length);
        }
        break;
        case 0xB://reserve for control frame
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
            break;
        default:
            return;
        }
    }
    else
    {
        switch (op_code)
        {
        case 0x0://middle frame 
        {
            if (!ws_session->fragment_data)
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "middle fragment no begin", (unsigned int)strlen("middle fragment no begin"));
                return;
            }

            if (is_compress)
            {
                pay_load_length = _ws_inflate(ws_session->mgr, pay_load_data, pay_load_length);
                if (!pay_load_length)
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1002, "inflate fail", (unsigned int)strlen("inflate fail"));
                    return;
                }
                pay_load_data = ws_session->mgr->inflate_cache;
            }

            ws_session->fragment_data = libsvr_memory_manager_realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            //ws_session->fragment_data = realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            memcpy(ws_session->fragment_data + ws_session->fragment_size, pay_load_data, pay_load_length);
            ws_session->fragment_size += pay_load_length;
        }
        break;
        case 0x1://begin frame
        case 0x2:
        {
            if (ws_session->fragment_data)
            {
                _ws_close_socket(ws_session, ws_error_websocket, 1002, "fragment begin in fragment", (unsigned int)strlen("fragment begin in fragment"));
                return;
            }

            ws_session->fragment_op_code = (ws_op_code)op_code;

            if (is_compress)
            {
                pay_load_length = _ws_inflate(ws_session->mgr, pay_load_data, pay_load_length);
                if (!pay_load_length)
                {
                    _ws_close_socket(ws_session, ws_error_websocket, 1002, "inflate fail", (unsigned int)strlen("inflate fail"));
                    return;
                }

                pay_load_data = ws_session->mgr->inflate_cache;
            }

            ws_session->fragment_data = libsvr_memory_manager_realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            //ws_session->fragment_data = realloc(ws_session->fragment_data, ws_session->fragment_size + pay_load_length);
            memcpy(ws_session->fragment_data + ws_session->fragment_size, pay_load_data, pay_load_length);
            ws_session->fragment_size += pay_load_length;
        }
        break;
        default:
            return;
        }
    }
}

ws_socket* _ws_alloc_socket(ws_manager* mgr)
{
    return (ws_socket*)memory_unit_alloc(mgr->ws_socket_unit);
}

void _ws_free_socket(ws_manager* mgr, ws_socket* sock)
{
    if (sock->fragment_data)
    {
        libsvr_memory_manager_free(sock->fragment_data);
        //free(sock->fragment_data);
        sock->fragment_data = 0;
    }
    memory_unit_free(mgr->ws_socket_unit, sock);
}


void _ws_on_establish(HLISTENER net_handle, HSESSION session)
{
    ws_socket* ws_session = 0;
    if (net_handle)
    {
        ws_listener* listener = (ws_listener*)net_tcp_get_listener_data(net_handle);
        ws_session = _ws_alloc_socket(listener->mgr);

        ws_session->session = session;
        ws_session->listener = listener;
        ws_session->state = ws_server_http;
        ws_session->mgr = listener->mgr;
        ws_session->error_type = ws_error_ok;
        ws_session->error_code = 0;
        ws_session->extension_options = 0;
        ws_session->extension_options |= ws_ext_server_no_context_takeover | ws_ext_client_no_context_takeover;
        ws_session->fragment_data = 0;
        ws_session->fragment_size = 0;
        net_tcp_set_session_data(session, ws_session);
    }
    else
    {
        ws_session = (ws_socket*)net_tcp_get_session_data(session);
        ws_session->state = ws_client_http;
        net_tcp_send(ws_session->session, ws_session->fragment_data, ws_session->fragment_size);
        libsvr_memory_manager_free(ws_session->fragment_data);
        //free(ws_session->fragment_data);
        ws_session->fragment_data = 0;
        ws_session->fragment_size = 0;
    }
}

void _ws_on_terminate(HSESSION session)
{
    ws_socket* ws_session = (ws_socket*)net_tcp_get_session_data(session);

    if (ws_session->state == ws_server_websocket ||
        ws_session->state == ws_client_websocket)
    {
        if (!ws_session->error_code)
        {
            ws_session->error_code = 1006;
        }
        ws_session->mgr->func_on_close(ws_session, (unsigned short)ws_session->error_code);
    }
    else if (ws_session->state == ws_client_http)
    {
        ws_session->mgr->func_on_fail(ws_session, ws_session->error_type, ws_session->error_code);
    }

    _ws_free_socket(ws_session->mgr, ws_session);
}

void _ws_on_error(HSESSION session, net_tcp_error module_error, int system_error)
{
    ws_socket* ws_session = (ws_socket*)net_tcp_get_session_data(session);

    switch (module_error)
    {
    case error_connect_fail:
    {
        _ws_close_socket(ws_session, ws_error_tcp, system_error, "", 0);
        ws_session->mgr->func_on_fail(ws_session, ws_session->error_type, ws_session->error_code);
        _ws_free_socket(ws_session->mgr, ws_session);
    }
    break;
    case error_system:
    {
        char sys_error[64];
		int sys_error_length = snprintf(sys_error, sizeof(sys_error), "svr sys error: %d", system_error);
        _ws_close_socket(ws_session, ws_error_websocket, 1001, sys_error, sys_error_length);
    }
    break;
	case error_ssl:
	{
		char ssl_error[64];
		int ssl_error_length = snprintf(ssl_error, sizeof(ssl_error), "svr ssl error: %d", system_error);
		_ws_close_socket(ws_session, ws_error_websocket, 1001, ssl_error, ssl_error_length);
	}
	break;
    case error_send_overflow:
    {
        _ws_close_socket(ws_session, ws_error_websocket, 1001, "svr send overflow", (unsigned int)strlen("svr send overflow"));
    }
    break;
    case error_recv_overflow:
    {
        _ws_close_socket(ws_session, ws_error_websocket, 1009, "svr recv overflow", (unsigned int)strlen("svr recv overflow"));
    }
    break;
    case error_packet:
    {
        _ws_close_socket(ws_session, ws_error_websocket, 1002, "svr error packet", (unsigned int)strlen("svr error packet"));
    }
    break;
	case error_ok:
    case error_none:
	break;
    }
}

void _ws_on_recv(HSESSION session, const char* data, const unsigned int len)
{
    ws_socket* ws_session = (ws_socket*)net_tcp_get_session_data(session);

    switch (ws_session->state)
    {
    case ws_client_http:
        _on_client_http_data(ws_session, data, len);
        break;
    case ws_client_websocket:
        _on_client_web_socket_data(ws_session, data, len);
        break;
    case ws_server_http:
        _on_server_http_data(ws_session, data, len);
        break;
    case ws_server_websocket:
        _on_server_web_socket_data(ws_session, data, len);
        break;
    default:
        net_tcp_close_session(session);
    }
}

ws_manager* create_net_ws(HTCPMANAGER net_mgr,
    pfn_on_open func_on_open,
    pfn_on_close func_on_close,
    pfn_on_fail func_on_fail,
    pfn_on_message func_on_message,
    pfn_on_http_upgrade func_on_http_upgrade)
{
    ws_manager* mgr = (ws_manager*)malloc(sizeof(ws_manager));
    mgr->net_mgr = net_mgr;
    mgr->func_on_open = func_on_open;
    mgr->func_on_close = func_on_close;
    mgr->func_on_fail = func_on_fail;
    mgr->func_on_message = func_on_message;
    mgr->func_on_http_upgrade = func_on_http_upgrade;

    mgr->ws_socket_unit = create_memory_unit(sizeof(ws_socket));

    memset(&mgr->inflation_stream, 0, sizeof(mgr->inflation_stream));
    memset(&mgr->deflation_stream, 0, sizeof(mgr->deflation_stream));

    inflateInit2(&mgr->inflation_stream, -15);
    deflateInit2(&mgr->deflation_stream, 1, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);

    mgr->deflate_cache_size = ZLIB_CACHE_EXTEND_SIZE;
    //mgr->deflate_cache = libsvr_memory_manager_alloc(mgr->deflate_cache_size);
    mgr->deflate_cache = malloc(mgr->deflate_cache_size);

    mgr->inflate_cache_size = ZLIB_CACHE_EXTEND_SIZE;
    //mgr->inflate_cache = libsvr_memory_manager_alloc(mgr->inflate_cache_size);
    mgr->inflate_cache = malloc(mgr->inflate_cache_size);

    mgr->mask_buffer_size = CLIENT_MASK_CACHE_SIZE;
    //mgr->mask_buffer = libsvr_memory_manager_alloc(mgr->mask_buffer_size);
    mgr->mask_buffer = malloc(mgr->mask_buffer_size);

    return mgr;
}

void destroy_net_ws(ws_manager* ws_mgr)
{
    //libsvr_memory_manager_free(ws_mgr->mask_buffer);
    free(ws_mgr->mask_buffer);
    ws_mgr->mask_buffer_size = 0;

    //libsvr_memory_manager_free(ws_mgr->deflate_cache);
    free(ws_mgr->deflate_cache);
    ws_mgr->deflate_cache_size = 0;

    //libsvr_memory_manager_free(ws_mgr->inflate_cache);
    free(ws_mgr->inflate_cache);
    ws_mgr->inflate_cache_size = 0;

    inflateEnd(&ws_mgr->inflation_stream);
    deflateEnd(&ws_mgr->deflation_stream);

    destroy_memory_unit(ws_mgr->ws_socket_unit);
    free(ws_mgr);
}

ws_listener* net_ws_listen(ws_manager* mgr, const char* ip, unsigned short port,
    unsigned int recv_buf_size, unsigned int send_buf_size, SSL_CTX* svr_ssl_ctx)
{
    ws_listener* listener = (ws_listener*)malloc(sizeof(ws_listener));

    if (svr_ssl_ctx)
    {
        listener->listener = net_ssl_listen(
            mgr->net_mgr,
            ip,
            port,
            recv_buf_size,
            send_buf_size,
            true,
            svr_ssl_ctx,
            _ws_on_establish,
            _ws_on_terminate,
            _ws_on_error,
            _ws_on_recv,
            _ws_parser_packet);
    }
    else
    {
        listener->listener = net_tcp_listen(
            mgr->net_mgr,
            ip,
            port,
            recv_buf_size,
            send_buf_size,
            true,
            _ws_on_establish,
            _ws_on_terminate,
            _ws_on_error,
            _ws_on_recv,
            _ws_parser_packet);
    }

    if (!listener->listener)
    {
        free(listener);
        return 0;
    }
    else
    {
        net_tcp_set_listener_data(listener->listener, listener);
    }


    listener->mgr = mgr;

    return listener;
}

void net_ws_close_listener(ws_listener* ws_listener)
{
    net_tcp_close_listener(ws_listener->listener);
    free(ws_listener);
}

bool _parser_uri(const char* uri, bool* is_secure, mem_seg* host_name_seg, mem_seg* path_seg, int* port)
{
    size_t uri_length = strlen(uri);
    if (uri_length < 5)
    {
        return false;
    }
    const char* uri_end = uri + uri_length;
    const char* uri_parser = uri + 5;
    const char* end_bracket = 0;

    *port = 80;
    *is_secure = false;

    host_name_seg->mem = 0;
    host_name_seg->mem_size = 0;

    path_seg->mem = 0;
    path_seg->mem_size = 0;

    if (!strncmp(uri, "wss://", 6))
    {
        *port = 443;
        *is_secure = true;
        uri_parser = uri + 6;
    }
    else if (strncmp(uri, "ws://", 5))
    {
        return false;
    }

    if (uri_parser >= uri_end)
    {
        return false;
    }

    //host name
    {
        if (*uri_parser == '[')
        {
            ++uri_parser;
            if (uri_parser >= uri_end)
            {
                return false;
            }

            end_bracket = strchr(uri_parser, ']');
            if (!end_bracket)
            {
                return false;
            }
        }
        else
        {
            end_bracket = uri_parser;

            while (end_bracket < uri_end)
            {
                if (*end_bracket == ':' ||
                    *end_bracket == '/' ||
                    *end_bracket == '?' ||
                    *end_bracket == '#')
                {
                    break;
                }
                ++end_bracket;
            }
        }

        host_name_seg->mem = uri_parser;
        host_name_seg->mem_size = end_bracket - uri_parser;
        uri_parser = end_bracket;
    }

    if (uri_parser >= uri_end)
    {
        return true;
    }

    //port
    if (*uri_parser == ':')
    {
        ++uri_parser;
        if (uri_parser >= uri_end)
        {
            return false;
        }

        end_bracket = uri_parser;

        while (end_bracket < uri_end)
        {
            if (*end_bracket == '/' ||
                *end_bracket == '?' ||
                *end_bracket == '#')
            {
                break;
            }
            ++end_bracket;
        }

        if (end_bracket - uri_parser <= 0)
        {
            return false;
        }

        char sz_port[32];
        memcpy(sz_port, uri_parser, end_bracket - uri_parser);
        sz_port[end_bracket - uri_parser] = 0;

        *port = atoi(sz_port);
        uri_parser = end_bracket;
    }

    //path
    if (*uri_parser == '/')
    {
        ++uri_parser;
        if (uri_parser >= uri_end)
        {
            return false;
        }

        end_bracket = uri_parser;

        while (end_bracket < uri_end)
        {
            if (*end_bracket == '?' ||
                *end_bracket == '#')
            {
                break;
            }
            ++end_bracket;
        }

        path_seg->mem = uri_parser;
        path_seg->mem_size = end_bracket - uri_parser;
    }

    return true;
}

#define _append_http_request(data, size) if (copy_length + (size) >= MAX_HTTP_REQUEST_SIZE)\
                                         {\
                                             goto FAIL;\
                                         }\
                                         else\
                                         {\
                                             memcpy(ws_session->fragment_data + copy_length, (data), (size));\
                                             copy_length += (size);\
                                         }

ws_socket* net_ws_connect(ws_manager* ws_mgr, const char* uri, const char* extra_headers, unsigned int recv_buf_size, unsigned int send_buf_size, SSL_CTX* cli_ssl_ctx)
{
    mem_seg host_name_seg;
    mem_seg path_seg;
    char host_name[256];
    int port;
    bool is_secure;

    HSESSION session = 0;

    if (!_parser_uri(uri, &is_secure, &host_name_seg, &path_seg, &port))
    {
        return 0;
    }

    if (sizeof(host_name) <= host_name_seg.mem_size)
    {
        return 0;
    }
    else
    {
        memcpy(host_name, host_name_seg.mem, host_name_seg.mem_size);
        host_name[host_name_seg.mem_size] = 0;
    }

    if (is_secure)
    {
        session = net_ssl_connect(
            ws_mgr->net_mgr, host_name,
            (unsigned short)port,
            recv_buf_size,
            send_buf_size,
            false,
            0,
            0,
            cli_ssl_ctx,
            _ws_on_establish,
            _ws_on_terminate,
            _ws_on_error,
            _ws_on_recv,
            _ws_parser_packet);
    }
    else
    {
        session = net_tcp_connect(
            ws_mgr->net_mgr, host_name,
            (unsigned short)port,
            recv_buf_size,
            send_buf_size,
            false,
            0,
            0,
            _ws_on_establish,
            _ws_on_terminate,
            _ws_on_error,
            _ws_on_recv,
            _ws_parser_packet);
    }

    if (!session)
    {
        return 0;
    }

    ws_socket* ws_session = _ws_alloc_socket(ws_mgr);

    ws_session->session = session;
    ws_session->listener = 0;
    ws_session->state = ws_client_tcp;
    ws_session->mgr = ws_mgr;
    ws_session->error_type = ws_error_ok;
    ws_session->error_code = 0;
    ws_session->extension_options = 0;
    ws_session->extension_options |= ws_ext_server_no_context_takeover | ws_ext_client_no_context_takeover;
    ws_session->fragment_data = libsvr_memory_manager_alloc(MAX_HTTP_REQUEST_SIZE);
    ws_session->fragment_size = MAX_HTTP_REQUEST_SIZE;
    {
        size_t copy_length = 0;
        size_t extra_headers_length = strlen(extra_headers);

        char sz_port[16] = { 0 };
		ulltostr(port, sz_port, sizeof(sz_port), 10);
        size_t port_length = strlen(sz_port);

        const char* tmp_data =
            " HTTP/1.1\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
            "Host: ";

        const char* tmp_data1 =
            "\r\n"
            "Sec-WebSocket-Version: 13\r\n";


        _append_http_request("GET /", 5);
        _append_http_request(path_seg.mem, path_seg.mem_size);
        _append_http_request(tmp_data, 103);
        _append_http_request(host_name_seg.mem, host_name_seg.mem_size);
        _append_http_request(":", 1);
        _append_http_request(sz_port, port_length);
        _append_http_request(tmp_data1, 29);
        _append_http_request(extra_headers, extra_headers_length);
        _append_http_request("\r\n", 2);

        ws_session->fragment_size = (unsigned int)copy_length;
    }

    net_tcp_set_session_data(ws_session->session, ws_session);

    return ws_session;
FAIL:
    _ws_free_socket(ws_mgr, ws_session);
    return 0;
}

bool net_ws_send_text(ws_socket* ws_session, const char* data, unsigned int length, bool compress)
{
    if (ws_session->error_type != ws_error_ok)
    {
        return false;
    }

    if (!is_valid_utf8((unsigned char*)data, length))
    {
        return false;
    }

    switch (ws_session->state)
    {
    case ws_server_websocket:
        return _ws_server_data(ws_session, op_text, data, length, compress);
        break;
    case ws_client_websocket:
        return _ws_client_data(ws_session, op_text, data, length, compress);
        break;
    default:
        return false;
    }
}

bool net_ws_send_binary(ws_socket*  ws_session, const char* data, unsigned int length, bool compress)
{
    if (ws_session->error_type != ws_error_ok)
    {
        return false;
    }

    switch (ws_session->state)
    {
    case ws_server_websocket:
        return _ws_server_data(ws_session, op_binary, data, length, compress);
        break;
    case ws_client_websocket:
        return _ws_client_data(ws_session, op_binary, data, length, compress);
        break;
    default:
        return false;
    }
}

bool net_ws_send_ping(ws_socket*  ws_session, const char* message, unsigned int length)
{
    if (ws_session->error_type != ws_error_ok)
    {
        return false;
    }

    switch (ws_session->state)
    {
    case ws_server_websocket:
        return _ws_server_ping(ws_session, message, length);
        break;
    case ws_client_websocket:
        return _ws_client_ping(ws_session, message, length);
        break;
    default:
        return false;
    }
}

bool net_ws_send_pong(ws_socket*  ws_session, const char* message, unsigned int length)
{
    if (ws_session->error_type != ws_error_ok)
    {
        return false;
    }

    switch (ws_session->state)
    {
    case ws_server_websocket:
        return _ws_server_pong(ws_session, message, length);
        break;
    case ws_client_websocket:
        return _ws_client_pong(ws_session, message, length);
        break;
    default:
        return false;
    }
}

void net_ws_close_session(ws_socket* ws_session)
{
    _ws_close_socket(ws_session, ws_error_websocket, 1000, "", 0);
}

HSESSION net_ws_to_tcp_session(ws_socket* ws_session)
{
    return ws_session->session;
}

HLISTENER net_ws_to_tcp_listener(ws_listener* ws_listener)
{
    return ws_listener->listener;
}

void net_ws_set_session_data(ws_socket* ws_session, void* user_data)
{
    ws_session->user_data = user_data;
}

void net_ws_set_listener_data(ws_listener* ws_listener, void* user_data)
{
    ws_listener->user_data = user_data;
}

void* net_ws_get_session_data(ws_socket* ws_session)
{
    return ws_session->user_data;
}

void* net_ws_get_listener_data(ws_listener* ws_listener)
{
    return ws_listener->user_data;
}
