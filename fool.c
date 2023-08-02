/* TextGameServer/simple_fool.c */
#include "logic.h"
#include "utils.h"
#include "fool_data_structures.c"
#include <stdlib.h>
#include <limits.h>

// @HUH I don't like all the mingling of session_logic_t and fool-level data
// @IDEA make a mandatory ref list to all sessions, and set a global max_player? Not liking it

typedef struct fool_session_data_tag {
    player_state_t state;
    linked_list_t *hand;
    bool can_attack;
} fool_session_data_t;

typedef struct fool_server_data_tag {
    session_logic_t *players[MAX_PLAYERS_PER_GAME];
    int num_players;

    game_state_t state;
    int num_active_players;

    int defender_index;
    int attacker_index;
    int attackers_left;

    deck_t deck;
    table_t table;
} fool_server_data_t;

// View
#define CHARS_TO_TRUMP       70
static char clrscr[] = "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                       "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                       "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n";

static void reset_server_data(fool_server_data_t *sv_data);

void fool_init_server_logic(server_logic_t *serv_l);
{
    serv_l->data = malloc(sizeof(fool_server_data_t));
    reset_server_logic(serv_l);
}

void fool_deinit_server_logic(server_logic_t *serv_l);
{
    free(serv_l->data);
    serv_l->data = NULL;
}

static void start_game(fool_server_data_t *sv_data);

void fool_init_session_logic(session_logic_t *sess_l);
{
    sess_l->data = malloc(sizeof(fool_session_data_t));
    fool_session_data_t *s_data = sess_l->data;
    fool_server_data_t *sv_data = sess_l->serv->data;

    s_data->state = ps_waiting;
    s_data->hand = ll_create();

    if (sv_data->num_players >= MAX_PLAYERS_PER_GAME) {
        OUTBUF_POSTF(sess_l, "The server is full (%d/%d)!\r\n",
                     MAX_PLAYERS_PER_GAME, MAX_PLAYERS_PER_GAME);
        
        sess_l->interf->quit = true;
        return sess;
    } else if (sv_data->state != gs_awaiting_players) {
        OUTBUF_POST(sess_l, "The game has already started! Try again later\r\n");
        sess_l->interf->quit = true;
        return sess;
    }

    sv_data->players[sv_data->num_players++] = sess_l;

    if (sv_data->num_players == MAX_PLAYERS_PER_GAME)
        start_game(sv_data);        
}

static void end_game_with_message(fool_server_data_t *sv_data, const char *msg);

void fool_deinit_session_logic(session_logic_t *sess_l);
{
    fool_session_data_t *s_data = sess_l->data;
    fool_server_data_t *sv_data = sess_l->serv->data;

    // Remove from player array and shift others
    bool offset = false;
    for (int i = 0; i < sv_data->num_players; i++) {
        if (sv_data->players[i] == sess_l) {
            sv_data->num_players--;
            offset = true;

            if (sv_data->defender_index >= i)
                sv_data->defender_index--;
            if (sv_data->attacker_index >= i)
                sv_data->attacker_index--;
        } else if (offset) {
            sv_data->players[i-1] = sv_data->players[i];
            sv_data->players[i] = NULL;
        }
    }

    // If a live player has disconnected, end the game
    if (
            (sv_data->state == gs_first_card || sv_data->state == gs_free_for_all) &&
            s_data->state != ps_spectating && !s_data->interf->quit
       )
    {
        end_game_with_message(sv_data,  
                "\r\nA player has disconnected, thus the game can not continue. Goodbye!\r\n");
    }

    ll_free(s_data->hand);
    free(s_data);
    sess_l->data = NULL;
}

static void process_attacker_first_card(session_logic_t *sess_l, fool_server_data_t *sv_data, const char *line);
static void process_defender_first_card(session_logic_t *sess_l, fool_server_data_t *sv_data, const char *line);
static void process_attacker_in_free_for_all(session_logic_t *sess_l, fool_server_data_t *sv_data, const char *line);
static void process_defender_in_free_for_all(session_logic_t *sess_l, fool_server_data_t *sv_data, const char *line);

