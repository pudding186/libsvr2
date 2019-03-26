#include <math.h>
#include "./lib_svr_common_def.h"
#include "../include/avl_tree.h"
#include "../include/memory_pool.h"
#include "../include/utility.hpp"

TLS_VAR HMEMORYUNIT def_avl_tree_unit = 0;
TLS_VAR HMEMORYUNIT def_avl_node_unit = 0;


__inline int _avl_node_height(avl_node* node)
{
    return node ? node->avl_height : 0;
}

__inline void _avl_link_node(avl_tree* tree, avl_node* node, avl_node* parent, avl_node** link)
{
    node->avl_child[0] = node->avl_child[1] = 0;
    node->avl_parent = parent;
    node->avl_height = 1;

    *link = node;

    if (parent)
    {
        if (&(parent->avl_child[0]) == link)
        {
            if (tree->head == parent)
            {
                node->next = parent;
                node->prev = 0;

                parent->prev = node;
                tree->head = node;
            }
            else
            {
                node->next = parent;
                node->prev = parent->prev;

                parent->prev->next = node;
                parent->prev = node;
            }
        }
        else
        {
            if (tree->tail == parent)
            {
                node->next = 0;
                node->prev = parent;

                parent->next = node;
                tree->tail = node;
            }
            else
            {
                node->next = parent->next;
                node->prev = parent;

                parent->next->prev = node;
                parent->next = node;
            }
        }
    }
    else
    {
        tree->head = node;
        tree->tail = node;
        node->next = 0;
        node->prev = 0;
    }

    ++tree->size;
}

void _avl_roate(avl_tree* tree, avl_node* node)
{
    int h0;
    int h1;

    const size_t side = (node != node->avl_parent->avl_child[0]);
    const size_t other_side = !side;

    avl_node* child = node->avl_child[other_side];
    avl_node* parent = node->avl_parent;

    node->avl_parent = parent->avl_parent;
    node->avl_child[other_side] = parent;

    parent->avl_child[side] = child;
    if (child)
        child->avl_parent = parent;

    if (parent->avl_parent)
    {
        const size_t parent_side = (parent != parent->avl_parent->avl_child[0]);
        parent->avl_parent->avl_child[parent_side] = node;
    }
    else
        tree->root = node;

    parent->avl_parent = node;

    h0 = _avl_node_height(parent->avl_child[0]);
    h1 = _avl_node_height(parent->avl_child[1]);

    if (h0 > h1)
    {
        parent->avl_height = 1 + h0;
    }
    else
    {
        parent->avl_height = 1 + h1;
    }

    h0 = _avl_node_height(node->avl_child[0]);
    h1 = _avl_node_height(node->avl_child[1]);

    if (h0 > h1)
    {
        node->avl_height = 1 + h0;
    }
    else
    {
        node->avl_height = 1 + h1;
    }
}

void _avl_balance(avl_tree* tree, avl_node* node)
{
    int h0;
    int h1;

    while (node)
    {
        ptrdiff_t balance;
        
        h0 = _avl_node_height(node->avl_child[0]);
        h1 = _avl_node_height(node->avl_child[1]);

        if (h0 > h1)
        {
            node->avl_height = 1 + h0;
        }
        else
        {
            node->avl_height = 1 + h1;
        }

        balance = h0 - h1;

        if (balance == 2 || balance == -2)
        {
            ptrdiff_t child_balance;

            avl_node* tall_child = node->avl_child[balance == -2];

            child_balance = _avl_node_height(tall_child->avl_child[0]) - _avl_node_height(tall_child->avl_child[1]);

            if (child_balance == 0 || (child_balance < 0) == (balance < 0))
            {
                _avl_roate(tree, tall_child);

                node = tall_child->avl_parent;
            }
            else
            {
                avl_node* tall_grand_child = tall_child->avl_child[child_balance == -1];

                _avl_roate(tree, tall_grand_child);
                _avl_roate(tree, tall_grand_child);

                node = tall_grand_child->avl_parent;
            }
        }
        else
        {
            node = node->avl_parent;
        }
    }
}

void _avl_splice_out(avl_tree* tree, avl_node* node)
{
    avl_node* parent = node->avl_parent;

    const size_t child_index = (node->avl_child[1] != 0);

    avl_node* child = node->avl_child[child_index];

    if (child)
        child->avl_parent = parent;

    if (parent)
    {
        parent->avl_child[node == parent->avl_child[1]] = child;
    }
    else
        tree->root = child;
}

