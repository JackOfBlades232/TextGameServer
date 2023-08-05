/* TextGameServer/chat.c */
#include "chat.h"
#include "chat_funcs.h"
#include "utils.h"
#include <string.h>

#define MAX_CHAT_MSG_LEN 64

chat_t *make_chat()
{
    chat_t *c = malloc(sizeof(*c));
    c->head = 0;
    c->tail = 0;
    for (int i = 0; i < CHAT_MSG_HISTORY_SIZE; i++) {
        c->history[i].username = NULL;
        c->history[i].content = NULL;
        c->history[i].used = false;
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

bool chat_try_post_message(chat_t *c, server_room_t *s_room,
                           room_session_t *author_sl, const char *msg)
{
    ASSERT(author_sl->is_in_chat);

    if (strlen(msg) > MAX_CHAT_MSG_LEN)
        return false;

    if (c->history[c->head].used && c->tail == c->head)
        inc_cycl(&c->head, CHAT_MSG_HISTORY_SIZE);

    c->history[c->tail].username = strdup(author_sl->username);
    c->history[c->tail].content = strdup(msg);
    c->history[c->tail].used = true;

    inc_cycl(&c->tail, CHAT_MSG_HISTORY_SIZE);

    for (int i = 0; i < s_room->sess_cnt; i++) {
        room_session_t *r_sess = s_room->sess_refs[i];
        if (r_sess->is_in_chat && r_sess != author_sl)
            OUTBUF_POSTF(r_sess, "%s: %s\r\n", author_sl->username, msg);
    }

    return true;
}

void chat_send_updates(chat_t *c, room_session_t *r_sess, const char *header)
{
    ASSERT(r_sess->is_in_chat);

    string_builder_t *sb = sb_create();
    sb_add_str(sb, clrscr);
    if (header)
        sb_add_str(sb, header);

    int msg_idx = c->head;
    for (int i = 0; i < CHAT_MSG_HISTORY_SIZE; i++) {
        chat_message_t *msg = &c->history[msg_idx];
        if (!msg->used)
            break;

        if (streq(r_sess->username, msg->username))
            sb_add_strf(sb, "%s\r\n", msg->content);
        else
            sb_add_strf(sb, "%s: %s\r\n", msg->username, msg->content);

        inc_cycl(&msg_idx, CHAT_MSG_HISTORY_SIZE);
    }

    char *full_str = sb_build_string(sb);
    OUTBUF_POST(r_sess, full_str);
    free(full_str);
    sb_free(sb);
}