void session_logic_process_line(session_logic_t *sess_l, const char *line)
{
    fool_server_data_t *s_data = sess_l->data;
    fool_server_data_t *sv_data = sess_l->serv->data;

    // If waiting for game to start and somebody pressed ENTER, start
    if (sv_data->state == gs_awaiting_players) {
        if (sv_data->num_players >= MIN_PLAYERS_PER_GAME && strlen(line) == 0)
            start_game(sv_data);
        return;
    } else if (sv_data->state == gs_game_end)
        return;

    if (sv_data->state == gs_first_card && s_data->state == ps_defending)
        process_defender_first_card(sess_l, sv_data, line);
    else if (sv_data->state == gs_first_card && s_data->state == ps_attacking)
        process_attacker_first_card(sess_l, sv_data, line);
    else if (sv_data->state == gs_free_for_all && s_data->state == ps_defending)
        process_defender_in_free_for_all(sess_l, sv_data, line);
    else if (sv_data->state == gs_free_for_all && s_data->state == ps_attacking)
        process_attacker_in_free_for_all(sess_l, sv_data, line);
}

static void end_game_with_message(fool_server_data_t *sv_data, const char *msg)
{
    // @TODO: add some possible delay before disconnecting all players
    //      (may just sleep the thread lol)
    for (int i = 0; i < sv_data->num_players; i++) {
        session_logic_t *sess_l = sv_data->players[i]; 
        if (sess_l) {
            if (msg)
                OUTBUF_POSTF(sess_l, "%s%s", clrscr, msg);
            sess_l->interf->quit = true;
        }
    }

    reset_server_data(sv_data);
}

static void reset_server_data(fool_server_data_t *sv_data);
{
    for (int i = 0; i < MAX_PLAYERS_PER_GAME; i++)
        sv_data->players[i] = NULL;
    sv_data->num_players = 0;
    sv_data->num_active_players = 0;

    sv_data->state = gs_awaiting_players;
    sv_data->defender_index = 0;
    sv_data->attacker_index = 0;
    sv_data->attackers_left = 0;
}

static void send_updates_to_players(fool_server_data_t *sv_data);

static void replenish_hands(fool_server_data_t *sv_data);
static void choose_first_turn(fool_server_data_t *sv_data);

static void start_game(fool_server_data_t *sv_data)
{
    ASSERT(sv_data->state == gs_awaiting_players);
    ASSERT(sv_data->num_players >= MIN_PLAYERS_PER_GAME);

    generate_deck(&sv_data->deck);
    reset_table(&sv_data->table);

    sv_data->state = gs_first_card;
    sv_data->num_active_players = sv_data->num_players;

    replenish_hands(sv_data);
    choose_first_turn(sv_data);

    send_updates_to_players(sv_data);
}

static void replenish_hands(fool_server_data_t *sv_data)
{
    ASSERT(sv_data->state == gs_first_card || serv_l->state == gs_free_for_all);

    int player_idx = sv_data->attacker_index;

    for (int i = 0; i < sv_data->num_players; i++)
    {
        fool_session_data_t *s_data = sv_data->players[player_idx]->data;

        if (s_data->state == ps_spectating) {
            inc_cycl(&player_idx, sv_data->num_players);
            continue;
        }

        while (s_data->hand->size < BASE_PLAYER_CARDS) {
            card_t *card = pop_card_from_deck(&sv_data->deck);
            if (!card)
                goto loop_brk;

            ll_push_front(s_data->hand, card);
        }

        inc_cycl(&player_idx, sv_data->num_players);
    }

loop_brk:
    return;
}

static void choose_first_turn(fool_server_data_t *sv_data)
{
    ASSERT(sv_data->state == gs_first_card);

    // Choose player with the lowest trump card
    int min_card_val = INT_MAX;
    for (int i = 0; i < sv_data->num_players; i++)
    {
        fool_session_data_t *s_data = sv_data->players[i]->data;
        list_node_t *card_node = s_data->hand->head;
        while (card_node) {
            card_t *card = card_node->data;
            if (
                    card->suit == sv_data->deck.trump.suit &&
                    card->val < min_card_val
               ) 
            {
                sv_data->attacker_index = i;
                min_card_val = card->val;
            }

            card_node = card_node->next;
        }
    }

    sv_data->defender_index = prev_cycl(sv_data->attacker_index, sv_data->num_players);

    sv_data->players[sv_data->defender_index]->state = ps_defending;
    sv_data->players[sv_data->attacker_index]->state = ps_attacking;
    sv_data->attackers_left = 1;
}

