/* TextGameServer/chat.c */
#include "chat.h"
#include "utils.h"
#include <string.h>

#define MAX_CHAT_MSG_LEN 64

// @TODO: refac dumb 2-field null checks

chat_t *make_chat()
{
    chat_t *c = malloc(sizeof(*c));
    c->head = 0;
    c->tail = 0;
    for (int i = 0; i < CHAT_MSG_HISTORY_SIZE; i++) {
        c->history[i].username = NULL;
        c->history[i].content = NULL;
    }

    return c;
}

void destroy_chat(chat_t *c)
{
    for (int i = 0; i < CHAT_MSG_HISTORY_SIZE; i++) {
        if (c->history[i].username)
            free(c->history[i].username);
        if (c->history[i].content)
            free(c->history[i].content);
    }
    free(c);
}

bool chat_try_post_message(chat_t *c, server_logic_t *serv_l,
                           session_logic_t *author_sl, const char *msg)
{
    //ASSERT(msg);
    if (strlen(msg) > MAX_CHAT_MSG_LEN)
        return false;

    if (c->history[c->head].username && c->history[c->head].content && c->tail == c->head)
        inc_cycl(&c->head, CHAT_MSG_HISTORY_SIZE);

    c->history[c->tail].username = strdup(author_sl->username);
    c->history[c->tail].content = strdup(msg);
    inc_cycl(&c->tail, CHAT_MSG_HISTORY_SIZE);

    for (int i = 0; i < serv_l->sess_cnt; i++) {
        session_logic_t *sess_l = serv_l->sess_refs[i];
        if (sess_l->is_in_chat && sess_l != author_sl)
            OUTBUF_POSTF(sess_l, "%s: %s\r\n", sess_l->username, msg);
    }

    return true;
}

void chat_send_updates(chat_t *c, session_logic_t *sess_l, const char *header)
{
    string_builder_t *sb = sb_create();
    sb_add_str(sb, clrscr);
    if (header)
        sb_add_str(sb, header);

    int msg_idx = c->head;
    for (int i = 0; i < CHAT_MSG_HISTORY_SIZE; i++) {
        chat_message_t *msg = &c->history[msg_idx];
        if (!msg->username || !msg->content)
            break;

        if (streq(sess_l->username, msg->username))
            sb_add_strf(sb, "You: %s\r\n", msg->content);
        else
            sb_add_strf(sb, "%s: %s\r\n", msg->username, msg->content);
    }

    char *full_str = sb_build_string(sb);
    OUTBUF_POST(sess_l, full_str);
    free(full_str);
    sb_free(sb);
}
