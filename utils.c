/* TextGameServer/utils.c */
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h> 

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
        node = node->next;
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
        node = node->next;
    }

    return node;
}

void ll_push_front(linked_list_t *list, void *data)
{
    list_node_t *new_head = malloc(sizeof(*new_head));
    new_head->data = data;
    new_head->prev = NULL;
    new_head->next = list->head;
    if (list->head)
        list->head->prev = new_head;
    list->head = new_head;

    list->size++;
}

bool ll_remove(linked_list_t *list, list_node_t *node)
{
    if (node) {
        if (node->prev)
            node->prev->next = node->next;
        else
            list->head = node->next;

        if (node->next)
            node->next->prev = node->prev;

        free(node);
        list->size--;
        return true;
    } else
        return false;
}

bool ll_remove_at(linked_list_t *list, int idx)
{
    ASSERT(idx >= 0);

    list_node_t *node = ll_find_at(list, idx);
    return ll_remove(list, node);
}

struct string_builder_tag {
    char **strings;
    int cnt, cap;
};
#define SB_BASE_CAP 8

string_builder_t *sb_create()
{
    string_builder_t *sb = malloc(sizeof(*sb));
    sb->cnt = 0;
    sb->cap = SB_BASE_CAP;
    sb->strings = malloc(sb->cap * sizeof(*sb->strings));

    return sb;
}

void sb_free(string_builder_t *sb)
{
    for (int i = 0; i < sb->cnt; i++)
        free(sb->strings[i]);
    free(sb->strings);
    free(sb);
}

size_t sb_add_str(string_builder_t *sb, const char *str)
{
    if (sb->cnt >= sb->cap) {
        while (sb->cnt >= sb->cap)
            sb->cap += SB_BASE_CAP;

        sb->strings = realloc(sb->strings, sb->cap * sizeof(*sb->strings));
    }

    sb->strings[sb->cnt++] = strdup(str);
    return strlen(str);
}

size_t sb_add_strf(string_builder_t *sb, const char *fmt, ...)
{
    va_list vl;

    va_start(vl, fmt);
    size_t len = vsnprintf(NULL, 0, fmt, vl);
    va_end(vl);

    char *str = malloc((len+1) * sizeof(*str));

    va_start(vl, fmt);
    vsprintf(str, fmt, vl);
    va_end(vl);

    sb_add_str(sb, str);
    free(str);
    return len;
}

char *sb_build_string(string_builder_t *sb)
{
    size_t tot_len = 0;
    for (int i = 0; i < sb->cnt; i++)
        tot_len += strlen(sb->strings[i]);

    char *str = malloc((tot_len+1) * sizeof(*str));
    char *write_p = str;
    for (int i = 0; i < sb->cnt; i++) {
        size_t len = strlen(sb->strings[i]);
        memcpy(write_p, sb->strings[i], len);
        write_p += len;
    }

    *write_p = '\0';
    return str;
}

char *strcat_alloc(const char *s1, const char *s2)
{
    size_t l1 = strlen(s1);
    size_t l2 = strlen(s2);
    char *res = malloc((l1+l2+1) * sizeof(*res));
    memcpy(res, s1, l1);
    memcpy(res+l1, s2, l2);
    res[l1+l2] = '\0';
    return res;
}

void inc_cycl(int *i, int len) 
{ 
    if (++(*i) >= len) 
        *i -= len; 
} 

int next_cycl(int i, int len) 
{ 
    if (++i >= len) 
        i -= len; 
    return i;
} 

void dec_cycl(int *i, int len)
{
    if (--(*i) < 0) 
        *i += len; 
}

int prev_cycl(int i, int len)
{
    if (--i < 0) 
        i += len; 
    return i;
}