static void respond_to_invalid_command(session_logic_t *sess_l);
static void switch_turn(fool_server_data_t *sv_data, bool defender_lost);
static void enable_free_for_all(fool_server_data_t *sv_data);
static list_node_t *try_retrieve_card_from_hand(fool_session_data_t *s_data, const char *line);

static void process_attacker_first_card(session_logic_t *sess_l, fool_server_data_t *sv_data, const char *line)
{
    fool_session_data_t *s_data = sess_l->data;
    ASSERT(sv_data->state == gs_first_card && s_data->state == ps_attacking);

    table_t *table = &s_data->table;

    if (strlen(line) == 0) // Chosen attacker can not forfeit first round
        respond_to_invalid_command(sess_l);
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(s_data, line);
        if (!card_node)
            respond_to_invalid_command(sess_l);
        else if (attacker_try_play_card(table, card_node->data)) {
            ll_remove(s_data->hand, card_node);
            // Once the first attacker card is placed, it is free for all
            enable_free_for_all(sv_data);

            send_updates_to_players(sv_data);
        } else
            respond_to_invalid_command(sess_l);
    }
}

static void process_defender_first_card(session_logic_t *sess_l, fool_server_data_t *sv_data, const char *line)
{
    fool_session_data_t *s_data = sess_l->data;
    ASSERT(sv_data->state == gs_first_card && s_data->state == ps_defending);

    // No actions can be performed by defender before first card
    respond_to_invalid_command(sess_l);
}

static void process_attacker_in_free_for_all(session_logic_t *sess_l, fool_server_data_t *sv_data, const char *line)
{
    fool_session_data_t *s_data = sess_l->data;
    ASSERT(sv_data->state == gs_free_for_all && s_data->state == ps_attacking);

    table_t *table = &sv_data->table;

    fool_session_data_t *def_data = sv_data->players[serv_l->defender_index]->data;
    linked_list_t *def_hand = def_data->hand;

    if (strlen(line) == 0) {
        s_data->state = ps_waiting;

        if (s_data->can_attack)
            sv_data->attackers_left--;
        if (sv_data->attackers_left == 0 && table_is_beaten(table))
            switch_turn(sv_data, false);

        if (sv_data->state != gs_game_end)
            send_updates_to_players(sv_data);
    } else if (table_is_full(table, def_hand))
        respond_to_invalid_command(s_data);
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(s_data, line);
        if (!card_node)
            respond_to_invalid_command(sess_l);
        else if (attacker_try_play_card(table, card_node->data)) {
            ll_remove(s_data->hand, card_node);

            if (!player_can_attack(table, s_data->hand)) {
                s_data->can_attack = false;
                sv_data->attackers_left--;
            }

            if (sv_data->state != gs_game_end)
                send_updates_to_players(sv_data);
        } else
            respond_to_invalid_command(sess_l);
    }
}

static void process_defender_in_free_for_all(session_logic_t *sess_l, fool_server_data_t *sv_data, const char *line)
{
    fool_session_data_t *s_data = sess_l->data;
    ASSERT(sv_data->state == gs_free_for_all && s_data->state == ps_defending);

    table_t *table = &sv_data->table;
    card_suit_t trump_suit = sv_data->deck.trump.suit;

    fool_session_data_t *def_data = sv_data->players[serv_l->defender_index]->data;
    linked_list_t *def_hand = def_data->hand;

    if (strlen(line) == 0) {
        // We assume table is not beaten, cause if it is and turn is not over, then attackers > 0 && table is not full
        if (sv_data->attackers_left > 0 && !table_is_full(table, def_hand))
            respond_to_invalid_command(sess_l); // Can't forfeit when waiting for cards from attackers
        else {
            switch_turn(sv_data, true);
            if (sv_data->state != gs_game_end)
                send_updates_to_players(sv_data);
        }
    } else if (table_is_beaten(table))
        respond_to_invalid_command(sess_l); // Cant defend at a beaten table
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(s_data, line);
        if (!card_node)
            respond_to_invalid_command(sess_l);
        else if (defender_try_play_card(table, card_node->data, trump_suit)) {
            ll_remove(s_data->hand, card_node);

            if (table_is_full(table, def_hand) && table_is_beaten(table))
                switch_turn(sv_data, false);
            else { // Once the defender, attackers get a new chance to throw in
                enable_free_for_all(sv_data);
                if (sv_data->attackers_left == 0 && table_is_beaten(table))
                    switch_turn(sv_data, false);
            }

            if (sv_data->state != gs_game_end)
                send_updates_to_players(sv_data);
        } else
            respond_to_invalid_command(sess_l);
    }
}

