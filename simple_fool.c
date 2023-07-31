/* TextGameServer/simple_fool.c */
#include "logic.h"
#include "utils.h"
#include <stdlib.h>
#include <limits.h>

// @TODO: reoder/organize/factor apart functions
// @IDEA: what shall we do with incorrect-format strings? I think, just say "wrong format" and resend update

// @TODO: implement random attack order -- once the attacker put down a card, it's free for all

#define NUM_SUITS            4
#define NUM_VALS             13

#define BASE_PLAYER_CARDS    6
#define MAX_TABLE_CARDS      6
#define MIN_PLAYERS_PER_GAME 2
#define DECK_SIZE            NUM_SUITS*NUM_VALS
#define MAX_PLAYERS_PER_GAME DECK_SIZE/BASE_PLAYER_CARDS

// View
#define CHARS_TO_TRUMP       70

typedef enum card_suit_tag {
    cs_empty     = 0, 
    cs_spades    = 1, 
    cs_clubs     = 2,
    cs_hearts    = 3,
    cs_diamonds  = 4
} card_suit_t;

typedef enum card_value_tag {
    cv_empty  = 0,
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
    card_t trump;
    card_t *head;
} deck_t;

typedef struct table_tag {
    card_t *faceoffs[MAX_TABLE_CARDS][2];
    int cards_played;
    bool waiting_for_defender;
} table_t;

typedef enum player_state_tag {
    ps_waiting,
    ps_attacking,
    ps_defending,
    ps_spectating
} player_state_t;

static inline bool deck_size(deck_t *d)
{
    return d->head - d->cards + 1;
}

static inline bool deck_is_empty(deck_t *d)
{
    return deck_size(d) <= 0;
}

static void generate_deck(deck_t *d)
{
    for (int i = 0; i < DECK_SIZE; i++) {
        d->cards[i].suit = i % NUM_SUITS + cs_spades;
        d->cards[i].val = (i / NUM_SUITS) + cv_two;
    }

    // Fisher-Yates random permutation
    for (int i = 0; i < DECK_SIZE-1; i++) {
        // Random index from i to DECK_SIZE-1
        int xchg_idx = (DECK_SIZE-1) - ((int)((float) (DECK_SIZE-i) * rand() / (RAND_MAX+1.0)));

        card_t tmp = d->cards[i];
        d->cards[i] = d->cards[xchg_idx];
        d->cards[xchg_idx] = tmp;
    }

    d->head = &d->cards[DECK_SIZE-1];
    d->trump = d->cards[0];
}

static void init_table(table_t *t)
{
    for (int i = 0; i < MAX_TABLE_CARDS; i++) {
        t->faceoffs[i][0] = NULL;
        t->faceoffs[i][1] = NULL;
    }

    t->cards_played = 0;
    t->waiting_for_defender = false;
}

static card_t *pop_card_from_deck(deck_t *d)
{
    if (deck_is_empty(d))
        return NULL;

    card_t *ret = d->head;
    d->head--;
    return ret;
}

struct session_logic_tag {
    server_logic_t *serv;
    session_interface_t *interf;

    player_state_t state;
    linked_list_t *hand;
};

struct server_logic_tag {
    session_logic_t *players[MAX_PLAYERS_PER_GAME];
    int num_players;

    bool game_started;
    int defender_index;
    int attacker_index;

    deck_t deck;
    table_t table;
};

#define OUTBUF_POST(_sess_l, _str) do { \
    _sess_l->interf->out_buf = strdup(_str); \
    _sess_l->interf->out_buf_len = strlen(_sess_l->interf->out_buf); \
} while (0)

