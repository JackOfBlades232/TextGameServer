/* TextGameServer/utils.c */
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h> 

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
