#pragma once
#ifdef  __cplusplus
extern "C" {
#endif

typedef struct st_delist_node
{
    struct st_delist_node* prev;
    struct st_delist_node* next;
}delist_node;

__inline void __delist_add(delist_node* new_node,
                           delist_node* prev,
                           delist_node* next)
{
    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}

__inline void __delist_del(delist_node* prev, delist_node* next)
{
    next->prev = prev;
    prev->next = next;
}

__inline void init_delist(delist_node* list)
{
    list->next = list;
    list->prev = list;
}

__inline void delist_add_front(delist_node* new_node, delist_node* exist_node)
{
    __delist_add(new_node, exist_node, exist_node->next);
}

__inline void delist_add_back(delist_node* new_node, delist_node* exist_node)
{
    __delist_add(new_node, exist_node->prev, exist_node);
}

__inline void delist_del(delist_node* exist_node)
{
    __delist_del(exist_node->prev, exist_node->next);
}

__inline void delist_replace(delist_node* old_node, delist_node* new_node)
{
    new_node->next = old_node->next;
    new_node->next->prev = new_node;
    new_node->prev = old_node->prev;
    new_node->prev->next = new_node;
}

__inline void delist_replace_init(delist_node* old_node, delist_node* new_node)
{
    delist_replace(old_node, new_node);
    init_delist(old_node);
}

__inline int delist_empty(delist_node* head)
{
    return head->next == head;
}

#ifdef  __cplusplus
}
#endif