avl_tree* create_avl_tree(compare_func key_cmp_func)
{
    avl_tree* tree = (avl_tree*)memory_unit_alloc(def_avl_tree_unit);
    tree->tree_unit = def_avl_tree_unit;
    tree->node_unit = def_avl_node_unit;

    tree->root = 0;
    tree->size = 0;
    tree->head = 0;
    tree->tail = 0;
    tree->key_cmp = key_cmp_func;

    return tree;
}

avl_tree* create_avl_tree_ex(compare_func key_cmp_func, HMEMORYUNIT tree_unit, HMEMORYUNIT node_unit)
{
    avl_tree* tree = 0;

    if (!tree_unit)
    {
        tree_unit = def_avl_tree_unit;
    }

    if (!node_unit)
    {
        node_unit = def_avl_node_unit;
    }

    tree = (avl_tree*)memory_unit_alloc(tree_unit);
    tree->tree_unit = tree_unit;
    tree->node_unit = node_unit;

    tree->root = 0;
    tree->size = 0;
    tree->head = 0;
    tree->tail = 0;
    tree->key_cmp = key_cmp_func;

    return tree;
}

void destroy_avl_tree(avl_tree* tree)
{
    avl_node* node = tree->head;

    while (node)
    {
        memory_unit_free(tree->node_unit, node);
        node = node->next;
    }

    memory_unit_free(tree->tree_unit, tree);
}

void avl_tree_clear(avl_tree* tree)
{
    avl_node* node = tree->head;

    while (node)
    {
        memory_unit_free(tree->node_unit, node);
        node = node->next;
    }

    tree->root = 0;
    tree->size = 0;
    tree->head = 0;
    tree->tail = 0;
}

size_t avl_tree_size(avl_tree* tree)
{
    return tree->size;
}

avl_node* avl_first(avl_tree* tree)
{
    return tree->head;
}

avl_node* avl_last(avl_tree* tree)
{
    return tree->tail;
}

avl_node* avl_next(avl_node* node)
{
    return node->next;
}

avl_node* avl_prev(avl_node* node)
{
    return node->prev;
}

size_t avl_node_key_integer(avl_node* node)
{
    return node->key.v_integer;
}

void* avl_node_key_user(avl_node* node)
{
    return node->key.v_pointer;
}

size_t avl_node_value_integer(avl_node* node)
{
    return node->value.v_integer;
}

HMEMORYUNIT avl_tree_unit(avl_tree* tree)
{
    return tree->tree_unit;
}
HMEMORYUNIT avl_node_unit(avl_tree* tree)
{
    return tree->node_unit;
}

size_t sizeof_avl_tree(void)
{
    return sizeof(avl_tree);
}

size_t sizeof_avl_node(void)
{
    return sizeof(avl_node);
}

void* avl_node_value_user(avl_node* node)
{
    return node->value.v_pointer;
}

void avl_node_set_value_integer(avl_node* node, size_t new_value)
{
    node->value.v_integer = new_value;
}

void avl_node_set_value_user(avl_node* node, void* new_user_data)
{
    node->value.v_pointer = new_user_data;
}

compare_func avl_tree_cmp_func(avl_tree* tree)
{
    return tree->key_cmp;
}

void avl_tree_erase(avl_tree* tree, avl_node* node)
{
    if (node)
    {
        avl_node* parent = node->avl_parent;

        if (!node->avl_child[0] || !node->avl_child[1])
        {
            _avl_splice_out(tree, node);
            _avl_balance(tree, parent);
        }
        else
        {
            avl_node* successor = node->next;

            avl_node* successor_parent = successor->avl_parent;

            _avl_splice_out(tree, successor);

            successor->avl_parent = parent;
            successor->avl_child[0] = node->avl_child[0];
            successor->avl_child[1] = node->avl_child[1];

            if (successor->avl_child[0])
            {
                successor->avl_child[0]->avl_parent = successor;
            }

            if (successor->avl_child[1])
            {
                successor->avl_child[1]->avl_parent = successor;
            }

            if (parent)
            {
                parent->avl_child[node == parent->avl_child[1]] = successor;
            }
            else
                tree->root = successor;

            _avl_balance(tree, node == successor_parent ? successor : successor_parent);
        }

        if (node->next)
        {
            node->next->prev = node->prev;
        }
        else
        {
            tree->tail = node->prev;
        }

        if (node->prev)
        {
            node->prev->next = node->next;
        }
        else
        {
            tree->head = node->next;
        }

        memory_unit_free(tree->node_unit, node);
        --tree->size;
    }
}

