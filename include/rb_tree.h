#pragma once
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern HRBTREE(create_rb_tree)(compare_func key_cmp_func);
extern HRBTREE(create_rb_tree_ex)(compare_func key_cmp_func, HMEMORYUNIT tree_unit, HMEMORYUNIT node_unit);
extern void destroy_rb_tree(HRBTREE tree);

extern void rb_tree_clear(HRBTREE tree);

extern size_t rb_tree_size(HRBTREE tree);

extern HRBNODE rb_first(HRBTREE tree);
extern HRBNODE rb_last(HRBTREE tree);
extern HRBNODE rb_next(HRBNODE node);
extern HRBNODE rb_prev(HRBNODE node);

extern HMEMORYUNIT rb_tree_unit(HRBTREE tree);
extern HMEMORYUNIT rb_node_unit(HRBTREE tree);

extern size_t sizeof_rb_tree(void);
extern size_t sizeof_rb_node(void);

extern size_t rb_node_value_integer(HRBNODE node);
extern void* rb_node_value_user(HRBNODE node);

extern void rb_node_set_value_integer(HRBNODE node, size_t new_value);
extern void rb_node_set_value_user(HRBNODE node, void* new_user_data);

extern compare_func rb_tree_cmp_func(HRBTREE tree);

extern void rb_tree_erase(HRBTREE tree, HRBNODE node);

extern HRBNODE rb_tree_insert_integer(HRBTREE tree, size_t key, void* user_data);
extern bool rb_tree_try_insert_integer(HRBTREE tree, size_t key, void* user_data, HRBNODE* insert_or_exist_node);
extern HRBNODE rb_tree_find_integer(HRBTREE tree, size_t key);
extern size_t rb_node_key_integer(HRBNODE node);

extern HRBNODE rb_tree_insert_user(HRBTREE tree, const void* key, void* user_data);
extern bool rb_tree_try_insert_user(HRBTREE tree, const void* key, void* user_data, HRBNODE* insert_or_exist_node);
extern HRBNODE rb_tree_find_user(HRBTREE tree, const void* key);
extern const void* rb_node_key_user(HRBNODE node);

#ifdef  __cplusplus
}
#endif