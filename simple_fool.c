/* TextGameServer/simple_fool.c */
#include "logic.h"
#include "utils.h"
#include "fool_data_structures.c"
#include <stdlib.h>
#include <limits.h>

// View
#define CHARS_TO_TRUMP       70
static char clrscr[] = "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                       "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                       "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n";

struct session_logic_tag {
    server_logic_t *serv;
    session_interface_t *interf;

    player_state_t state;
    linked_list_t *hand;
    bool can_attack;
};

struct server_logic_tag {
    session_logic_t *players[MAX_PLAYERS_PER_GAME];
    int num_players;

    game_state_t state;
    int num_active_players;

    int defender_index;
    int attacker_index;
    int attackers_left;

    deck_t deck;
    table_t table;
};

#define OUTBUF_POST(_sess_l, _str) do { \
    if (_sess_l->interf->out_buf) free(_sess_l->interf->out_buf); \
    _sess_l->interf->out_buf = strdup(_str); \
    _sess_l->interf->out_buf_len = strlen(_sess_l->interf->out_buf); \
} while (0)

#define OUTBUF_POSTF(_sess_l, _fmt, ...) do { \
    if (_sess_l->interf->out_buf) free(_sess_l->interf->out_buf); \
    size_t req_size = snprintf(NULL, 0, _fmt, ##__VA_ARGS__) + 1; \
    _sess_l->interf->out_buf = malloc(req_size * sizeof(*_sess_l->interf->out_buf)); \
    _sess_l->interf->out_buf_len = sprintf(_sess_l->interf->out_buf, _fmt, ##__VA_ARGS__); \
} while (0)

static void reset_server_logic(server_logic_t *serv_l);

server_logic_t *make_server_logic()
{
    server_logic_t *serv_l = malloc(sizeof(*serv_l));
    reset_server_logic(serv_l);

    return serv_l;
}

void destroy_server_logic(server_logic_t *serv_l)
{
    ASSERT(serv_l);
    free(serv_l);
}

static void start_game(server_logic_t *serv_l);

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

    if (serv_l->num_players >= MAX_PLAYERS_PER_GAME) {
        OUTBUF_POSTF(sess, "The server is full (%d/%d)!\r\n",
                     MAX_PLAYERS_PER_GAME, MAX_PLAYERS_PER_GAME);
        
        interf->quit = true;
        return sess;
    } else if (serv_l->state != gs_awaiting_players) {
        OUTBUF_POST(sess, "The game has already started! Try again later\r\n");
        interf->quit = true;
        return sess;
    }

    serv_l->players[serv_l->num_players++] = sess;

    if (serv_l->num_players == MAX_PLAYERS_PER_GAME)
        start_game(serv_l);        

    return sess;
}

static void end_game_with_message(server_logic_t *serv_l, const char *msg);

void destroy_session_logic(session_logic_t *sess_l)
{
    ASSERT(sess_l);
    server_logic_t *serv_l = sess_l->serv;

    // Remove from player array and shift others
    bool offset = false;
    for (int i = 0; i < serv_l->num_players; i++) {
        if (serv_l->players[i] == sess_l) {
            serv_l->num_players--;
            offset = true;

            if (serv_l->defender_index >= i)
                serv_l->defender_index--;
            if (serv_l->attacker_index >= i)
                serv_l->attacker_index--;
        } else if (offset) {
            serv_l->players[i-1] = serv_l->players[i];
            serv_l->players[i] = NULL;
        }
    }

    // If a live player has disconnected, end the game
    if (
            (serv_l->state == gs_first_card || serv_l->state == gs_free_for_all) &&
            sess_l->state != ps_spectating && !sess_l->interf->quit
       )
    {
        end_game_with_message(serv_l,  
                "\r\nA player has disconnected, thus the game can not continue. Goodbye!\r\n");
    }

    ll_free(sess_l->hand);
    free(sess_l);
}

void session_logic_process_too_long_line(session_logic_t *sess_l)
{
    OUTBUF_POST(sess_l, "ERR: Line was too long\r\n");
    sess_l->interf->quit = true;
}