avl_node* avl_tree_insert_integer(avl_tree* tree, size_t key, void* user_data)
{
    avl_node* node;
    avl_node** test = &(tree->root);
    avl_node* parent = 0;

    while (*test)
    {
        parent = *test;

        if (key < parent->key.v_integer)
        {
            test = &(parent->avl_child[0]);
        }
        else if (key > parent->key.v_integer)
        {
            test = &(parent->avl_child[1]);
        }
        else
        {
            parent->value.v_pointer = user_data;
            return parent;
        }
    }

    node = (avl_node*)memory_unit_alloc(tree->node_unit);

    node->key.v_integer = key;
    node->value.v_pointer = user_data;

    _avl_link_node(tree, node, parent, test);
    _avl_balance(tree, node);

    return node;
}

bool avl_tree_try_insert_integer(avl_tree* tree, size_t key, void* user_data, avl_node** insert_or_exist_node)
{
    avl_node* node;
    avl_node** test = &(tree->root);
    avl_node* parent = 0;

    while (*test)
    {
        parent = *test;

        if (key < parent->key.v_integer)
        {
            test = &(parent->avl_child[0]);
        }
        else if (key > parent->key.v_integer)
        {
            test = &(parent->avl_child[1]);
        }
        else
        {
            *insert_or_exist_node = parent;
            return false;
        }
    }

    node = (avl_node*)memory_unit_alloc(tree->node_unit);

    node->key.v_integer = key;
    node->value.v_pointer = user_data;

    _avl_link_node(tree, node, parent, test);
    _avl_balance(tree, node);

    *insert_or_exist_node = node;
    return true;
}

avl_node* avl_tree_find_integer(avl_tree* tree, size_t key)
{
    avl_node* node = tree->root;

    while (node)
    {
        if (key < node->key.v_integer)
        {
            node = node->avl_child[0];
        }
        else if (key > node->key.v_integer)
        {
            node = node->avl_child[1];
        }
        else
            return node;
    }

    return 0;
}

avl_node* avl_tree_insert_user(avl_tree* tree, void* key, void* user_data)
{
    avl_node* node;
    avl_node** test = &(tree->root);
    avl_node* parent = 0;

    while (*test)
    {
        parent = *test;

        ptrdiff_t cmp_ret = tree->key_cmp(key, parent->key.v_pointer);

        if (cmp_ret < 0)
        {
            test = &(parent->avl_child[0]);
        }
        else if (cmp_ret > 0)
        {
            test = &(parent->avl_child[1]);
        }
        else
        {
            parent->key.v_pointer = key;
            parent->value.v_pointer = user_data;
            return parent;
        }
    }

    node = (avl_node*)memory_unit_alloc(tree->node_unit);

    node->key.v_pointer = key;
    node->value.v_pointer = user_data;

    _avl_link_node(tree, node, parent, test);
    _avl_balance(tree, node);

    return node;
}

bool avl_tree_try_insert_user(avl_tree* tree, void* key, void* user_data, avl_node** insert_or_exist_node)
{
    avl_node* node;
    avl_node** test = &(tree->root);
    avl_node* parent = 0;

    while (*test)
    {
        parent = *test;

        ptrdiff_t cmp_ret = tree->key_cmp(key, parent->key.v_pointer);

        if (cmp_ret < 0)
        {
            test = &(parent->avl_child[0]);
        }
        else if (cmp_ret > 0)
        {
            test = &(parent->avl_child[1]);
        }
        else
        {
            *insert_or_exist_node = parent;
            return false;
        }
    }

    node = (avl_node*)memory_unit_alloc(tree->node_unit);

    node->key.v_pointer = key;
    node->value.v_pointer = user_data;

    _avl_link_node(tree, node, parent, test);
    _avl_balance(tree, node);

    *insert_or_exist_node = node;
    return true;
}

avl_node* avl_tree_find_user(avl_tree* tree, void* key)
{
    avl_node* node = tree->root;

    while (node)
    {
        ptrdiff_t cmp_ret = tree->key_cmp(key, node->key.v_pointer);

        if (cmp_ret < 0)
        {
            node = node->avl_child[0];
        }
        else if (cmp_ret > 0)
        {
            node = node->avl_child[1];
        }
        else
            return node;
    }

    return 0;
}

//ptrdiff_t _bkdr_str_cmp(bkdr_str* bkdr_str1, bkdr_str* bkdr_str2)
//{
//    if (bkdr_str1->hash_code < bkdr_str2->hash_code)
//    {
//        return -1;
//    }
//    else if (bkdr_str1->hash_code > bkdr_str2->hash_code)
//    {
//        return 1;
//    }
//
//    return strcmp(bkdr_str1->str, bkdr_str2->str);
//}


