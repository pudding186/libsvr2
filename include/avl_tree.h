#pragma once
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern HAVLTREE(create_avl_tree)(compare_func key_cmp_func);
extern HAVLTREE(create_avl_tree_ex)(compare_func key_cmp_func, HMEMORYUNIT tree_unit, HMEMORYUNIT node_unit);
extern void destroy_avl_tree(HAVLTREE tree);

extern void avl_tree_clear(HAVLTREE tree);

extern size_t avl_tree_size(HAVLTREE tree);

extern HAVLNODE avl_first(HAVLTREE tree);
extern HAVLNODE avl_last(HAVLTREE tree);
extern HAVLNODE avl_next(HAVLNODE node);
extern HAVLNODE avl_prev(HAVLNODE node);

extern HMEMORYUNIT avl_tree_unit(HAVLTREE tree);
extern HMEMORYUNIT avl_node_unit(HAVLTREE tree);

extern size_t sizeof_avl_tree(void);
extern size_t sizeof_avl_node(void);

extern size_t avl_node_value_integer(HAVLNODE node);
extern void* avl_node_value_user(HAVLNODE node);

extern void avl_node_set_value_integer(HAVLNODE node, size_t new_value);
extern void avl_node_set_value_user(HAVLNODE node, void* new_user_data);

extern compare_func avl_tree_cmp_func(HAVLTREE tree);

extern void avl_tree_erase(HAVLTREE tree, HAVLNODE node);

extern HAVLNODE avl_tree_insert_integer(HAVLTREE tree, size_t key, void* user_data);
extern bool avl_tree_try_insert_integer(HAVLTREE tree, size_t key, void* user_data, HAVLNODE* insert_or_exist_node);
extern HAVLNODE avl_tree_find_integer(HAVLTREE tree, size_t key);
extern size_t avl_node_key_integer(HAVLNODE node);

extern HAVLNODE avl_tree_insert_user(HAVLTREE tree, void* key, void* user_data);
extern bool avl_tree_try_insert_user(HAVLTREE tree, void* key, void* user_data, HAVLNODE* insert_or_exist_node);
extern HAVLNODE avl_tree_find_user(HAVLTREE tree, void* key);
extern void* avl_node_key_user(HAVLNODE node);

#ifdef  __cplusplus
}
#endif