static void process_attacker_first_card(session_logic_t *sess_l, server_logic_t *serv_l, const char *line);
static void process_defender_first_card(session_logic_t *sess_l, server_logic_t *serv_l, const char *line);
static void process_attacker_in_free_for_all(session_logic_t *sess_l, server_logic_t *serv_l, const char *line);
static void process_defender_in_free_for_all(session_logic_t *sess_l, server_logic_t *serv_l, const char *line);

void session_logic_process_line(session_logic_t *sess_l, const char *line)
{
    server_logic_t *serv_l = sess_l->serv;
    // If waiting for game to start and somebody pressed ENTER, start
    if (serv_l->state == gs_awaiting_players) {
        if (serv_l->num_players >= MIN_PLAYERS_PER_GAME && strlen(line) == 0)
            start_game(serv_l);
        return;
    } else if (serv_l->state == gs_game_end)
        return;

    if (serv_l->state == gs_first_card && sess_l->state == ps_defending)
        process_defender_first_card(sess_l, serv_l, line);
    else if (serv_l->state == gs_first_card && sess_l->state == ps_attacking)
        process_attacker_first_card(sess_l, serv_l, line);
    else if (serv_l->state == gs_free_for_all && sess_l->state == ps_defending)
        process_defender_in_free_for_all(sess_l, serv_l, line);
    else if (serv_l->state == gs_free_for_all && sess_l->state == ps_attacking)
        process_attacker_in_free_for_all(sess_l, serv_l, line);
}

static void end_game_with_message(server_logic_t *serv_l, const char *msg)
{
    // @TODO: add some possible delay before disconnecting all players
    //      (may just sleep the thread lol)
    for (int i = 0; i < serv_l->num_players; i++) {
        session_logic_t *sess_l = serv_l->players[i]; 
        if (sess_l) {
            if (msg)
                OUTBUF_POSTF(sess_l, "%s%s", clrscr, msg);
            sess_l->interf->quit = true;
        }
    }

    reset_server_logic(serv_l);
}

static void reset_server_logic(server_logic_t *serv_l)
{
    for (int i = 0; i < MAX_PLAYERS_PER_GAME; i++)
        serv_l->players[i] = NULL;
    serv_l->num_players = 0;
    serv_l->num_active_players = 0;

    serv_l->state = gs_awaiting_players;
    serv_l->defender_index = 0;
    serv_l->attacker_index = 0;
    serv_l->attackers_left = 0;
}

static void send_updates_to_players(server_logic_t *serv_l);

static void replenish_hands(server_logic_t *serv_l);
static void choose_first_turn(server_logic_t *serv_l);

static void start_game(server_logic_t *serv_l)
{
    ASSERT(serv_l->state == gs_awaiting_players);
    ASSERT(serv_l->num_players >= MIN_PLAYERS_PER_GAME);

    generate_deck(&serv_l->deck);
    reset_table(&serv_l->table);

    serv_l->state = gs_first_card;
    serv_l->num_active_players = serv_l->num_players;

    replenish_hands(serv_l);
    choose_first_turn(serv_l);

    send_updates_to_players(serv_l);
}

