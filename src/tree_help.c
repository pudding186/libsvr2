#include "./lib_svr_common_def.h"
#include "../include/memory_pool.h"
#include "../include/tree_help.h"
#include "../include/utility.hpp"

rb_tree* create_str_rb_tree(void)
{
    return create_rb_tree(bkdr_str_cmp);
}

void destroy_str_rb_tree(rb_tree* tree)
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

rb_node* str_rb_tree_insert(rb_tree* tree, const char* str, bool copy_str, void* user_data)
{
    bkdr_str* pt = alloc_bkdr_str(str, copy_str);

    rb_node* exist_node;

    if (!rb_tree_try_insert_user(tree, pt, user_data, &exist_node))
    {
        free_bkdr_str(exist_node->key.v_pointer);

        exist_node->key.v_pointer = pt;
        exist_node->value.v_pointer = user_data;
    }

    return exist_node;
}

bool str_rb_tree_try_insert(rb_tree* tree, const char* str, bool copy_str, void* user_data, rb_node** exist_node)
{
    bkdr_str* pt = alloc_bkdr_str(str, copy_str);

    if (!rb_tree_try_insert_user(tree, pt, user_data, exist_node))
    {
        free_bkdr_str(pt);

        return false;
    }

    return true;
}

rb_node* str_rb_tree_find(rb_tree* tree, const char* str)
{
    bkdr_str pt;
    pt.hash_code = BKDRHash(str);
    pt.str = str;

    return rb_tree_find_user(tree, &pt);
}

const char* str_rb_node_key(rb_node* node)
{
    return ((bkdr_str*)rb_node_key_user(node))->str;
}

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
        free_bkdr_wstr(exist_node->key.v_pointer);

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