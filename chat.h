/* TextGameServer/chat.h */
#ifndef CHAT_SENTRY
#define CHAT_SENTRY

#include "defs.h"

#define CHAT_MSG_HISTORY_SIZE 16

typedef struct chat_message_tag {
    char *username;
    char *content;
    bool used;
} chat_message_t;

typedef struct chat_tag {
    chat_message_t history[CHAT_MSG_HISTORY_SIZE];
    int head, tail;
} chat_t;

#endif