static void sb_add_attacker_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   fool_server_data_t *sv_data);
static void sb_add_defender_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   fool_server_data_t *sv_data);
static void sb_add_card(string_builder_t *sb, card_t card);


static void send_updates_to_players(fool_server_data_t *sv_data)
{
    for (int i = 0; i < sv_data->num_players; i++) {
        session_logic_t *sess_l = sv_data->players[i];
        fool_session_data_t *s_data = sess_l->data;
        string_builder_t *sb = sb_create();

        // Clear screen
        sb_add_str(sb, clrscr);

        // Player card counts
        int num_players = sv_data->num_players;
        size_t chars_used = 0; 
        int player_idx = i;
        for (dec_cycl(&player_idx, num_players); 
                player_idx != i;
                dec_cycl(&player_idx, num_players))
        {
            fool_session_data_t *p_data = sv_data->players[player_idx]->data;
            const char *fmt = player_idx == sv_data->defender_index ? "| %d |   " : "< %d >   ";
            chars_used += sb_add_strf(sb, fmt, p_data->hand->size);
        }

        // Deck info: trump & remaining cards
        sb_add_strf(sb, "%*c", CHARS_TO_TRUMP - chars_used, ' ');
        sb_add_card(sb, sv_data->deck.trump);
        sb_add_strf(sb, "  [ %d ]\r\n", deck_size(&sv_data->deck));

        // Table
        table_t *table = &sv_data->table;
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
        linked_list_t *hand = s_data->hand;
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
            sb_add_attacker_prompt(sb, s_data->hand, sv_data);
        else if (sess_l->state == ps_defending)
            sb_add_defender_prompt(sb, s_data->hand, sv_data);

        char *full_str = sb_build_string(sb);
        OUTBUF_POST(sess_l, full_str);
        free(full_str);
        sb_free(sb);
    }
}

static void respond_to_invalid_command(fool_session_data_t *sess_l)
{
    string_builder_t *sb = sb_create();
    sb_add_str(sb, "The command is invalid or can not be used now\r\n");

    fool_session_data_t *s_data = sess_l->data;
    if (sess_l->state == ps_attacking)
        sb_add_attacker_prompt(sb, s_data->hand, sess_l->serv);
    else if (sess_l->state == ps_defending)
        sb_add_defender_prompt(sb, s_data->hand, sess_l->serv);

    char *full_str = sb_build_string(sb);
    OUTBUF_POST(sess_l, full_str);
    free(full_str);
    sb_free(sb);
}