static void replenish_hands(server_logic_t *serv_l)
{
    ASSERT(serv_l->state == gs_first_card || serv_l->state == gs_free_for_all);

    int player_idx = serv_l->attacker_index;

    for (int i = 0; i < serv_l->num_players; i++)
    {
        session_logic_t *player = serv_l->players[player_idx];
        if (player->state == ps_spectating) {
            inc_cycl(&player_idx, serv_l->num_players);
            continue;
        }

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
    ASSERT(serv_l->state == gs_first_card);

    // Choose player with the lowest trump card
    int min_card_val = INT_MAX;
    for (int i = 0; i < serv_l->num_players; i++)
    {
        session_logic_t *player = serv_l->players[i];
        list_node_t *card_node = player->hand->head;
        while (card_node) {
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

    serv_l->defender_index = prev_cycl(serv_l->attacker_index, serv_l->num_players);

    serv_l->players[serv_l->defender_index]->state = ps_defending;
    serv_l->players[serv_l->attacker_index]->state = ps_attacking;
    serv_l->attackers_left = 1;
}

static void respond_to_invalid_command(session_logic_t *sess_l);
static void switch_turn(server_logic_t *serv_l, bool defender_lost);
static void enable_free_for_all(server_logic_t *serv_l);
static list_node_t *try_retrieve_card_from_hand(session_logic_t *sess_l, const char *line);

static void process_attacker_first_card(session_logic_t *sess_l, server_logic_t *serv_l, const char *line)
{
    ASSERT(serv_l->state == gs_first_card && sess_l->state == ps_attacking);

    table_t *table = &serv_l->table;

    if (strlen(line) == 0) // Chosen attacker can not forfeit first round
        respond_to_invalid_command(sess_l);
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(sess_l, line);
        if (!card_node)
            respond_to_invalid_command(sess_l);
        else if (attacker_try_play_card(table, card_node->data)) {
            ll_remove(sess_l->hand, card_node);
            // Once the first attacker card is placed, it is free for all
            enable_free_for_all(serv_l);

            send_updates_to_players(serv_l);
        } else
            respond_to_invalid_command(sess_l);
    }
}

static void process_defender_first_card(session_logic_t *sess_l, server_logic_t *serv_l, const char *line)
{
    ASSERT(serv_l->state == gs_first_card && sess_l->state == ps_defending);

    // No actions can be performed by defender before first card
    respond_to_invalid_command(sess_l);
}

static void process_attacker_in_free_for_all(session_logic_t *sess_l, server_logic_t *serv_l, const char *line)
{
    ASSERT(serv_l->state == gs_free_for_all && sess_l->state == ps_attacking);

    table_t *table = &serv_l->table;
    linked_list_t *def_hand = serv_l->players[serv_l->defender_index]->hand;

    if (strlen(line) == 0) {
        sess_l->state = ps_waiting;

        if (sess_l->can_attack)
            serv_l->attackers_left--;
        if (serv_l->attackers_left == 0 && table_is_beaten(table))
            switch_turn(serv_l, false);

        if (serv_l->state != gs_game_end)
            send_updates_to_players(serv_l);
    } else if (table_is_full(table, def_hand))
        respond_to_invalid_command(sess_l);
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(sess_l, line);
        if (!card_node)
            respond_to_invalid_command(sess_l);
        else if (attacker_try_play_card(table, card_node->data)) {
            ll_remove(sess_l->hand, card_node);

            if (!player_can_attack(table, sess_l->hand)) {
                sess_l->can_attack = false;
                serv_l->attackers_left--;
            }

            if (serv_l->state != gs_game_end)
                send_updates_to_players(serv_l);
        } else
            respond_to_invalid_command(sess_l);
    }
}

static void process_defender_in_free_for_all(session_logic_t *sess_l, server_logic_t *serv_l, const char *line)
{
    ASSERT(serv_l->state == gs_free_for_all && sess_l->state == ps_defending);

    table_t *table = &serv_l->table;
    linked_list_t *def_hand = serv_l->players[serv_l->defender_index]->hand;
    card_suit_t trump_suit = serv_l->deck.trump.suit;

    if (strlen(line) == 0) {
        // We assume table is not beaten, cause if it is and turn is not over, then attackers > 0 && table is not full
        if (serv_l->attackers_left > 0 && !table_is_full(table, def_hand))
            respond_to_invalid_command(sess_l); // Can't forfeit when waiting for cards from attackers
        else {
            switch_turn(serv_l, true);
            if (serv_l->state != gs_game_end)
                send_updates_to_players(serv_l);
        }
    } else if (table_is_beaten(table))
        respond_to_invalid_command(sess_l); // Cant defend at a beaten table
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(sess_l, line);
        if (!card_node)
            respond_to_invalid_command(sess_l);
        else if (defender_try_play_card(table, card_node->data, trump_suit)) {
            ll_remove(sess_l->hand, card_node);

            if (table_is_full(table, def_hand) && table_is_beaten(table))
                switch_turn(serv_l, false);
            else { // Once the defender, attackers get a new chance to throw in
                enable_free_for_all(serv_l);
                if (serv_l->attackers_left == 0 && table_is_beaten(table))
                    switch_turn(serv_l, false);
            }

            if (serv_l->state != gs_game_end)
                send_updates_to_players(serv_l);
        } else
            respond_to_invalid_command(sess_l);
    }
}

