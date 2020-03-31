#include "./lib_svr_common_def.h"
#include "../include/memory_pool.h"
#include "../include/tree_help.h"
#include "../include/utility.hpp"

HRBTREE create_str_bkdr_tree(void)
{
    return create_rb_tree(bkdr_str_cmp);
}

void destroy_str_bkdr_tree(rb_tree* tree)
{
    rb_node* str_node = rb_first(tree);
    while (str_node)
    {
        bkdr_str* pt = (bkdr_str*)rb_node_key_user(str_node);
        free_bkdr_str(pt);

        str_node = rb_next(str_node);
    }

    destroy_rb_tree(tree);
}

rb_node* str_bkdr_tree_insert(rb_tree* tree, const char* str, bool copy_str, void* user_data)
{
    bkdr_str* pt = alloc_bkdr_str(str, copy_str);

    rb_node* exist_node;

    if (!rb_tree_try_insert_user(tree, pt, user_data, &exist_node))
    {
        free_bkdr_str((bkdr_str*)exist_node->key.v_pointer);

        exist_node->key.v_pointer = pt;
        exist_node->value.v_pointer = user_data;
    }

    return exist_node;
}

bool str_bkdr_tree_try_insert(rb_tree* tree, const char* str, bool copy_str, void* user_data, rb_node** exist_node)
{
    bkdr_str* pt = alloc_bkdr_str(str, copy_str);

    if (!rb_tree_try_insert_user(tree, pt, user_data, exist_node))
    {
        free_bkdr_str(pt);

        return false;
    }

    return true;
}

void* str_bkdr_tree_find(rb_tree* tree, const char* str)
{
    bkdr_str pt;
    pt.hash_code = BKDRHash(str);
    pt.str = str;

    rb_node* node = rb_tree_find_user(tree, &pt);
    if (node)
    {
        return rb_node_value_user(node);
    }

    return 0;
}

const char* str_bkdr_node_key(rb_node* node)
{
    return ((bkdr_str*)rb_node_key_user(node))->str;
}

