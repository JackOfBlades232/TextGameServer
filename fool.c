/* TextGameServer/fool.c */
#include "fool.h"
#include "logic.h"
#include "chat_funcs.h"
#include "utils.h"
#include "fool_data_structures.c"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>

typedef struct fool_session_data_tag {
    player_state_t state;
    linked_list_t *hand;
    bool can_attack;
} fool_session_data_t;

typedef struct fool_server_data_tag {
    game_state_t state;
    int num_active_players;

    int defender_index;
    int attacker_index;
    int attackers_left;

    deck_t deck;
    table_t table;

    server_logic_t *hub_ref;
} fool_server_data_t;

// View
#define CHARS_TO_TRUMP 70

static void reset_server_logic(server_logic_t *serv_l);

void fool_init_server_logic(server_logic_t *serv_l, void *payload)
{
    serv_l->sess_cap = MAX_PLAYERS_PER_GAME;
    serv_l->sess_refs = malloc(serv_l->sess_cap * sizeof(*serv_l->sess_refs));
    serv_l->data = malloc(sizeof(fool_server_data_t));

    fool_server_data_t *sv_data = serv_l->data;
    sv_data->hub_ref = payload;

    reset_server_logic(serv_l);
}

void fool_deinit_server_logic(server_logic_t *serv_l)
{
    free(serv_l->data);
    free(serv_l->sess_refs);
}

static void start_game(server_logic_t *serv_l);

void fool_init_session_logic(session_logic_t *sess_l)
{
    sess_l->data = malloc(sizeof(fool_session_data_t));

    fool_session_data_t *s_data = sess_l->data;
    server_logic_t *serv_l = sess_l->serv;
    fool_server_data_t *sv_data = serv_l->data;

    s_data->state = ps_waiting;
    s_data->hand = ll_create();

    if (serv_l->sess_cnt >= serv_l->sess_cap) {
        OUTBUF_POSTF(sess_l, "The server is full (%d/%d)!\r\n",
                     serv_l->sess_cap, serv_l->sess_cap);
        
        sess_l->interf->next_room = sv_data->hub_ref;
        return;
    } else if (sv_data->state != gs_awaiting_players) {
        OUTBUF_POST(sess_l, "The game has already started! Try again later\r\n");
        sess_l->interf->next_room = sv_data->hub_ref;
        return;
    }

    OUTBUF_POSTF(sess_l,
            "%sWelcome to the game of FOOL! "
            "Once there is one more player, you can press ENTER to start the game", 
            clrscr);
    serv_l->sess_refs[serv_l->sess_cnt++] = sess_l;

    if (serv_l->sess_cnt == serv_l->sess_cap)
        start_game(serv_l);        
}

static void end_game_with_message(server_logic_t *serv_l, const char *msg);

void fool_deinit_session_logic(session_logic_t *sess_l)
{
    fool_session_data_t *s_data = sess_l->data;
    server_logic_t *serv_l = sess_l->serv;
    fool_server_data_t *sv_data = serv_l->data;

    // If not reset, remove from player array and shift others
    bool offset = false;
    for (int i = 0; i < serv_l->sess_cnt; i++) {
        if (serv_l->sess_refs[i] == sess_l) {
            offset = true;

            if (sv_data->defender_index > i)
                sv_data->defender_index--;
            if (sv_data->attacker_index > i)
                sv_data->attacker_index--;
        } else if (offset) {
            serv_l->sess_refs[i-1] = serv_l->sess_refs[i];
            serv_l->sess_refs[i] = NULL;
        }
    }
    if (offset) 
        serv_l->sess_cnt--;

    // If a live player has disconnected, end the game
    if (
            (sv_data->state == gs_first_card || sv_data->state == gs_free_for_all) &&
            s_data->state != ps_spectating && !sess_l->interf->quit && !sess_l->interf->next_room
       )
    {
        end_game_with_message(serv_l,  
                "\r\nA player has disconnected, thus the game can not continue. Press ENTER to exit\r\n");
    }

    // If last player quit, reset server
    if (serv_l->sess_cnt == 0)
        reset_server_logic(serv_l);

    ll_free(s_data->hand);
    free(s_data);
    sess_l->data = NULL;
}

static int get_player_index(session_logic_t *sess_l, server_logic_t *serv_l);
static void send_updates_to_player(server_logic_t *serv_l, int i);