static void sb_add_attacker_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   server_logic_t *serv_l);
static void sb_add_defender_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   server_logic_t *serv_l);
static void sb_add_card(string_builder_t *sb, card_t card);

static void send_updates_to_players(server_logic_t *serv_l)
{
    for (int i = 0; i < serv_l->num_players; i++) {
        session_logic_t *sess_l = serv_l->players[i];
        string_builder_t *sb = sb_create();

        // Clear screen
        sb_add_str(sb, clrscr);

        // Player card counts
        int num_players = serv_l->num_players;
        size_t chars_used = 0; 
        int player_idx = i;
        for (dec_cycl(&player_idx, num_players); 
                player_idx != i;
                dec_cycl(&player_idx, num_players))
        {
            session_logic_t *player = serv_l->players[player_idx];
            const char *fmt = player_idx == serv_l->defender_index ? "| %d |   " : "< %d >   ";
            chars_used += sb_add_strf(sb, fmt, player->hand->size);
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
                sb_add_str(sb, "\r\n   ");
                sb_add_card(sb, *faceoff[0]);

                // Print defender cards where they exist
                if (i < table->cards_beat) {
                    sb_add_str(sb, " / ");
                    sb_add_card(sb, *faceoff[1]);
                }
            }
            sb_add_str(sb, "\r\n\r\n");
        }

        // Hand
        linked_list_t *hand = sess_l->hand;
        list_node_t *node = hand->head;
        for (int i = 0; i < hand->size; i++) {
            card_t *card = node->data;

            sb_add_strf(sb, "%c: ", card_char_index(i));
            sb_add_card(sb, *card);
            sb_add_str(sb, "   ");

            node = node->next;
        }
        sb_add_str(sb, "\r\n");

        // Prompts
        if (sess_l->state == ps_attacking)
            sb_add_attacker_prompt(sb, sess_l->hand, serv_l);
        else if (sess_l->state == ps_defending)
            sb_add_defender_prompt(sb, sess_l->hand, serv_l);

        char *full_str = sb_build_string(sb);
        OUTBUF_POST(sess_l, full_str);
        free(full_str);
        sb_free(sb);
    }
}

static void respond_to_invalid_command(session_logic_t *sess_l)
{
    string_builder_t *sb = sb_create();
    sb_add_str(sb, "Invalid command! Please type in one of the available letters, or just press ENTER to skip turn\r\n"
                   "If you are the first attacker, or defending with an empty table, you can not skip your turn\r\n");

    if (sess_l->state == ps_attacking)
        sb_add_attacker_prompt(sb, sess_l->hand, sess_l->serv);
    else if (sess_l->state == ps_defending)
        sb_add_defender_prompt(sb, sess_l->hand, sess_l->serv);

    char *full_str = sb_build_string(sb);
    OUTBUF_POST(sess_l, full_str);
    free(full_str);
    sb_free(sb);
}

static void sb_add_attacker_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   server_logic_t *serv_l)
{
    table_t *t = &serv_l->table;
    list_node_t *node = hand->head;
    linked_list_t *def_hand = serv_l->players[serv_l->defender_index]->hand;
    if (!table_is_full(t, def_hand)) {
        for (int i = 0; i < hand->size; i++) {
            card_t *card = node->data;
            if (attacker_can_play_card(t, *card))
                sb_add_strf(sb, "%c", card_char_index(i));

            node = node->next;
        }
    }
    sb_add_str(sb, " > ");
}

