/* TextGameServer/simple_fool.c */
#include "logic.h"
#include <stdlib.h>

#define MAX_PLAYERS_PER_GAME 2

struct session_state_tag {
    server_state *serv;
    session_interface *interf;

    // @TEST
    int id;
    int lines_read;

    // @TODO: add game logic
};

struct server_state_tag {
    session_state *players[MAX_PLAYERS_PER_GAME];
    int num_players;

    // @TODO: add game logic
};

server_state *make_server_state()
{
    server_state *serv = malloc(sizeof(*serv));
    for (int i = 0; i < sizeof(serv->players); i++)
        serv->players[i] = NULL;
    serv->num_players = 0;

    return serv;
}

session_state *make_session_state(server_state *serv_s,
                                  session_interface *interf)
{
    ASSERT(serv_s);
    ASSERT(interf);

    session_state *sess = malloc(sizeof(*sess));
    sess->serv = serv_s;
    sess->interf = interf;

    if (serv_s->num_players >= MAX_PLAYERS_PER_GAME) {
        // @TODO: factor out posting
        // @TEST
        sess->interf->out_buf = malloc(64 * sizeof(*sess->interf->out_buf));
        sess->interf->out_buf_len =
            sprintf(sess->interf->out_buf, "The server is full (%d/%d)!\n",
                    MAX_PLAYERS_PER_GAME, MAX_PLAYERS_PER_GAME);
                    
        interf->quit = true;
        return sess;
    }

    // @TEST
    sess->id = serv_s->num_players;
    sess->lines_read = 0;

    serv_s->players[serv_s->num_players] = sess;
    serv_s->num_players++;

    return sess;
}

void destroy_session_state(session_state *sess_s)
{
    ASSERT(sess_s);

    for (int i = 0; i < sizeof(sess_s->serv->players); i++) {
        if (sess_s->serv->players[i] == sess_s) {
            sess_s->serv->players[i] = NULL;
            sess_s->serv->num_players--;
        }
    }

    // @TODO: logically handle one player disconnection

    free(sess_s);
}

void session_state_process_line(session_state *sess_s, const char *line)
{
    // @TEST
    if (sess_s->interf->out_buf)
        return;

    sess_s->interf->out_buf = malloc(128 * sizeof(*sess_s->interf->out_buf));
    sess_s->interf->out_buf_len =
        sprintf(sess_s->interf->out_buf,
                "Session %d read line %d, here it is: %s\n",
                sess_s->id, ++(sess_s->lines_read), line);
}