bool tree_is_str_bkdr(rb_tree* tree)
{
    if (rb_tree_cmp_func(tree) == bkdr_str_cmp)
    {
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
rb_tree* create_int_seg_tree(rb_tree* tree, size_t interval)
{
    rb_tree* new_tree = create_rb_tree(integer_key_segment_cmp);

    integer_key_segment* key_segment = 0;

    size_t value_segment_count = 128;
    void** value_segment = (void**)libsvr_memory_manager_alloc(value_segment_count * sizeof(void*));
    size_t segment_count = 0;

    rb_node* node = rb_first(tree);
    while (node)
    {
        if (key_segment)
        {
            size_t key = rb_node_key_integer(node);

            if (key - key_segment->key_end <= interval)
            {
                key_segment->key_end++;
                while (key_segment->key_end < key)
                {
                    if (segment_count >= value_segment_count)
                    {
                        void** tmp;
                        value_segment_count += 1024;

                        tmp = (void**)libsvr_memory_manager_alloc(value_segment_count * sizeof(void*));
                        memcpy(tmp, value_segment, segment_count * sizeof(void*));
                        libsvr_memory_manager_free(value_segment);
                        value_segment = tmp;
                    }
                    value_segment[segment_count] = 0;
                    segment_count++;
                    key_segment->key_end++;
                }

                if (segment_count >= value_segment_count)
                {
                    void** tmp;
                    value_segment_count += 1024;

                    tmp = (void**)libsvr_memory_manager_alloc(value_segment_count * sizeof(void*));
                    memcpy(tmp, value_segment, segment_count * sizeof(void*));
                    libsvr_memory_manager_free(value_segment);
                    value_segment = tmp;
                }
                value_segment[segment_count] = rb_node_value_user(node);
                segment_count++;
            }
            else
            {
                void** real_value_segment = (void**)libsvr_memory_manager_alloc(segment_count * sizeof(void*));
                memcpy(real_value_segment, value_segment, segment_count * sizeof(void*));
                rb_tree_insert_user(new_tree, key_segment, real_value_segment);

                key_segment = (integer_key_segment*)libsvr_memory_manager_alloc(sizeof(integer_key_segment));
                key_segment->key_begin = key;
                key_segment->key_end = key_segment->key_begin;
                value_segment[0] = rb_node_value_user(node);
                segment_count = 1;
            }
        }
        else
        {
            key_segment = (integer_key_segment*)libsvr_memory_manager_alloc(sizeof(integer_key_segment));
            key_segment->key_begin = rb_node_key_integer(node);
            key_segment->key_end = key_segment->key_begin;
            value_segment[0] = rb_node_value_user(node);
            segment_count = 1;
        }

        node = rb_next(node);
    }

    if (key_segment)
    {
        void** real_value_segment = (void**)libsvr_memory_manager_alloc(segment_count * sizeof(void*));
        memcpy(real_value_segment, value_segment, segment_count * sizeof(void*));
        rb_tree_insert_user(new_tree, key_segment, real_value_segment);
    }

    libsvr_memory_manager_free(value_segment);

    destroy_rb_tree(tree);
    
    return new_tree;
}

bool tree_is_int_seg(rb_tree* tree)
{
    if (rb_tree_cmp_func(tree) == integer_key_segment_cmp)
    {
        return true;
    }

    return false;
}

void* int_seg_tree_find(rb_tree* tree, size_t idx)
{
    rb_node* node;
    integer_key_segment key_segment;
    key_segment.key_begin = idx;
    key_segment.key_end = idx;

    node = rb_tree_find_user(tree, &key_segment);

    if (node)
    {
        integer_key_segment* node_key_segment = (integer_key_segment*)rb_node_key_user(node);
        void** arry = (void**)rb_node_value_user(node);

        return arry[idx - node_key_segment->key_begin];
    }

    return 0;
}

void destroy_int_seg_tree(rb_tree* tree)
{
    rb_node* node = rb_first(tree);

    while (node)
    {
        libsvr_memory_manager_free((void*)rb_node_key_user(node));
        libsvr_memory_manager_free(rb_node_value_user(node));

        node = rb_next(node);
    }

    destroy_rb_tree(tree);
}

void* tree_find_integer(rb_tree* tree, size_t key)
{
    rb_node* node = rb_tree_find_integer(tree, key);
    if (node)
    {
        return rb_node_value_user(node);
    }

    return 0;
}

size_t int_seg_tree_node_value(rb_node* node, void*** value_arry)
{
    integer_key_segment* key = (integer_key_segment*)rb_node_key_user(node);
    *value_arry = (void**)rb_node_value_user(node);

    return (key->key_end - key->key_begin + 1);
}

//////////////////////////////////////////////////////////////////////////

rb_tree* create_wstr_rb_tree(void)
{
    return create_rb_tree(bkdr_wstr_cmp);
}

void destroy_wstr_rb_tree(rb_tree* tree)
{
    rb_node* str_node = rb_first(tree);
    while (str_node)
    {
        bkdr_wstr* pt = (bkdr_wstr*)rb_node_key_user(str_node);
        free_bkdr_wstr(pt);

        str_node = rb_next(str_node);
    }

    destroy_rb_tree(tree);
}

rb_node* wstr_rb_tree_insert(rb_tree* tree, const wchar_t* str, bool copy_str, void* user_data)
{
    bkdr_wstr* pt = alloc_bkdr_wstr(str, copy_str);

    rb_node* exist_node;

    if (!rb_tree_try_insert_user(tree, pt, user_data, &exist_node))
    {
        free_bkdr_wstr((bkdr_wstr*)exist_node->key.v_pointer);

        exist_node->key.v_pointer = pt;
        exist_node->value.v_pointer = user_data;
    }

    return exist_node;
}

bool wstr_rb_tree_try_insert(rb_tree* tree, const wchar_t* str, bool copy_str, void* user_data, rb_node** exist_node)
{
    bkdr_wstr* pt = alloc_bkdr_wstr(str, copy_str);

    if (!rb_tree_try_insert_user(tree, pt, user_data, exist_node))
    {
        free_bkdr_wstr(pt);

        return false;
    }

    return true;
}

rb_node* wstr_rb_tree_find(rb_tree* tree, const wchar_t* str)
{
    bkdr_wstr pt;
    pt.hash_code = BKDRHashW(str);
    pt.str = str;

    return rb_tree_find_user(tree, &pt);
}

const wchar_t* wstr_rb_node_key(rb_node* node)
{
    return ((bkdr_wstr*)rb_node_key_user(node))->str;
}