static void sb_add_attacker_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   fool_server_data_t *sv_data)
{
    table_t *t = &sv_data->table;
    list_node_t *node = hand->head;

    fool_session_data_t *def_data = sv_data->players[serv_l->defender_index]->data;
    linked_list_t *def_hand = def_data->hand;

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
                                   fool_server_data_t *sv_data)
{
    table_t *t = &sv_data->table;
    card_suit_t trump_suit = sv_data->deck.trump.suit;
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

static void advance_turns(fool_server_data_t *sv_data, int num_turns);
static void send_win_lose_messages_to_players(fool_server_data_t *sv_data);
static void send_draw_messages_to_players(fool_server_data_t *sv_data);

static void switch_turn(fool_server_data_t *sv_data, bool defender_lost)
{
    table_t *t = &sv_data->table;
    fool_session_data_t *def_data = sv_data->players[sv_data->defender_index]->data;

    if (defender_lost)
        flush_table(t, def_data->hand);
    else
        reset_table(t);

    replenish_hands(sv_data);

    // Game end check
    for (int i = 0; i < sv_data->num_players; i++) {
        fool_session_data_t *s_data = sv_data->players[i]->data;
        if (s_data->state != ps_spectating && ll_is_empty(s_data->hand)) {
            s_data->state = ps_spectating;
            sv_data->num_active_players--;
        }
    }

    if (sv_data->num_active_players == 1) {
        sv_data->state = gs_game_end;
        send_win_lose_messages_to_players(sv_data);

        end_game_with_message(sv_data, NULL);
    } else if (sv_data->num_active_players <= 0) {
        sv_data->state = gs_game_end;
        send_draw_messages_to_players(sv_data);

        end_game_with_message(sv_data, NULL);
    } else {
        sv_data->state = gs_first_card;
        advance_turns(sv_data, defender_lost ? 2 : 1);
    }
}

static void enable_free_for_all(fool_server_data_t *sv_data)
{
    sv_data->state = gs_free_for_all;
    sv_data->attackers_left = 0;

    table_t *table = &sv_data->table;
    for (int i = 0; i < sv_data->num_players; i++) {
        fool_session_data_t *s_data = sv_data->players[i]->data;
        if (s_data->state == ps_waiting)
            s_data->state = ps_attacking;

        // Only count those attackers that can do smth
        if (s_data->state == ps_attacking) {
            if (player_can_attack(table, s_data->hand)) {
                s_data->can_attack = true;
                sv_data->attackers_left++;
            } else 
                s_data->can_attack = false;
        } 
    }
}

static list_node_t *try_retrieve_card_from_hand(fool_session_data_t *s_data, const char *line)
{
    if (strlen(line) != 1)
        return NULL;

    int card_int_index = card_char_index_to_int(*line); 
    if (card_int_index < 0)
        return NULL;

    return ll_find_at(s_data->hand, card_int_index);
}

static void advance_turns(fool_server_data_t *sv_data, int num_turns)
{
    ASSERT(num_turns > 0 && sv_data->num_active_players > 1);

    while (num_turns > 0) {
        dec_cycl(&sv_data->attacker_index, sv_data->num_players);
        fool_session_data_t *a_data = sv_data->players[sv_data->attacker_index]->data;
        if (a_data->state != ps_spectating)
            num_turns--;
    }

    sv_data->defender_index = prev_cycl(sv_data->attacker_index, sv_data->num_players);
    fool_session_data_t *d_data = sv_data->players[sv_data->defender_index]->data;
    while (d_data->state == ps_spectating) {
        dec_cycl(&sv_data->defender_index, sv_data->num_players);
        d_data = sv_data->players[sv_data->defender_index]->data;
    }

    for (int i = 0; i < sv_data->num_players; i++) {
        fool_session_data_t *s_data = sv_data->players[i]->data;
        if (i == sv_data->attacker_index)
            s_data->state = ps_attacking;
        else if (i == sv_data->defender_index)
            s_data->state = ps_defending;
        else if (s_data->state != ps_spectating)
            s_data->state = ps_waiting;
    }

    sv_data->attackers_left = 1;
}

static void send_win_lose_messages_to_players(fool_server_data_t *sv_data)
{
    for (int i = 0; i < sv_data->num_players; i++) {
        session_logic_t *sess_l = sv_data->players[i];
        fool_session_data_t *s_data = sess_l->data;
        if (s_data->state == ps_spectating)
            OUTBUF_POSTF(sess_l, "%sYou've won! Kinda\r\n", clrscr);
        else
            OUTBUF_POSTF(sess_l, "%sYou're the fool! Oopsy-daisy)\r\n", clrscr);
    }
}

static void send_draw_messages_to_players(fool_server_data_t *serv_l)
{
    for (int i = 0; i < serv_l->num_players; i++)
        OUTBUF_POSTF(serv_l->players[i], "%sSeems that nobody is the fool today! What a pity\r\n", clrscr);
}
