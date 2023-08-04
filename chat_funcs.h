/* TextGameServer/chat_funcs.h */
#ifndef CHAT_FUNCS_SENTRY
#define CHAT_FUNCS_SENTRY

#include "chat.h"
#include "logic.h"

chat_t *make_chat();
void destroy_chat(chat_t *c);
bool chat_try_post_message(chat_t *c, server_logic_t *serv_l,
                           session_logic_t *author_sl, const char *msg);
void chat_send_updates(chat_t *c, session_logic_t *sess_l, const char *header);

#endif
