#include <string.h>
#include "./lib_svr_common_def.h"
#include "../include/rb_tree.h"
#include "../include/memory_pool.h"
#include "../include/utility.hpp"

#define	RB_RED		    0
#define	RB_BLACK	    1
#define rb_color(r)   ((r)->rb_color)
#define rb_is_red(r)    (!rb_color(r))
#define rb_is_black(r)  rb_color(r)

TLS_VAR HMEMORYUNIT def_rb_tree_unit = 0;
TLS_VAR HMEMORYUNIT def_rb_node_unit = 0;

static inline void _rb_link_node(rb_tree* root, rb_node* node, rb_node* parent, rb_node** rb_link)
{
    node->rb_color = RB_RED;
    node->rb_parent = parent;
    node->rb_left = node->rb_right = 0;

    *rb_link = node;

    if (parent)
    {
        if (&(parent->rb_left) == rb_link)
        {
            if (root->head == parent)
            {
                node->next = parent;
                node->prev = 0;

                parent->prev = node;
                root->head = node;
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
            if (root->tail == parent)
            {
                node->next = 0;
                node->prev = parent;

                parent->next = node;
                root->tail = node;
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
        root->head = node;
        root->tail = node;
        node->next = 0;
        node->prev = 0;
    }

    ++root->size;
}

static inline void _rb_erase_list(rb_tree* root, rb_node* node)
{
    if (node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        root->head = node->next;
    }

    if (node->next)
    {
        node->next->prev = node->prev;
    }
    else
    {
        root->tail = node->prev;
    }

    --root->size;
}

void _rb_rotate_left(rb_node* node, rb_tree* root)
{
    rb_node *right = node->rb_right;
    rb_node *parent = node->rb_parent;

    node->rb_right = right->rb_left;

    if (node->rb_right)
        right->rb_left->rb_parent = node;

    right->rb_left = node;

    right->rb_parent = parent;

    if (parent)
    {
        if (node == parent->rb_left)
            parent->rb_left = right;
        else
            parent->rb_right = right;
    }
    else
        root->root = right;

    node->rb_parent = right;
}

static void _rb_rotate_right(rb_node *node, rb_tree *root)
{
    rb_node *left = node->rb_left;
    rb_node *parent = node->rb_parent;

    node->rb_left = left->rb_right;

    if (node->rb_left)
        left->rb_right->rb_parent = node;

    left->rb_right = node;

    left->rb_parent = parent;

    if (parent)
    {
        if (node == parent->rb_right)
            parent->rb_right = left;
        else
            parent->rb_left = left;
    }
    else
        root->root = left;
    node->rb_parent = left;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4706 )
#endif

void _rb_insert_balance(rb_node *node, rb_tree *root)
{
    rb_node *parent, *gparent;

    while ((parent = node->rb_parent) && rb_is_red(parent))
    {
        gparent = parent->rb_parent;

        if (parent == gparent->rb_left)
        {
            {
                register rb_node *uncle = gparent->rb_right;
                if (uncle && rb_is_red(uncle))
                {
                    uncle->rb_color = RB_BLACK;
                    parent->rb_color = RB_BLACK;
                    gparent->rb_color = RB_RED;

                    node = gparent;
                    continue;
                }
            }

            if (parent->rb_right == node)
            {
                register rb_node *tmp;
                _rb_rotate_left(parent, root);
                tmp = parent;
                parent = node;
                node = tmp;
            }

            parent->rb_color = RB_BLACK;
            gparent->rb_color = RB_RED;

            _rb_rotate_right(gparent, root);
        }
        else
        {
            {
                register rb_node *uncle = gparent->rb_left;
                if (uncle && rb_is_red(uncle))
                {
                    uncle->rb_color = RB_BLACK;
                    parent->rb_color = RB_BLACK;
                    gparent->rb_color = RB_RED;

                    node = gparent;
                    continue;
                }
            }

            if (parent->rb_left == node)
            {
                register rb_node *tmp;
                _rb_rotate_right(parent, root);
                tmp = parent;
                parent = node;
                node = tmp;
            }

            parent->rb_color = RB_BLACK;
            gparent->rb_color = RB_RED;

            _rb_rotate_left(gparent, root);
        }
    }

    root->root->rb_color = RB_BLACK;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

void _rb_erase_balance(rb_node *node, rb_node *parent, rb_tree *root)
{
    rb_node *other;

    while ((!node || rb_is_black(node)) && node != root->root)
    {
        if (parent->rb_left == node)
        {
            other = parent->rb_right;
            if (rb_is_red(other))
            {
                other->rb_color = RB_BLACK;
                parent->rb_color = RB_RED;
                _rb_rotate_left(parent, root);
                other = parent->rb_right;
            }
            if ((!other->rb_left || rb_is_black(other->rb_left)) &&
                (!other->rb_right || rb_is_black(other->rb_right)))
            {
                other->rb_color = RB_RED;
                node = parent;
                parent = node->rb_parent;
            }
            else
            {
                if (!other->rb_right || rb_is_black(other->rb_right))
                {
                    other->rb_left->rb_color = RB_BLACK;
                    other->rb_color = RB_RED;
                    _rb_rotate_right(other, root);
                    other = parent->rb_right;
                }
                other->rb_color = parent->rb_color;
                parent->rb_color = RB_BLACK;
                other->rb_right->rb_color = RB_BLACK;

                _rb_rotate_left(parent, root);
                node = root->root;
                break;
            }
        }
        else
        {
            other = parent->rb_left;
            if (rb_is_red(other))
            {
                other->rb_color = RB_BLACK;
                parent->rb_color = RB_RED;

                _rb_rotate_right(parent, root);
                other = parent->rb_left;
            }
            if ((!other->rb_left || rb_is_black(other->rb_left)) &&
                (!other->rb_right || rb_is_black(other->rb_right)))
            {
                other->rb_color = RB_RED;
                node = parent;
                parent = node->rb_parent;
            }
            else
            {
                if (!other->rb_left || rb_is_black(other->rb_left))
                {
                    other->rb_right->rb_color = RB_BLACK;
                    other->rb_color = RB_RED;
                    _rb_rotate_left(other, root);
                    other = parent->rb_left;
                }
                other->rb_color = parent->rb_color;
                parent->rb_color = RB_BLACK;
                other->rb_left->rb_color = RB_BLACK;
                _rb_rotate_right(parent, root);
                node = root->root;
                break;
            }
        }
    }
    if (node)
        node->rb_color = RB_BLACK;
}

void rb_tree_erase(rb_tree* root, rb_node* node)
{
    rb_node *child, *parent, *del;
    int color;

    if (node)
    {
        del = node;

        if (!node->rb_left)
            child = node->rb_right;
        else if (!node->rb_right)
            child = node->rb_left;
        else
        {
            rb_node *old = node, *left;

            node = node->rb_right;
            while ((left = node->rb_left) != 0)
                node = left;

            if (old->rb_parent) {
                if (old->rb_parent->rb_left == old)
                    old->rb_parent->rb_left = node;
                else
                    old->rb_parent->rb_right = node;
            }
            else
                root->root = node;

            child = node->rb_right;
            parent = node->rb_parent;
            color = rb_color(node);

            if (parent == old) {
                parent = node;
            }
            else {
                if (child)
                    child->rb_parent = parent;

                parent->rb_left = child;

                node->rb_right = old->rb_right;
                old->rb_right->rb_parent = node;
            }

            node->rb_parent = old->rb_parent;
            node->rb_color = old->rb_color;
            node->rb_left = old->rb_left;
            old->rb_left->rb_parent = node;

            goto color;
        }

        parent = node->rb_parent;
        color = rb_color(node);

        if (child)
            child->rb_parent = parent;

        if (parent)
        {
            if (parent->rb_left == node)
                parent->rb_left = child;
            else
                parent->rb_right = child;
        }
        else
            root->root = child;

    color:
        if (color == RB_BLACK)
            _rb_erase_balance(child, parent, root);

        _rb_erase_list(root, del);
        memory_unit_free(root->node_unit, del);
    }
}

rb_node* rb_first(rb_tree* tree)
{
    return tree->head;
}

rb_node* rb_last(rb_tree* tree)
{
    return tree->tail;
}

rb_node* rb_next(rb_node* node)
{
    return node->next;
}

rb_node* rb_prev(rb_node* node)
{
    return node->prev;
}

size_t rb_tree_size(HRBTREE tree)
{
    return tree->size;
}

rb_tree* create_rb_tree(compare_func key_cmp_func)
{
    rb_tree* tree = (rb_tree*)memory_unit_alloc(def_rb_tree_unit);
    tree->tree_unit = def_rb_tree_unit;
    tree->node_unit = def_rb_node_unit;

    tree->root = 0;
    tree->size = 0;
    tree->head = 0;
    tree->tail = 0;
    tree->key_cmp = key_cmp_func;

    return tree;
}

rb_tree* create_rb_tree_ex(compare_func key_cmp_func, HMEMORYUNIT tree_unit, HMEMORYUNIT node_unit)
{
    rb_tree* tree = 0;

    if (!tree_unit)
    {
        tree_unit = def_rb_tree_unit;
    }

    if (!node_unit)
    {
        node_unit = def_rb_node_unit;
    }

    tree = (rb_tree*)memory_unit_alloc(tree_unit);
    tree->tree_unit = tree_unit;
    tree->node_unit = node_unit;

    tree->root = 0;
    tree->size = 0;
    tree->head = 0;
    tree->tail = 0;
    tree->key_cmp = key_cmp_func;

    return tree;
}

void destroy_rb_tree(rb_tree* tree)
{
    rb_node* node = tree->head;

    while (node)
    {
        memory_unit_free(tree->node_unit, node);
        node = node->next;
    }

    memory_unit_free(tree->tree_unit, tree);
}

void rb_tree_clear(rb_tree* tree)
{
    rb_node* node = tree->head;

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

size_t rb_node_key_integer(rb_node* node)
{
    return node->key.v_integer;
}

void* rb_node_key_user(rb_node* node)
{
    return node->key.v_pointer;
}

size_t rb_node_value_integer(rb_node* node)
{
    return node->value.v_integer;
}

HMEMORYUNIT rb_tree_unit(rb_tree* tree)
{
    return tree->tree_unit;
}
HMEMORYUNIT rb_node_unit(rb_tree* tree)
{
    return tree->node_unit;
}

size_t sizeof_rb_tree(void)
{
    return sizeof(rb_tree);
}

size_t sizeof_rb_node(void)
{
    return sizeof(rb_node);
}

void* rb_node_value_user(rb_node* node)
{
    return node->value.v_pointer;
}

void rb_node_set_value_integer(rb_node* node, size_t new_value)
{
    node->value.v_integer = new_value;
}

void rb_node_set_value_user(rb_node* node, void* new_user_data)
{
    node->value.v_pointer = new_user_data;
}

compare_func rb_tree_cmp_func(rb_tree* tree)
{
    return tree->key_cmp;
}

rb_node* rb_tree_insert_integer(rb_tree* tree, size_t key, void* user_data)
{
    rb_node* node;
    rb_node** test = &(tree->root);
    rb_node* parent = 0;

    while (*test)
    {
        parent = *test;

        if (key < parent->key.v_integer)
        {
            test = &(parent->rb_left);
        }
        else if (key > parent->key.v_integer)
        {
            test = &(parent->rb_right);
        }
        else
        {
            parent->value.v_pointer = user_data;
            return parent;
        }
    }

    node = (rb_node*)memory_unit_alloc(tree->node_unit);

    node->key.v_integer = key;
    node->value.v_pointer = user_data;

    _rb_link_node(tree, node, parent, test);
    _rb_insert_balance(node, tree);

    return node;
}

bool rb_tree_try_insert_integer(rb_tree* tree, size_t key, void* user_data, rb_node** insert_or_exist_node)
{
    rb_node* node;
    rb_node** test = &(tree->root);
    rb_node* parent = 0;

    while (*test)
    {
        parent = *test;

        if (key < parent->key.v_integer)
        {
            test = &(parent->rb_left);
        }
        else if (key > parent->key.v_integer)
        {
            test = &(parent->rb_right);
        }
        else
        {
            *insert_or_exist_node = parent;
            return false;
        }
    }

    node = (rb_node*)memory_unit_alloc(tree->node_unit);

    node->key.v_integer = key;
    node->value.v_pointer = user_data;

    _rb_link_node(tree, node, parent, test);
    _rb_insert_balance(node, tree);

    *insert_or_exist_node = node;
    return node;
}

rb_node* rb_tree_find_integer(rb_tree* tree, size_t key)
{
    rb_node* node = tree->root;

    while (node)
    {
        if (key < node->key.v_integer)
        {
            node = node->rb_left;
        }
        else if (key > node->key.v_integer)
        {
            node = node->rb_right;
        }
        else
            return node;
    }

    return 0;
}

rb_node* rb_tree_insert_user(rb_tree* tree, void* key, void* user_data)
{
    rb_node* node;
    rb_node** test = &(tree->root);
    rb_node* parent = 0;

    while (*test)
    {
        parent = *test;

        ptrdiff_t cmp_ret = tree->key_cmp(key, parent->key.v_pointer);

        if (cmp_ret < 0)
        {
            test = &(parent->rb_left);
        }
        else if (cmp_ret > 0)
        {
            test = &(parent->rb_right);
        }
        else
        {
            parent->key.v_pointer = key;
            parent->value.v_pointer = user_data;
            return parent;
        }
    }

    node = (rb_node*)memory_unit_alloc(tree->node_unit);

    node->key.v_pointer = key;
    node->value.v_pointer = user_data;

    _rb_link_node(tree, node, parent, test);
    _rb_insert_balance(node, tree);

    return node;
}

bool rb_tree_try_insert_user(rb_tree* tree, void* key, void* user_data, rb_node** insert_or_exist_node)
{
    rb_node* node;
    rb_node** test = &(tree->root);
    rb_node* parent = 0;

    while (*test)
    {
        parent = *test;

        ptrdiff_t cmp_ret = tree->key_cmp(key, parent->key.v_pointer);

        if (cmp_ret < 0)
        {
            test = &(parent->rb_left);
        }
        else if (cmp_ret > 0)
        {
            test = &(parent->rb_right);
        }
        else
        {
            *insert_or_exist_node = parent;
            return false;
        }
    }

    node = (rb_node*)memory_unit_alloc(tree->node_unit);

    node->key.v_pointer = key;
    node->value.v_pointer = user_data;

    _rb_link_node(tree, node, parent, test);
    _rb_insert_balance(node, tree);

    *insert_or_exist_node = node;
    return true;
}

rb_node* rb_tree_find_user(rb_tree* tree, void* key)
{
    rb_node* node = tree->root;

    while (node)
    {
        ptrdiff_t cmp_ret = tree->key_cmp(key, node->key.v_pointer);

        if (cmp_ret < 0)
        {
            node = node->rb_left;
        }
        else if (cmp_ret > 0)
        {
            node = node->rb_right;
        }
        else
            return node;
    }

    return 0;
}
