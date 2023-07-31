/* TextGameServer/utils.h */
#ifndef UTILS_SENTRY
#define UTILS_SENTRY

#include <stddef.h>

typedef struct string_builder_tag string_builder_t;

string_builder_t *sb_create();
void sb_free(string_builder_t *sb);
size_t sb_add_str(string_builder_t *sb, const char *str);
size_t sb_add_strf(string_builder_t *sb, const char *fmt, ...);
char *sb_build_string(string_builder_t *sb);

void inc_cycl(int *i, int len); 
int next_cycl(int i, int len);
void dec_cycl(int *i, int len); 
int prev_cycl(int i, int len);

#endif
