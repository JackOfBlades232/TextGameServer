/* TextGameServer/simple_fool.c */
#include "logic.h"
#include <stdlib.h>

#define MAX_PLAYERS_PER_GAME 2

struct session_logic_tag {
    server_logic *serv;
    session_interface *interf;

    // @TEST
    int id;
    int lines_read;

    // @TODO: add game logic
};

struct server_logic_tag {
    session_logic *players[MAX_PLAYERS_PER_GAME];
    int num_players;

    // @TODO: add game logic
};

#define OUTBUF_POST(_sess_l, _str) do { \
    _sess_l->interf->out_buf = strdup(_str); \
    _sess_l->interf->out_buf_len = strlen(_sess_l->interf->out_buf); \
} while (0)

#define OUTBUF_POSTF(_sess_l, _max_len, _fmt, ...) do { \
    _sess_l->interf->out_buf = malloc(_max_len * sizeof(*_sess_l->interf->out_buf)); \
    _sess_l->interf->out_buf_len = sprintf(_sess_l->interf->out_buf, _fmt, ##__VA_ARGS__); \
} while (0)

server_logic *make_server_logic()
{
    server_logic *serv = malloc(sizeof(*serv));
    for (int i = 0; i < sizeof(serv->players); i++)
        serv->players[i] = NULL;
    serv->num_players = 0;

    return serv;
}

session_logic *make_session_logic(server_logic *serv_l,
                                  session_interface *interf)
{
    ASSERT(serv_l);
    ASSERT(interf);

    session_logic *sess = malloc(sizeof(*sess));
    sess->serv = serv_l;
    sess->interf = interf;

    if (serv_l->num_players >= MAX_PLAYERS_PER_GAME) {
        // @TEST
        OUTBUF_POSTF(sess, 64, "The server is full (%d/%d)!\r\n",
                     MAX_PLAYERS_PER_GAME, MAX_PLAYERS_PER_GAME);
                    
        interf->quit = true;
        return sess;
    }

    // @TEST
    sess->id = serv_l->num_players;
    sess->lines_read = 0;

    serv_l->players[serv_l->num_players] = sess;
    serv_l->num_players++;

    return sess;
}

void destroy_session_logic(session_logic *sess_l)
{
    ASSERT(sess_l);

    for (int i = 0; i < sizeof(sess_l->serv->players); i++) {
        if (sess_l->serv->players[i] == sess_l) {
            sess_l->serv->players[i] = NULL;
            sess_l->serv->num_players--;
        }
    }

    // @TODO: logically handle one player disconnection

    free(sess_l);
}

void session_logic_process_line(session_logic *sess_l, const char *line)
{
    OUTBUF_POSTF(sess_l, 128, "Session %d read line %d, here it is: %s\r\n",
                 sess_l->id, ++(sess_l->lines_read), line);
}

void session_logic_post_too_long_line_msg(session_logic *sess_l)
{
    OUTBUF_POST(sess_l, "ERR: Line was too long\r\n");
}