static void process_attacker_first_card(session_logic_t *sess_l, server_logic_t *serv_l, const char *line);
static void process_defender_first_card(session_logic_t *sess_l, server_logic_t *serv_l, const char *line);
static void process_attacker_in_free_for_all(session_logic_t *sess_l, server_logic_t *serv_l, const char *line);
static void process_defender_in_free_for_all(session_logic_t *sess_l, server_logic_t *serv_l, const char *line);

void fool_process_line(session_logic_t *sess_l, const char *line)
{
    fool_session_data_t *s_data = sess_l->data;
    server_logic_t *serv_l = sess_l->serv;
    fool_server_data_t *sv_data = serv_l->data;

    // If waiting for game to start and somebody pressed ENTER, start
    if (sv_data->state == gs_awaiting_players) {
        if (serv_l->sess_cnt >= MIN_PLAYERS_PER_GAME && strlen(line) == 0)
            start_game(serv_l);
        return;
    } else if (sv_data->state == gs_game_end) { // If game ended, quit on ENTER
        sess_l->interf->next_room = sv_data->hub_ref;
        return;
    }

    if (sess_l->is_in_chat) {
        if (streq(line, "game")) {
            sess_l->is_in_chat = false;
            send_updates_to_player(serv_l, get_player_index(sess_l, serv_l));
        } else if (strlen(line) > 0) {
            if (!chat_try_post_message(serv_l->chat, serv_l, sess_l, line))
                OUTBUF_POST(sess_l, "The message is too long!\r\n");
        }
        return;
    } else if (streq(line, "chat")) {
        sess_l->is_in_chat = true;
        chat_send_updates(serv_l->chat, sess_l, "In-game chat\r\n\r\n");
        return;
    }

    if (sv_data->state == gs_first_card && s_data->state == ps_defending)
        process_defender_first_card(sess_l, serv_l, line);
    else if (sv_data->state == gs_first_card && s_data->state == ps_attacking)
        process_attacker_first_card(sess_l, serv_l, line);
    else if (sv_data->state == gs_free_for_all && s_data->state == ps_defending)
        process_defender_in_free_for_all(sess_l, serv_l, line);
    else if (sv_data->state == gs_free_for_all && s_data->state == ps_attacking)
        process_attacker_in_free_for_all(sess_l, serv_l, line);
}

bool fool_server_is_available(server_logic_t *serv_l)
{
    fool_server_data_t *sv_data = serv_l->data;
    return serv_l->sess_cnt < serv_l->sess_cap && sv_data->state == gs_awaiting_players;
}

static void end_game_with_message(server_logic_t *serv_l, const char *msg)
{
    fool_server_data_t *sv_data = serv_l->data;
    sv_data->state = gs_game_end;

    for (int i = 0; i < serv_l->sess_cnt; i++) {
        session_logic_t *sess_l = serv_l->sess_refs[i]; 
        if (sess_l && msg)
            OUTBUF_POSTF(sess_l, "%s%s", clrscr, msg);
    }
}

static void reset_server_logic(server_logic_t *serv_l)
{
    for (int i = 0; i < serv_l->sess_cap; i++)
        serv_l->sess_refs[i] = NULL;

    serv_l->sess_cnt = 0;

    fool_server_data_t *sv_data = serv_l->data;

    sv_data->num_active_players = 0;

    sv_data->state = gs_awaiting_players;
    sv_data->defender_index = 0;
    sv_data->attacker_index = 0;
    sv_data->attackers_left = 0;
}

static void send_updates_to_all_players(server_logic_t *serv_l);

static void replenish_hands(server_logic_t *serv_l);
static void choose_first_turn(server_logic_t *serv_l);

static fool_session_data_t *data_at_index(server_logic_t *serv_l, int sess_idx);

static void start_game(server_logic_t *serv_l)
{
    fool_server_data_t *sv_data = serv_l->data;

    ASSERT(sv_data->state == gs_awaiting_players);
    ASSERT(serv_l->sess_cnt >= MIN_PLAYERS_PER_GAME);

    generate_deck(&sv_data->deck);
    reset_table(&sv_data->table);

    sv_data->state = gs_first_card;
    sv_data->num_active_players = serv_l->sess_cnt;

    replenish_hands(serv_l);
    choose_first_turn(serv_l);

    send_updates_to_all_players(serv_l);
}

