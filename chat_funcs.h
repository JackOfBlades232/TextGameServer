/* TextGameServer/chat_funcs.h */
#ifndef CHAT_FUNCS_SENTRY
#define CHAT_FUNCS_SENTRY

#include "chat.h"
#include "logic.h"

chat_t *make_chat();
void destroy_chat(chat_t *c);
bool chat_try_post_message(chat_t *c, server_room_t *s_room,
                           room_session_t *author_rs, const char *msg);
void chat_send_updates(chat_t *c, room_session_t *r_sess, const char *header);

#endif