#define OUTBUF_POSTF(_sess_l, _fmt, ...) do { \
    size_t req_size = snprintf(NULL, 0, _fmt, ##__VA_ARGS__) + 1; \
    _sess_l->interf->out_buf = malloc(req_size * sizeof(*_sess_l->interf->out_buf)); \
    _sess_l->interf->out_buf_len = sprintf(_sess_l->interf->out_buf, _fmt, ##__VA_ARGS__); \
} while (0)

static void replenish_hands(server_logic_t *serv_l)
{
    int player_idx = serv_l->defender_index;
    inc_cycl(&player_idx, serv_l->num_players);

    for (int i = 0; i < serv_l->num_players; i++)
    {
        session_logic_t *player = serv_l->players[player_idx];
        if (player->state == ps_spectating)
            continue;

        while (player->hand->size < BASE_PLAYER_CARDS) {
            card_t *card = pop_card_from_deck(&serv_l->deck);
            if (!card)
                goto loop_brk;

            ll_push_front(player->hand, card);
        }

        inc_cycl(&player_idx, serv_l->num_players);
    }

loop_brk:
    return;
}

static void choose_first_turn(server_logic_t *serv_l)
{
    // Choose player with the lowest trump card
    int min_card_val = INT_MAX;
    for (int i = 0; i < serv_l->num_players; i++)
    {
        session_logic_t *player = serv_l->players[i];
        list_node_t *card_node = player->hand->head;
        while (card_node) {
            // @TODO: factor this out as a func
            card_t *card = card_node->data;
            if (
                    card->suit == serv_l->deck.trump.suit &&
                    card->val < min_card_val
               ) 
            {
                serv_l->attacker_index = i;
                min_card_val = card->val;
            }

            card_node = card_node->next;
        }
    }

    if (serv_l->attacker_index == -1)
        serv_l->attacker_index = 0;

    serv_l->defender_index = prev_cycl(serv_l->defender_index, serv_l->num_players);

    // @TODO: set player states
}

static void sb_add_card(string_builder_t *sb, card_t card)
{
    ASSERT(card.suit != cs_empty && card.val != cv_empty); 

    char suit_c;
    switch (card.suit) {
        case cs_spades:
            suit_c = '^';
            break;
        case cs_clubs:
            suit_c = '%';
            break;
        case cs_hearts:
            suit_c = 'v';
            break;
        case cs_diamonds:
            suit_c = '#';
            break;
        default:
            return;
    }

    if (card.val <= cv_ten)
        sb_add_strf(sb, "%d%c", card.val, suit_c);
    else if (card.val == cv_jack)
        sb_add_strf(sb, "J%c", suit_c);
    else if (card.val == cv_queen)
        sb_add_strf(sb, "Q%c", suit_c);
    else if (card.val == cv_king)
        sb_add_strf(sb, "K%c", suit_c);
    else if (card.val == cv_ace)
        sb_add_strf(sb, "A%c", suit_c);
}

static void send_update_to_player(server_logic_t *serv_l, int i)
{
    session_logic_t *self_sess = serv_l->players[i];
    string_builder_t *sb = sb_create();

    // Clear screen
    sb_add_str(sb, "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                   "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n");

    // Player cart counts
    int num_players = serv_l->num_players;
    size_t chars_used = 0; 
    int player_idx = i;
    for (dec_cycl(&player_idx, num_players); 
         player_idx != i;
         dec_cycl(&player_idx, num_players))
    {
        session_logic_t *player = serv_l->players[player_idx];
        chars_used += sb_add_strf(sb, "< %d >   ", player->hand->size);
    }

    // Deck info: trump & remaining cards
    sb_add_strf(sb, "%*c", CHARS_TO_TRUMP - chars_used, ' ');
    sb_add_card(sb, serv_l->deck.trump);
    sb_add_strf(sb, "  [ %d ]\r\n", deck_size(&serv_l->deck));

    // Table
    table_t *table = &serv_l->table;
    if (table->cards_played > 0) {
        for (int i = 0; i < table->cards_played; i++) {
            card_t **faceoff = table->faceoffs[i];
            sb_add_strf(sb, "\r\n   ");
            sb_add_card(sb, *faceoff[0]);

            // Do not print last card if defender is yet to answer
            if (i < table->cards_played - 1 || !table->waiting_for_defender)
                sb_add_card(sb, *faceoff[1]);
        }
        sb_add_strf(sb, "\r\n\r\n");
    }

    // Hand
    linked_list_t *hand = self_sess->hand;
    list_node_t *node = hand->head;
    for (int i = 0; i < hand->size; i++) {
        char card_index = i < 'z' - 'a' ? i + 'a' : i + 'A' - ('z' - 'a');
        card_t *card = node->data;

        sb_add_strf(sb, "%c: ", card_index);
        sb_add_card(sb, *card);
        sb_add_strf(sb, "   ");

        node = node->next;
    }
    sb_add_strf(sb, "\r\n");

    // @TODO: prompt available cards

    char *full_str = sb_build_string(sb);
    //printf("%s", full_str);
    OUTBUF_POST(self_sess, full_str);
    free(full_str);
    sb_free(sb);
}

static void send_updates_to_players(server_logic_t *serv_l)
{
    for (int i = 0; i < serv_l->num_players; i++)
        send_update_to_player(serv_l, i);
}

static void start_game(server_logic_t *serv_l)
{
    ASSERT(serv_l->num_players >= MIN_PLAYERS_PER_GAME);

    serv_l->game_started = true;
    replenish_hands(serv_l);
    choose_first_turn(serv_l);

    // @TEST
    send_updates_to_players(serv_l);
}

server_logic_t *make_server_logic()
{
    server_logic_t *serv = malloc(sizeof(*serv));

    for (int i = 0; i < MAX_PLAYERS_PER_GAME; i++)
        serv->players[i] = NULL;
    serv->num_players = 0;

    serv->game_started = false;
    serv->defender_index = -1;
    serv->attacker_index = -1;

    generate_deck(&serv->deck);
    init_table(&serv->table);

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

    sess->state = ps_waiting;
    sess->hand = ll_create();

    // @TODO: refac
    if (serv_l->num_players >= MAX_PLAYERS_PER_GAME) {
        OUTBUF_POSTF(sess, "The server is full (%d/%d)!\r\n",
                     MAX_PLAYERS_PER_GAME, MAX_PLAYERS_PER_GAME);
        
        interf->quit = true;
        return sess;
    } else if (serv_l->game_started) {
        OUTBUF_POST(sess, "The game has already started! Try again later\r\n");
        interf->quit = true;
        return sess;
    }

    serv_l->players[serv_l->num_players] = sess;
    serv_l->num_players++;

    if (serv_l->num_players == MAX_PLAYERS_PER_GAME)
        start_game(serv_l);        

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

    // @TODO: logically handle one player disconnection (oopsy-daisy all the other players)

    ll_free(sess_l->hand);
    free(sess_l);
}

void session_logic_process_line(session_logic_t *sess_l, const char *line)
{
    server_logic_t *serv_l = sess_l->serv;

    // If waiting for game to start
    if (!serv_l->game_started) {
        if (serv_l->num_players >= MIN_PLAYERS_PER_GAME && strlen(line) == 0)
            start_game(serv_l);
    } else {
    }
}

void session_logic_process_too_long_line(session_logic_t *sess_l)
{
    OUTBUF_POST(sess_l, "ERR: Line was too long\r\n");
    sess_l->interf->quit = true;
}
