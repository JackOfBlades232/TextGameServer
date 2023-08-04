/* TextGameServer/chat.h */
#ifndef CHAT_SENTRY
#define CHAT_SENTRY

#include "logic.h"

#define CHAT_MSG_HISTORY_SIZE 16

typedef struct chat_message_tag {
    char *username;
    char *content;
} chat_message_t;

typedef struct chat_tag {
    chat_message_t history[CHAT_MSG_HISTORY_SIZE];
    int head, tail;
} chat_t;

chat_t *make_chat();
void destroy_chat(chat_t *c);
bool chat_try_post_message(chat_t *c, server_logic_t *serv_l,
                           session_logic_t *author_sl, const char *msg);
void chat_send_updates(chat_t *c, session_logic_t *sess_l, const char *header);

#endif
