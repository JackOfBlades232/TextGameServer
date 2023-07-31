/* TextGameServer/linked_list.h */
#ifndef LINKED_LIST_SENTRY
#define LINKED_LIST_SENTRY

#include "defs.h"

typedef struct list_node_tag {
    void *data;
    struct list_node_tag *next, *prev;
} list_node_t;

typedef struct linked_list_tag {
    list_node_t *head;
    int size;
} linked_list_t;

linked_list_t *ll_create();
void ll_free(linked_list_t *list);
list_node_t *ll_find(linked_list_t *list, void *query);
list_node_t *ll_find_at(linked_list_t *list, int idx);
void ll_push_front(linked_list_t *list, void *data);
bool ll_remove(linked_list_t *list, void *data);
bool ll_remove_at(linked_list_t *list, int idx);

inline bool ll_is_empty(linked_list_t *list) { return list->size == 0; }

#endif
