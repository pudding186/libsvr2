#pragma once
#include "../include/lib_svr_def.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include <stddef.h>
#include <stdlib.h>


#ifdef _MSC_VER
#define TLS_VAR __declspec(thread)
#elif __GNUC__
#define TLS_VAR __thread
#else
#error "unknown compiler"
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#define CRUSH_CODE() do{char* p = 0;*p = 'a';}while(0)
//////////////////////////////////////////////////////////////////////////
typedef struct st_tag_pointer
{
    union
    {
        struct { unsigned int tag[2]; void* ptr; } tp;
        struct { long long Int[2]; } bit128;
    }u_data;
}tag_pointer;

typedef struct st_mem_block
{
    struct st_mem_block*    next; //指向下一个内存块的
}mem_block;

typedef struct st_mem_unit
{
    struct st_tag_pointer   unit_free_head_mt;
    struct st_tag_pointer   unit_free_head_cache;
    void*                   unit_create_thread;
    struct st_mem_block*    block_head;     //内存块链表头
    void*                   unit_free_head; //可分配内存单元链表头
    size_t                  unit_size;      //内存单元的大小
    size_t                  grow_count;     //内存池每次增长个数
    size_t                  alloc_count;
    long long               total_count;
}mem_unit;

typedef struct st_mem_pool
{
    struct st_mem_unit**    units;          //内存池数组
    size_t                  unit_size;      //内存池数组长度
    size_t                  shift;          //位移偏移量
    size_t                  align;          //内存池对齐字节数，必须是4的倍数
    size_t                  grow;           //每次扩展内存大小
    size_t                  min_mem_size;   //内存池管理的最小内存大小，小于此大小按最小分配
    size_t                  max_mem_size;   //内存池管理的最大内存大小，大于此大小的内存由系统托管
    void*                   pool_create_thread;
}mem_pool;

//////////////////////////////////////////////////////////////////////////
union un_value
{
    size_t      v_integer;
    void*       v_pointer;
};

typedef struct st_avl_node
{
    struct st_avl_node* avl_child[2];
    struct st_avl_node* avl_parent;
    int                 avl_height;

    struct st_avl_node* prev;
    struct st_avl_node* next;

    union un_value      key;
    union un_value      value;
}avl_node;

typedef struct st_avl_tree
{
    struct st_avl_node* root;
    size_t              size;

    struct st_avl_node* head;
    struct st_avl_node* tail;

    compare_func        key_cmp;
    mem_unit*           tree_unit;
    mem_unit*           node_unit;
}avl_tree;

//////////////////////////////////////////////////////////////////////////
typedef struct st_rb_node
{
    struct st_rb_node*  rb_right;
    struct st_rb_node*  rb_left;
    struct st_rb_node*  rb_parent;
    int                 rb_color;

    struct st_rb_node*  prev;
    struct st_rb_node*  next;

    union un_value      key;
    union un_value      value;
}rb_node;

typedef struct st_rb_tree
{
    struct st_rb_node*  root;
    size_t              size;

    struct st_rb_node*  head;
    struct st_rb_node*  tail;

    compare_func        key_cmp;
    mem_unit*           tree_unit;
    mem_unit*           node_unit;
}rb_tree;

//////////////////////////////////////////////////////////////////////////
typedef struct st_mem_mgr
{
    avl_tree    mem_pool_map;
}mem_mgr;

extern void* (libsvr_memory_manager_realloc)(void* old_mem, size_t mem_size);

extern void* (libsvr_memory_manager_alloc)(size_t mem_size);

extern void (libsvr_memory_manager_free)(void* mem);

extern bool (libsvr_memory_manager_check)(void* mem);

//////////////////////////////////////////////////////////////////////////
typedef struct st_bkdr_str
{
    unsigned long long  hash_code;
    const char*         str;
}bkdr_str;

typedef struct st_bkdr_wstr
{
    unsigned long long  hash_code;
    const wchar_t*      str;
}bkdr_wstr;


extern bkdr_str* alloc_bkdr_str(const char* str, bool is_copy);
extern void free_bkdr_str(bkdr_str* str);
extern ptrdiff_t bkdr_str_cmp(void* str1, void* str2);

extern bkdr_wstr* alloc_bkdr_wstr(const wchar_t* str, bool is_copy);
extern void free_bkdr_wstr(bkdr_wstr* str);
extern ptrdiff_t bkdr_wstr_cmp(void* str1, void* str2);

typedef struct st_integer_key_group
{
    size_t key_begin;
    size_t key_end;
}integer_key_group;

extern ptrdiff_t integer_key_group_cmp(void* key_group1, void* key_group2);
//////////////////////////////////////////////////////////////////////////
#define TVN_BITS 6
#define TVR_BITS 8
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)

struct list_head {
    struct list_head *next, *prev;
};

typedef struct st_timer_info
{
    struct list_head            entry;
    unsigned                    expires;
    unsigned                    elapse;
    int                         count;
    void*                       data;
    struct st_timer_manager*    manager;
}timer_info;

typedef struct st_timer_manager
{
    struct list_head    tv1[TVR_SIZE];
    struct list_head    tv2[TVN_SIZE];
    struct list_head    tv3[TVN_SIZE];
    struct list_head    tv4[TVN_SIZE];
    struct list_head    tv5[TVN_SIZE];

    unsigned            last_tick;
    pfn_on_timer        func_on_timer;
    HMEMORYUNIT         timer_info_unit;
}timer_manager;

//////////////////////////////////////////////////////////////////////////
typedef struct st_loop_cache
{
    char*   cache_begin;
    char*   cache_end;
    char*   head;
    char*   tail;
    size_t  size;
}loop_cache;

//////////////////////////////////////////////////////////////////////////
typedef struct st_json_node
{
    struct st_json_node*    next;
    struct st_json_node*    stack;

    json_value_type         type;
    char*                   key;
    size_t                  key_length;
    size_t                  str_length;

    union
    {
        char*               str;
        long long           number;
        double              number_ex;
        struct st_json_struct* struct_ptr;
    } value;
}json_node;

typedef struct st_json_struct
{
    enum e_json_value_type  type;
    struct st_json_node*    head;
    struct st_json_node*    tail;
    struct st_json_struct*  stack;
}json_struct;

//////////////////////////////////////////////////////////////////////////

#define BIO_RECV                0
#define BIO_SEND                1

#define DEF_SSL_SEND_CACHE_SIZE 4096
#define DEF_SSL_RECV_CACHE_SIZE 4096

#define SSL_UN_HAND_SHAKE       0
#define SSL_HAND_SHAKE          1

typedef struct st_net_ssl_core
{
    SSL*                ssl;
    BIO*                bio[2];
}net_ssl_core;

//////////////////////////////////////////////////////////////////////////

extern ptrdiff_t trace_info_cmp(void* info1, void* info2);
extern ptrdiff_t trace_ptr_cmp(void* ptr1, void* ptr2);

#ifdef __cplusplus
}

//////////////////////////////////////////////////////////////////////////
//typedef class CFuncPerformanceMgr* HFUNCPERFMGR;

#endif // __cplusplus