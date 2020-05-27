#pragma once
#include "./lib_svr_def.h"
#include "./rb_tree.h"
#include "./avl_tree.h"
#ifdef  __cplusplus
extern "C" {
#endif

    extern HRBTREE (create_str_bkdr_tree)(void);

    extern void (destroy_str_bkdr_tree)(HRBTREE tree);

    extern HRBNODE (str_bkdr_tree_insert)(HRBTREE tree, const char* str, bool copy_str, void* user_data);

    extern bool (str_bkdr_tree_try_insert)(HRBTREE tree, const char* str, bool copy_str, void* user_data, HRBNODE* exist_node);

    extern void* (str_bkdr_tree_find)(HRBTREE tree, const char* str);

    extern void* (str_bkdr_tree_erase)(HRBTREE tree, const char* str);

    extern const char* (str_bkdr_node_key)(HRBNODE node);

    extern bool (tree_is_str_bkdr)(HRBTREE tree);

    //////////////////////////////////////////////////////////////////////////
    extern HRBTREE (create_int_seg_tree)(HRBTREE tree, size_t interval);

    extern bool (tree_is_int_seg)(HRBTREE tree);

    extern void* (int_seg_tree_find)(HRBTREE tree, size_t idx);

    extern void (destroy_int_seg_tree)(HRBTREE tree);

    extern void* (tree_find_integer)(HRBTREE tree, size_t key);

    extern size_t (int_seg_tree_node_value)(HRBNODE node, void*** value_arry);

#ifdef  __cplusplus
}
#endif