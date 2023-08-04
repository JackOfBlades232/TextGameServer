/* TextGameServer/utils.h */
#ifndef UTILS_SENTRY
#define UTILS_SENTRY

#include "defs.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct sized_array_tag {
    void **data;
    int size;
} sized_array_t;

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
bool ll_remove(linked_list_t *list, list_node_t *node);
bool ll_remove_at(linked_list_t *list, int idx);

static inline bool ll_is_empty(linked_list_t *list) { return list->size == 0; }

typedef struct string_builder_tag string_builder_t;

string_builder_t *sb_create();
void sb_free(string_builder_t *sb);
size_t sb_add_str(string_builder_t *sb, const char *str);
size_t sb_add_strf(string_builder_t *sb, const char *fmt, ...);
char *sb_build_string(string_builder_t *sb);

static inline bool char_is_a_symbol(int c) { return c >= '!' && c <= '~'; }

static inline bool streq(const char *s1, const char *s2) { return strcmp(s1, s2) == 0; }

char *strcat_alloc(const char *s1, const char *s2);

size_t fread_word_to_buf(FILE *f, char *buf, size_t bufsize, int *break_char);

void inc_cycl(int *i, int len); 
int next_cycl(int i, int len);
void dec_cycl(int *i, int len); 
int prev_cycl(int i, int len);

static inline int randint(int min, int max)
{
    return min + (int) ((float) (max-min+1) * rand() / (RAND_MAX+1.0));
}

#endif