static void replenish_hands(server_logic_t *serv_l)
{
    fool_server_data_t *sv_data = serv_l->data;
    ASSERT(sv_data->state == gs_first_card || sv_data->state == gs_free_for_all);

    int player_idx = sv_data->attacker_index;

    for (int i = 0; i < serv_l->sess_cnt; i++)
    {
        fool_session_data_t *s_data = data_at_index(serv_l, i);

        if (s_data->state == ps_spectating) {
            inc_cycl(&player_idx, serv_l->sess_cnt);
            continue;
        }

        while (s_data->hand->size < BASE_PLAYER_CARDS) {
            card_t *card = pop_card_from_deck(&sv_data->deck);
            if (!card)
                goto loop_brk;

            ll_push_front(s_data->hand, card);
        }

        inc_cycl(&player_idx, serv_l->sess_cnt);
    }

loop_brk:
    return;
}

static void choose_first_turn(server_logic_t *serv_l)
{
    fool_server_data_t *sv_data = serv_l->data;
    ASSERT(sv_data->state == gs_first_card);

    // Choose player with the lowest trump card
    int min_card_val = INT_MAX;
    for (int i = 0; i < serv_l->sess_cnt; i++)
    {
        fool_session_data_t *s_data = data_at_index(serv_l, i);
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

    sv_data->defender_index = prev_cycl(sv_data->attacker_index, serv_l->sess_cnt);

    data_at_index(serv_l, sv_data->defender_index)->state = ps_defending;
    data_at_index(serv_l, sv_data->attacker_index)->state = ps_attacking;
    sv_data->attackers_left = 1;
}

static void respond_to_invalid_command(session_logic_t *sess_l);
static void switch_turn(server_logic_t *serv_l, bool defender_lost);
static void enable_free_for_all(server_logic_t *serv_l);
static list_node_t *try_retrieve_card_from_hand(fool_session_data_t *s_data, const char *line);

static void process_attacker_first_card(session_logic_t *sess_l, server_logic_t *serv_l, const char *line)
{
    fool_session_data_t *s_data = sess_l->data;
    fool_server_data_t *sv_data = serv_l->data;

    ASSERT(sv_data->state == gs_first_card && s_data->state == ps_attacking);

    table_t *table = &sv_data->table;

    if (strlen(line) == 0) // Chosen attacker can not forfeit first round
        respond_to_invalid_command(sess_l);
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(s_data, line);
        if (!card_node)
            respond_to_invalid_command(sess_l);
        else if (attacker_try_play_card(table, card_node->data)) {
            ll_remove(s_data->hand, card_node);
            // Once the first attacker card is placed, it is free for all
            enable_free_for_all(serv_l);

            send_updates_to_all_players(serv_l);
        } else
            respond_to_invalid_command(sess_l);
    }
}

static void process_defender_first_card(session_logic_t *sess_l, server_logic_t *serv_l, const char *line)
{
    fool_session_data_t *s_data = sess_l->data;
    fool_server_data_t *sv_data = serv_l->data;

    ASSERT(sv_data->state == gs_first_card && s_data->state == ps_defending);

    // No actions can be performed by defender before first card
    respond_to_invalid_command(sess_l);
}

static void process_attacker_in_free_for_all(session_logic_t *sess_l, server_logic_t *serv_l, const char *line)
{
    fool_session_data_t *s_data = sess_l->data;
    fool_server_data_t *sv_data = serv_l->data;

    ASSERT(sv_data->state == gs_free_for_all && s_data->state == ps_attacking);

    table_t *table = &sv_data->table;
    linked_list_t *def_hand = data_at_index(serv_l, sv_data->defender_index)->hand;

    if (strlen(line) == 0) {
        s_data->state = ps_waiting;

        if (s_data->can_attack)
            sv_data->attackers_left--;
        if (sv_data->attackers_left == 0 && table_is_beaten(table))
            switch_turn(serv_l, false);

        if (sv_data->state != gs_game_end)
            send_updates_to_all_players(serv_l);
    } else if (table_is_full(table, def_hand))
        respond_to_invalid_command(sess_l);
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
                send_updates_to_all_players(serv_l);
        } else
            respond_to_invalid_command(sess_l);
    }
}

