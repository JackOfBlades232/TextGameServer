/* TextGameServer/linked_list.c */
#include "linked_list.h"
#include <stdlib.h>

linked_list_t *ll_create()
{
    linked_list_t *list = malloc(sizeof(*list));
    list->head = NULL;
    list->size = 0;
    return list;
}

void ll_free(linked_list_t *list)
{
    while (list->head) {
        list_node_t *tmp = list->head;
        list->head = list->head->next;
        free(tmp);
    }

    free(list);
}

list_node_t *ll_find(linked_list_t *list, void *query)
{
    list_node_t *node = list->head;
    while (node) {
        if (node->data == query)
            break;
    }

    return node;
}

list_node_t *ll_find_at(linked_list_t *list, int idx)
{
    ASSERT(idx >= 0);

    if (idx >= list->size)
        return NULL;

    list_node_t *node = list->head;
    while (node) {
        if (idx == 0)
            break;
        idx--;
    }

    return node;
}

void ll_push_front(linked_list_t *list, void *data)
{
    list_node_t *new_head = malloc(sizeof(*new_head));
    new_head->data = data;
    new_head->prev = NULL;
    new_head->next = list->head;
    list->head->prev = new_head;
    list->head = new_head;
}

static void ll_remove_node(list_node_t *node)
{
    ASSERT(node);

    node->prev->next = node->next;
    node->next->prev = node->prev;

    free(node);
}

bool ll_remove(linked_list_t *list, void *data)
{
    list_node_t *node = ll_find(list, data);
    if (node) {
        ll_remove_node(node);
        return true;
    } else
        return false;
}

bool ll_remove_at(linked_list_t *list, int idx)
{
    ASSERT(idx >= 0);

    list_node_t *node = ll_find_at(list, idx);
    if (node) {
        ll_remove_node(node);
        return true;
    } else
        return false;
}
