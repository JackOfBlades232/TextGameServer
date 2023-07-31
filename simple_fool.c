/* TextGameServer/simple_fool.c */
#include "logic.h"
#include "linked_list.h"
#include <stdlib.h>

#define MAX_PLAYERS_PER_GAME 8
#define MAX_TABLE_CARDS      6
#define DECK_SIZE            52

typedef enum card_suit_tag {
    cs_spades, 
    cs_clubs,
    cs_hearts,
    cs_diamonds
} card_suit_t;

typedef enum card_value_tag {
    cv_two    = 2,
    cv_three  = 3,
    cv_four   = 4,
    cv_five   = 5,
    cv_six    = 6,
    cv_seven  = 7,
    cv_eight  = 8,
    cv_nine   = 9,
    cv_ten    = 10,
    cv_jack   = 11,
    cv_queen  = 12,
    cv_king   = 13,
    cv_ace    = 14,
} card_value_t;

typedef struct card_tag {
    card_suit_t suit;
    card_value_t val;
} card_t;

typedef struct deck_tag {
    card_t cards[DECK_SIZE];
    card_t *head;
    card_t *trump;
} deck_t;

typedef struct table_tag {
    card_t *cards[MAX_TABLE_CARDS][2];
    int pairs_played;
    bool waiting_for_defender;
} table_t;

struct session_logic_tag {
    server_logic_t *serv;
    session_interface_t *interf;

    // @TEST
    int id;
    int lines_read;

    // @TODO: add game logic
    linked_list_t *hand;
};

struct server_logic_tag {
    session_logic_t *players[MAX_PLAYERS_PER_GAME];
    int num_players;

    deck_t deck;
    table_t table;
    // @TODO: add game state (waiting, playing), and more fine (which player defends, so on)
};

#define OUTBUF_POST(_sess_l, _str) do { \
    _sess_l->interf->out_buf = strdup(_str); \
    _sess_l->interf->out_buf_len = strlen(_sess_l->interf->out_buf); \
} while (0)

#define OUTBUF_POSTF(_sess_l, _max_len, _fmt, ...) do { \
    _sess_l->interf->out_buf = malloc(_max_len * sizeof(*_sess_l->interf->out_buf)); \
    _sess_l->interf->out_buf_len = sprintf(_sess_l->interf->out_buf, _fmt, ##__VA_ARGS__); \
} while (0)

server_logic_t *make_server_logic()
{
    server_logic_t *serv = malloc(sizeof(*serv));
    for (int i = 0; i < MAX_PLAYERS_PER_GAME; i++)
        serv->players[i] = NULL;
    serv->num_players = 0;

    return serv;
}

void destroy_server_logic(server_logic_t *serv_l)
{
    ASSERT(serv_l);
    free(serv_l);
}

session_logic_t *make_session_logic(server_logic_t *serv_l,
                                    session_interface_t *interf)
{
    ASSERT(serv_l);
    ASSERT(interf);

    session_logic_t *sess = malloc(sizeof(*sess));
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

void destroy_session_logic(session_logic_t *sess_l)
{
    ASSERT(sess_l);

    for (int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
        if (sess_l->serv->players[i] == sess_l) {
            sess_l->serv->players[i] = NULL;
            sess_l->serv->num_players--;
        }
    }

    // @TODO: logically handle one player disconnection

    free(sess_l);
}

void session_logic_process_line(session_logic_t *sess_l, const char *line)
{
    OUTBUF_POSTF(sess_l, 128, "Session %d read line %d, here it is: %s\r\n",
                 sess_l->id, ++(sess_l->lines_read), line);
}

void session_logic_process_too_long_line(session_logic_t *sess_l)
{
    OUTBUF_POST(sess_l, "ERR: Line was too long\r\n");
    sess_l->interf->quit = true;
}