static void process_defender_in_free_for_all(session_logic_t *sess_l, server_logic_t *serv_l, const char *line)
{
    fool_session_data_t *s_data = sess_l->data;
    fool_server_data_t *sv_data = serv_l->data;

    ASSERT(sv_data->state == gs_free_for_all && s_data->state == ps_defending);

    table_t *table = &sv_data->table;
    card_suit_t trump_suit = sv_data->deck.trump.suit;
    linked_list_t *def_hand = data_at_index(serv_l, sv_data->defender_index)->hand;

    if (strlen(line) == 0) {
        // We assume table is not beaten, cause if it is and turn is not over, then attackers > 0 && table is not full
        if (sv_data->attackers_left > 0 && !table_is_full(table, def_hand))
            respond_to_invalid_command(sess_l); // Can't forfeit when waiting for cards from attackers
        else {
            switch_turn(serv_l, true);
            if (sv_data->state != gs_game_end)
                send_updates_to_all_players(serv_l);
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
                switch_turn(serv_l, false);
            else { // Once the defender, attackers get a new chance to throw in
                enable_free_for_all(serv_l);
                if (sv_data->attackers_left == 0 && table_is_beaten(table))
                    switch_turn(serv_l, false);
            }

            if (sv_data->state != gs_game_end)
                send_updates_to_all_players(serv_l);
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


static void send_updates_to_all_players(server_logic_t *serv_l)
{
    for (int i = 0; i < serv_l->sess_cnt; i++)
        send_updates_to_player(serv_l, i);
}

static void send_updates_to_player(server_logic_t *serv_l, int i)
{
    ASSERT(i >= 0 && i < serv_l->sess_cnt);
    session_logic_t *sess_l = serv_l->sess_refs[i];
    
    if (sess_l->is_in_chat)
        return;

    fool_server_data_t *sv_data = serv_l->data;
    fool_session_data_t *s_data = sess_l->data;
    string_builder_t *sb = sb_create();

    // Clear screen
    sb_add_str(sb, clrscr);

    // Room name and list of players
    sb_add_strf(sb, "Room: %s\r\n", serv_l->name);
    sb_add_str(sb, "Players:");

    int num_players = serv_l->sess_cnt;
    int player_idx = i;
    for (dec_cycl(&player_idx, num_players); 
            player_idx != i;
            dec_cycl(&player_idx, num_players))
    {
        sb_add_strf(sb, " %s", serv_l->sess_refs[player_idx]->username);
    }
    sb_add_str(sb, "\r\n\r\n");


    // Player card counts
    size_t chars_used = 0; 
    player_idx = i;
    for (dec_cycl(&player_idx, num_players); 
            player_idx != i;
            dec_cycl(&player_idx, num_players))
    {
        fool_session_data_t *p_data = serv_l->sess_refs[player_idx]->data;
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
    if (s_data->state == ps_attacking)
        sb_add_attacker_prompt(sb, s_data->hand, serv_l);
    else if (s_data->state == ps_defending)
        sb_add_defender_prompt(sb, s_data->hand, serv_l);

    char *full_str = sb_build_string(sb);
    OUTBUF_POST(sess_l, full_str);
    free(full_str);
    sb_free(sb);
}

static int get_player_index(session_logic_t *sess_l, server_logic_t *serv_l)
{
    for (int i = 0; i < serv_l->sess_cnt; i++) {
        if (sess_l == serv_l->sess_refs[i])
            return i;
    }
    return -1;
}

static void respond_to_invalid_command(session_logic_t *sess_l)
{
    string_builder_t *sb = sb_create();
    sb_add_str(sb, "The command is invalid or can not be used now\r\n");

    fool_session_data_t *s_data = sess_l->data;
    if (s_data->state == ps_attacking)
        sb_add_attacker_prompt(sb, s_data->hand, sess_l->serv);
    else if (s_data->state == ps_defending)
        sb_add_defender_prompt(sb, s_data->hand, sess_l->serv);

    char *full_str = sb_build_string(sb);
    OUTBUF_POST(sess_l, full_str);
    free(full_str);
    sb_free(sb);
}

static void sb_add_attacker_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   server_logic_t *serv_l)
{
    fool_server_data_t *sv_data = serv_l->data;

    table_t *t = &sv_data->table;
    list_node_t *node = hand->head;
    linked_list_t *def_hand = data_at_index(serv_l, sv_data->defender_index)->hand;

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
    fool_server_data_t *sv_data = serv_l->data;

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

static void advance_turns(server_logic_t *serv_l, int num_turns);
static void send_win_lose_messages_to_players(server_logic_t *serv_l);
static void send_draw_messages_to_players(server_logic_t *serv_l);
static void log_game_results(server_logic_t *serv_l);

static void switch_turn(server_logic_t *serv_l, bool defender_lost)
{
    fool_server_data_t *sv_data = serv_l->data;

    table_t *t = &sv_data->table;
    linked_list_t *def_hand = data_at_index(serv_l, sv_data->defender_index)->hand;

    if (defender_lost)
        flush_table(t, def_hand);
    else
        reset_table(t);

    replenish_hands(serv_l);

    // Game end check
    for (int i = 0; i < serv_l->sess_cnt; i++) {
        fool_session_data_t *s_data = data_at_index(serv_l, i);
        if (s_data->state != ps_spectating && ll_is_empty(s_data->hand)) {
            s_data->state = ps_spectating;
            sv_data->num_active_players--;
        }
    }

    if (sv_data->num_active_players == 1) {
        send_win_lose_messages_to_players(serv_l);
        log_game_results(serv_l);
        end_game_with_message(serv_l, NULL);
    } else if (sv_data->num_active_players <= 0) {
        send_draw_messages_to_players(serv_l);
        log_game_results(serv_l);
        end_game_with_message(serv_l, NULL);
    } else {
        sv_data->state = gs_first_card;
        advance_turns(serv_l, defender_lost ? 2 : 1);
    }
}

static void enable_free_for_all(server_logic_t *serv_l)
{
    fool_server_data_t *sv_data = serv_l->data;

    sv_data->state = gs_free_for_all;
    sv_data->attackers_left = 0;

    table_t *table = &sv_data->table;
    for (int i = 0; i < serv_l->sess_cnt; i++) {
        fool_session_data_t *s_data = data_at_index(serv_l, i);
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

static void advance_turns(server_logic_t *serv_l, int num_turns)
{
    fool_server_data_t *sv_data = serv_l->data;
    ASSERT(num_turns > 0 && sv_data->num_active_players > 1);

    while (num_turns > 0) {
        dec_cycl(&sv_data->attacker_index, serv_l->sess_cnt);
        fool_session_data_t *a_data = data_at_index(serv_l, sv_data->attacker_index);
        if (a_data->state != ps_spectating)
            num_turns--;
    }

    sv_data->defender_index = prev_cycl(sv_data->attacker_index, serv_l->sess_cnt);
    fool_session_data_t *d_data = data_at_index(serv_l, sv_data->defender_index);
    while (d_data->state == ps_spectating) {
        dec_cycl(&sv_data->defender_index, serv_l->sess_cnt);
        d_data = data_at_index(serv_l, sv_data->defender_index);
    }

    for (int i = 0; i < serv_l->sess_cnt; i++) {
        fool_session_data_t *s_data = data_at_index(serv_l, i);
        if (i == sv_data->attacker_index)
            s_data->state = ps_attacking;
        else if (i == sv_data->defender_index)
            s_data->state = ps_defending;
        else if (s_data->state != ps_spectating)
            s_data->state = ps_waiting;
    }

    sv_data->attackers_left = 1;
}

static void send_win_lose_messages_to_players(server_logic_t *serv_l)
{
    for (int i = 0; i < serv_l->sess_cnt; i++) {
        session_logic_t *sess_l = serv_l->sess_refs[i];
        fool_session_data_t *s_data = sess_l->data;
        if (s_data->state == ps_spectating)
            OUTBUF_POSTF(sess_l, "%sYou've won! Kinda. Press ENTER to exit\r\n", clrscr);
        else
            OUTBUF_POSTF(sess_l, "%sYou're the fool! Oopsy-daisy) Press ENTER to exit\r\n", clrscr);
    }
}

static void send_draw_messages_to_players(server_logic_t *serv_l)
{
    for (int i = 0; i < serv_l->sess_cnt; i++)
        OUTBUF_POSTF(serv_l->sess_refs[i], "%sSeems that nobody is the fool today! What a pity. Press ENTER to exit\r\n", clrscr);
}

static void log_game_results(server_logic_t *serv_l)
{
    fool_server_data_t *sv_data = serv_l->data;
    fprintf(serv_l->logs_file_handle, "FOOL: room %s, players(%d):", 
            serv_l->name, serv_l->sess_cnt);
    for (int i = 0; i < serv_l->sess_cnt; i++) {
        session_logic_t *sess_l = serv_l->sess_refs[i];
        fool_session_data_t *s_data = sess_l->data;
        const char *status = sv_data->num_active_players == 0 ? "draw" :
                             (s_data->state == ps_spectating ? "won" : "lost");
                            
        fprintf(serv_l->logs_file_handle, " %s(%s)", sess_l->username, status);
    }
    fputc('\n', serv_l->logs_file_handle);
    fflush(serv_l->logs_file_handle);
}


static fool_session_data_t *data_at_index(server_logic_t *serv_l, int sess_idx)
{
    ASSERT(sess_idx >= 0 && sess_idx < serv_l->sess_cnt);
    return serv_l->sess_refs[sess_idx]->data;
}