static void sb_add_defender_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   server_logic_t *serv_l)
{
    table_t *t = &serv_l->table;
    card_suit_t trump_suit = serv_l->deck.trump.suit;
    list_node_t *node = hand->head;
    if (!table_is_beaten(t)) {
        for (int i = 0; i < hand->size; i++) {
            card_t *card = node->data;
            if (defender_can_play_card(t, *card, trump_suit))
                sb_add_strf(sb, "%c", card_char_index(i));

            node = node->next;
        }
    }
    sb_add_str(sb, " => ");
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

static void advance_turns(server_logic_t *serv_l, int num_turns);
static void send_win_lose_messages_to_players(server_logic_t *serv_l);
static void send_draw_messages_to_players(server_logic_t *serv_l);

static void switch_turn(server_logic_t *serv_l, bool defender_lost)
{
    table_t *t = &serv_l->table;
    session_logic_t *defender = serv_l->players[serv_l->defender_index];

    if (defender_lost)
        flush_table(t, defender->hand);
    else
        reset_table(t);

    replenish_hands(serv_l);

    // Game end check
    for (int i = 0; i < serv_l->num_players; i++) {
        session_logic_t *sess_l = serv_l->players[i];
        if (sess_l->state != ps_spectating && ll_is_empty(sess_l->hand)) {
            sess_l->state = ps_spectating;
            serv_l->num_active_players--;
        }
    }

    if (serv_l->num_active_players == 1) {
        serv_l->state = gs_game_end;
        send_win_lose_messages_to_players(serv_l);

        end_game_with_message(serv_l, NULL);
    } else if (serv_l->num_active_players <= 0) {
        serv_l->state = gs_game_end;
        send_draw_messages_to_players(serv_l);

        end_game_with_message(serv_l, NULL);
    } else {
        serv_l->state = gs_first_card;
        advance_turns(serv_l, defender_lost ? 2 : 1);
    }
}

static void enable_free_for_all(server_logic_t *serv_l)
{
    serv_l->state = gs_free_for_all;
    serv_l->attackers_left = 0;

    table_t *table = &serv_l->table;
    for (int i = 0; i < serv_l->num_players; i++) {
        session_logic_t *sess_l = serv_l->players[i];
        if (sess_l->state == ps_waiting)
            sess_l->state = ps_attacking;

        // Only count those attackers that can do smth
        if (sess_l->state == ps_attacking) {
            if (player_can_attack(table, sess_l->hand)) {
                sess_l->can_attack = true;
                serv_l->attackers_left++;
            } else 
                sess_l->can_attack = false;
        } 
    }
}

static list_node_t *try_retrieve_card_from_hand(session_logic_t *sess_l, const char *line)
{
    if (strlen(line) != 1)
        return NULL;

    int card_int_index = card_char_index_to_int(*line); 
    if (card_int_index < 0)
        return NULL;

    return ll_find_at(sess_l->hand, card_int_index);
}

static void advance_turns(server_logic_t *serv_l, int num_turns)
{
    ASSERT(num_turns > 0 && serv_l->num_active_players > 1);

    while (num_turns > 0) {
        dec_cycl(&serv_l->attacker_index, serv_l->num_players);
        if (serv_l->players[serv_l->attacker_index]->state != ps_spectating)
            num_turns--;
    }

    serv_l->defender_index = prev_cycl(serv_l->attacker_index, serv_l->num_players);
    while (serv_l->players[serv_l->defender_index]->state == ps_spectating)
        dec_cycl(&serv_l->defender_index, serv_l->num_players);

    for (int i = 0; i < serv_l->num_players; i++) {
        if (i == serv_l->attacker_index)
            serv_l->players[i]->state = ps_attacking;
        else if (i == serv_l->defender_index)
            serv_l->players[i]->state = ps_defending;
        else if (serv_l->players[i]->state != ps_spectating)
            serv_l->players[i]->state = ps_waiting;
    }

    serv_l->attackers_left = 1;
}

static void send_win_lose_messages_to_players(server_logic_t *serv_l)
{
    for (int i = 0; i < serv_l->num_players; i++) {
        session_logic_t *sess_l = serv_l->players[i];
        if (sess_l->state == ps_spectating)
            OUTBUF_POSTF(sess_l, "%sYou've won! Kinda\r\n", clrscr);
        else
            OUTBUF_POSTF(sess_l, "%sYou're the fool! Oopsy-daisy)\r\n", clrscr);
    }
}

static void send_draw_messages_to_players(server_logic_t *serv_l)
{
    for (int i = 0; i < serv_l->num_players; i++)
        OUTBUF_POSTF(serv_l->players[i], "%sSeems that nobody is the fool today! What a pity\r\n", clrscr);
}
