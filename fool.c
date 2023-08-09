/* TextGameServer/fool.c */
#include "fool.h"
#include "logic.h"
#include "chat_funcs.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>

#include "fool_data_structures.c"

typedef struct fool_session_data_tag {
    player_state_t state;
    linked_list_t *hand;
    bool can_attack;
} fool_session_data_t;

typedef struct fool_room_data_tag {
    game_state_t state;
    int num_active_players;

    int defender_index;
    int attacker_index;
    int attackers_left;

    deck_t deck;
    table_t table;

    server_room_t *hub_ref;
} fool_room_data_t;

// View
#define CHARS_TO_TRUMP 70

static const char tutorial_text[] = 
    "Welcome to the game of FOOL! "
    "If there is more than 1 player, or the game is ongoing, you can press ENTER to start/continue the game\r\n"
    "Commands\r\n"
    "   <quit>: quit the game, works at any moment\r\n"
    "   <chat>: switch to in-game chat, works only when the game is in progress\r\n"
    "   <game>: switch back from chat to game\r\n"
    "   <tutor>: show this message again\r\n"
    "   any letter: play the card indexed by the letter (if you can play that card)\r\n"
    "   empty line: pass (if rules allow it right now)\r\n";

static void reset_room(server_room_t *s_room);

void fool_init_room(server_room_t *s_room, void *payload)
{
    s_room->sess_cap = MAX_PLAYERS_PER_GAME;
    s_room->sess_refs = malloc(s_room->sess_cap * sizeof(*s_room->sess_refs));
    s_room->data = malloc(sizeof(fool_room_data_t));

    fool_room_data_t *r_data = s_room->data;
    game_payload_t *payload_data = payload;
    r_data->hub_ref = payload_data->hub_ref;

    reset_room(s_room);
}

void fool_deinit_room(server_room_t *s_room)
{
    free(s_room->data);
    free(s_room->sess_refs);
}

static void start_game(server_room_t *s_room);

void fool_init_room_session(room_session_t *r_sess)
{
    r_sess->data = malloc(sizeof(fool_session_data_t));

    fool_session_data_t *rs_data = r_sess->data;
    server_room_t *s_room = r_sess->room;
    fool_room_data_t *r_data = s_room->data;

    rs_data->state = ps_waiting;
    rs_data->hand = ll_create();

    if (s_room->sess_cnt >= s_room->sess_cap) {
        OUTBUF_POSTF(r_sess, "The server is full (%d/%d)!\r\n",
                     s_room->sess_cap, s_room->sess_cap);
        
        r_sess->interf->next_room = r_data->hub_ref;
        return;
    } else if (r_data->state != gs_awaiting_players) {
        OUTBUF_POST(r_sess, "The game has already started! Try again later\r\n");
        r_sess->interf->next_room = r_data->hub_ref;
        return;
    }

    OUTBUF_POSTF(r_sess, "%s%s", tutorial_text, clrscr);
    s_room->sess_refs[s_room->sess_cnt++] = r_sess;

    if (s_room->sess_cnt == s_room->sess_cap)
        start_game(s_room);        
}

static void end_game_with_message(server_room_t *s_room, const char *msg);

void fool_deinit_room_session(room_session_t *r_sess)
{
    fool_session_data_t *rs_data = r_sess->data;
    server_room_t *s_room = r_sess->room;
    fool_room_data_t *r_data = s_room->data;

    // If not reset, remove from player array and shift others
    bool offset = false;
    for (int i = 0; i < s_room->sess_cnt; i++) {
        if (s_room->sess_refs[i] == r_sess) {
            offset = true;

            if (r_data->defender_index > i)
                r_data->defender_index--;
            if (r_data->attacker_index > i)
                r_data->attacker_index--;
        } else if (offset) {
            s_room->sess_refs[i-1] = s_room->sess_refs[i];
            s_room->sess_refs[i] = NULL;
        }
    }
    if (offset) 
        s_room->sess_cnt--;

    // If a live player has disconnected, end the game
    if (
            (r_data->state == gs_first_card || r_data->state == gs_free_for_all) &&
            rs_data->state != ps_spectating
       )
    {
        end_game_with_message(s_room,  
                "\r\nA player has disconnected, thus the game can not continue. Press ENTER to exit\r\n");
    }

    // If last player quit, reset server
    if (s_room->sess_cnt == 0)
        reset_room(s_room);

    ll_free(rs_data->hand);
    free(rs_data);
    r_sess->data = NULL;
}

static int get_player_index(room_session_t *r_sess, server_room_t *s_room);
static void send_updates_to_player(server_room_t *s_room, int i);

static void process_attacker_first_card(room_session_t *r_sess, server_room_t *s_room, const char *line);
static void process_defender_first_card(room_session_t *r_sess, server_room_t *s_room, const char *line);
static void process_attacker_in_free_for_all(room_session_t *r_sess, server_room_t *s_room, const char *line);
static void process_defender_in_free_for_all(room_session_t *r_sess, server_room_t *s_room, const char *line);

void fool_process_line(room_session_t *r_sess, const char *line)
{
    fool_session_data_t *rs_data = r_sess->data;
    server_room_t *s_room = r_sess->room;
    fool_room_data_t *r_data = s_room->data;

    if (streq(line, "quit") || r_data->state == gs_game_end) {
        r_sess->interf->next_room = r_data->hub_ref;
        return;
    }

    // If waiting for game to start and somebody pressed ENTER, start
    if (r_data->state == gs_awaiting_players) {
        if (s_room->sess_cnt >= MIN_PLAYERS_PER_GAME)
            start_game(s_room);
        return;
    }

    /*
    if (r_sess->is_in_tutorial) {
        r_sess->is_in_tutorial = false;
        if (r_sess->is_in_chat)
            chat_send_updates(s_room->chat, r_sess, "In-game chat\r\n\r\n");
        else
            send_updates_to_player(s_room, get_player_index(r_sess, s_room));
        
        return;
    } else if (streq(line, "tutor")) {
        r_sess->is_in_tutorial = true;
        OUTBUF_POSTF(r_sess, "%s%s", tutorial_text, clrscr);
        return;
    }
    */

    if (r_sess->is_in_chat) {
        if (streq(line, "game")) {
            r_sess->is_in_chat = false;
            send_updates_to_player(s_room, get_player_index(r_sess, s_room));
        } else if (strlen(line) > 0) {
            if (!chat_try_post_message(s_room->chat, s_room, r_sess, line))
                OUTBUF_POST(r_sess, "The message is too long!\r\n");
        }
        return;
    } else if (streq(line, "chat")) {
        r_sess->is_in_chat = true;
        chat_send_updates(s_room->chat, r_sess, "In-game chat\r\n\r\n");
        return;
    }

    if (r_data->state == gs_first_card && rs_data->state == ps_defending)
        process_defender_first_card(r_sess, s_room, line);
    else if (r_data->state == gs_first_card && rs_data->state == ps_attacking)
        process_attacker_first_card(r_sess, s_room, line);
    else if (r_data->state == gs_free_for_all && rs_data->state == ps_defending)
        process_defender_in_free_for_all(r_sess, s_room, line);
    else if (r_data->state == gs_free_for_all && rs_data->state == ps_attacking)
        process_attacker_in_free_for_all(r_sess, s_room, line);
}

bool fool_room_is_available(server_room_t *s_room)
{
    fool_room_data_t *r_data = s_room->data;
    return s_room->sess_cnt < s_room->sess_cap && r_data->state == gs_awaiting_players;
}

static void end_game_with_message(server_room_t *s_room, const char *msg)
{
    fool_room_data_t *r_data = s_room->data;
    r_data->state = gs_game_end;

    for (int i = 0; i < s_room->sess_cnt; i++) {
        room_session_t *r_sess = s_room->sess_refs[i]; 
        if (r_sess && msg)
            OUTBUF_POSTF(r_sess, "%s%s", clrscr, msg);
    }
}

static void reset_room(server_room_t *s_room)
{
    for (int i = 0; i < s_room->sess_cap; i++)
        s_room->sess_refs[i] = NULL;

    s_room->sess_cnt = 0;

    fool_room_data_t *r_data = s_room->data;

    r_data->num_active_players = 0;

    r_data->state = gs_awaiting_players;
    r_data->defender_index = 0;
    r_data->attacker_index = 0;
    r_data->attackers_left = 0;
}

static void send_updates_to_all_players(server_room_t *s_room);

static void replenish_hands(server_room_t *s_room);
static void choose_first_turn(server_room_t *s_room);

static fool_session_data_t *data_at_index(server_room_t *s_room, int sess_idx);

static void start_game(server_room_t *s_room)
{
    fool_room_data_t *r_data = s_room->data;

    ASSERT(r_data->state == gs_awaiting_players);
    ASSERT(s_room->sess_cnt >= MIN_PLAYERS_PER_GAME);

    generate_deck(&r_data->deck);
    reset_table(&r_data->table);

    r_data->state = gs_first_card;
    r_data->num_active_players = s_room->sess_cnt;

    replenish_hands(s_room);
    choose_first_turn(s_room);

    send_updates_to_all_players(s_room);
}

static void replenish_hands(server_room_t *s_room)
{
    fool_room_data_t *r_data = s_room->data;
    ASSERT(r_data->state == gs_first_card || r_data->state == gs_free_for_all);

    int player_idx = r_data->attacker_index;

    for (int i = 0; i < s_room->sess_cnt; i++)
    {
        fool_session_data_t *rs_data = data_at_index(s_room, i);

        if (rs_data->state == ps_spectating) {
            inc_cycl(&player_idx, s_room->sess_cnt);
            continue;
        }

        while (rs_data->hand->size < BASE_PLAYER_CARDS) {
            card_t *card = pop_card_from_deck(&r_data->deck);
            if (!card)
                goto loop_brk;

            ll_push_front(rs_data->hand, card);
        }

        inc_cycl(&player_idx, s_room->sess_cnt);
    }

loop_brk:
    return;
}

static void choose_first_turn(server_room_t *s_room)
{
    fool_room_data_t *r_data = s_room->data;
    ASSERT(r_data->state == gs_first_card);

    // Choose player with the lowest trump card
    int min_card_val = INT_MAX;
    for (int i = 0; i < s_room->sess_cnt; i++)
    {
        fool_session_data_t *rs_data = data_at_index(s_room, i);
        list_node_t *card_node = rs_data->hand->head;
        while (card_node) {
            card_t *card = card_node->data;
            if (
                    card->suit == r_data->deck.trump.suit &&
                    card->val < min_card_val
               ) 
            {
                r_data->attacker_index = i;
                min_card_val = card->val;
            }

            card_node = card_node->next;
        }
    }

    r_data->defender_index = prev_cycl(r_data->attacker_index, s_room->sess_cnt);

    data_at_index(s_room, r_data->defender_index)->state = ps_defending;
    data_at_index(s_room, r_data->attacker_index)->state = ps_attacking;
    r_data->attackers_left = 1;
}

static void respond_to_invalid_command(room_session_t *r_sess);
static void switch_turn(server_room_t *s_room, bool defender_lost);
static void enable_free_for_all(server_room_t *s_room);
static list_node_t *try_retrieve_card_from_hand(fool_session_data_t *rs_data, const char *line);

static void process_attacker_first_card(room_session_t *r_sess, server_room_t *s_room, const char *line)
{
    fool_session_data_t *rs_data = r_sess->data;
    fool_room_data_t *r_data = s_room->data;

    ASSERT(r_data->state == gs_first_card && rs_data->state == ps_attacking);

    table_t *table = &r_data->table;

    if (strlen(line) == 0) // Chosen attacker can not forfeit first round
        respond_to_invalid_command(r_sess);
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(rs_data, line);
        if (!card_node)
            respond_to_invalid_command(r_sess);
        else if (attacker_try_play_card(table, card_node->data)) {
            ll_remove(rs_data->hand, card_node);
            // Once the first attacker card is placed, it is free for all
            enable_free_for_all(s_room);

            send_updates_to_all_players(s_room);
        } else
            respond_to_invalid_command(r_sess);
    }
}

static void process_defender_first_card(room_session_t *r_sess, server_room_t *s_room, const char *line)
{
    fool_session_data_t *rs_data = r_sess->data;
    fool_room_data_t *r_data = s_room->data;

    ASSERT(r_data->state == gs_first_card && rs_data->state == ps_defending);

    // No actions can be performed by defender before first card
    respond_to_invalid_command(r_sess);
}

static void process_attacker_in_free_for_all(room_session_t *r_sess, server_room_t *s_room, const char *line)
{
    fool_session_data_t *rs_data = r_sess->data;
    fool_room_data_t *r_data = s_room->data;

    ASSERT(r_data->state == gs_free_for_all && rs_data->state == ps_attacking);

    table_t *table = &r_data->table;
    linked_list_t *def_hand = data_at_index(s_room, r_data->defender_index)->hand;

    if (strlen(line) == 0) {
        rs_data->state = ps_waiting;

        if (rs_data->can_attack)
            r_data->attackers_left--;
        if (r_data->attackers_left == 0 && table_is_beaten(table))
            switch_turn(s_room, false);

        if (r_data->state != gs_game_end)
            send_updates_to_all_players(s_room);
    } else if (table_is_full(table, def_hand))
        respond_to_invalid_command(r_sess);
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(rs_data, line);
        if (!card_node)
            respond_to_invalid_command(r_sess);
        else if (attacker_try_play_card(table, card_node->data)) {
            ll_remove(rs_data->hand, card_node);

            if (!player_can_attack(table, rs_data->hand)) {
                rs_data->can_attack = false;
                r_data->attackers_left--;
            }

            if (r_data->state != gs_game_end)
                send_updates_to_all_players(s_room);
        } else
            respond_to_invalid_command(r_sess);
    }
}

static void process_defender_in_free_for_all(room_session_t *r_sess, server_room_t *s_room, const char *line)
{
    fool_session_data_t *rs_data = r_sess->data;
    fool_room_data_t *r_data = s_room->data;

    ASSERT(r_data->state == gs_free_for_all && rs_data->state == ps_defending);

    table_t *table = &r_data->table;
    card_suit_t trump_suit = r_data->deck.trump.suit;
    linked_list_t *def_hand = data_at_index(s_room, r_data->defender_index)->hand;

    if (strlen(line) == 0) {
        // We assume table is not beaten, cause if it is and turn is not over, then attackers > 0 && table is not full
        if (r_data->attackers_left > 0 && !table_is_full(table, def_hand))
            respond_to_invalid_command(r_sess); // Can't forfeit when waiting for cards from attackers
        else {
            switch_turn(s_room, true);
            if (r_data->state != gs_game_end)
                send_updates_to_all_players(s_room);
        }
    } else if (table_is_beaten(table))
        respond_to_invalid_command(r_sess); // Cant defend at a beaten table
    else {
        list_node_t *card_node = try_retrieve_card_from_hand(rs_data, line);
        if (!card_node)
            respond_to_invalid_command(r_sess);
        else if (defender_try_play_card(table, card_node->data, trump_suit)) {
            ll_remove(rs_data->hand, card_node);

            if (table_is_full(table, def_hand) && table_is_beaten(table))
                switch_turn(s_room, false);
            else { // Once the defender, attackers get a new chance to throw in
                enable_free_for_all(s_room);
                if (r_data->attackers_left == 0 && table_is_beaten(table))
                    switch_turn(s_room, false);
            }

            if (r_data->state != gs_game_end)
                send_updates_to_all_players(s_room);
        } else
            respond_to_invalid_command(r_sess);
    }
}

static void sb_add_attacker_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   server_room_t *s_room);
static void sb_add_defender_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   server_room_t *s_room);
static void sb_add_card(string_builder_t *sb, card_t card);


static void send_updates_to_all_players(server_room_t *s_room)
{
    for (int i = 0; i < s_room->sess_cnt; i++)
        send_updates_to_player(s_room, i);
}

static void send_updates_to_player(server_room_t *s_room, int i)
{
    ASSERT(i >= 0 && i < s_room->sess_cnt);
    room_session_t *r_sess = s_room->sess_refs[i];
    
    if (r_sess->is_in_chat)
        return;

    fool_room_data_t *r_data = s_room->data;
    fool_session_data_t *rs_data = r_sess->data;
    string_builder_t *sb = sb_create();

    // Clear screen
    sb_add_str(sb, clrscr);

    // Room name and list of players
    sb_add_strf(sb, "Room: %s\r\n", s_room->name);
    sb_add_str(sb, "Other players:");

    int num_players = s_room->sess_cnt;
    int player_idx = i;
    for (dec_cycl(&player_idx, num_players); 
            player_idx != i;
            dec_cycl(&player_idx, num_players))
    {
        sb_add_strf(sb, " %s", s_room->sess_refs[player_idx]->username);
    }
    sb_add_str(sb, "\r\n\r\n");


    // Player card counts
    size_t chars_used = 0; 
    player_idx = i;
    for (dec_cycl(&player_idx, num_players); 
            player_idx != i;
            dec_cycl(&player_idx, num_players))
    {
        fool_session_data_t *p_data = s_room->sess_refs[player_idx]->data;
        const char *fmt = player_idx == r_data->defender_index ? "| %d |   " : "< %d >   ";
        chars_used += sb_add_strf(sb, fmt, p_data->hand->size);
    }

    // Deck info: trump & remaining cards
    sb_add_strf(sb, "%*c", CHARS_TO_TRUMP - chars_used, ' ');
    sb_add_card(sb, r_data->deck.trump);
    sb_add_strf(sb, "  [ %d ]\r\n", deck_size(&r_data->deck));

    // Table
    table_t *table = &r_data->table;
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
    linked_list_t *hand = rs_data->hand;
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
    if (rs_data->state == ps_attacking)
        sb_add_attacker_prompt(sb, rs_data->hand, s_room);
    else if (rs_data->state == ps_defending)
        sb_add_defender_prompt(sb, rs_data->hand, s_room);

    OUTBUF_POST_SB(r_sess, sb);
    sb_free(sb);
}

static int get_player_index(room_session_t *r_sess, server_room_t *s_room)
{
    for (int i = 0; i < s_room->sess_cnt; i++) {
        if (r_sess == s_room->sess_refs[i])
            return i;
    }
    return -1;
}

static void respond_to_invalid_command(room_session_t *r_sess)
{
    string_builder_t *sb = sb_create();
    sb_add_str(sb, "The command is invalid or can not be used now\r\n");

    fool_session_data_t *rs_data = r_sess->data;
    if (rs_data->state == ps_attacking)
        sb_add_attacker_prompt(sb, rs_data->hand, r_sess->room);
    else if (rs_data->state == ps_defending)
        sb_add_defender_prompt(sb, rs_data->hand, r_sess->room);

    OUTBUF_POST_SB(r_sess, sb);
    sb_free(sb);
}

static void sb_add_attacker_prompt(string_builder_t *sb,
                                   linked_list_t *hand,
                                   server_room_t *s_room)
{
    fool_room_data_t *r_data = s_room->data;

    table_t *t = &r_data->table;
    list_node_t *node = hand->head;
    linked_list_t *def_hand = data_at_index(s_room, r_data->defender_index)->hand;

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
                                   server_room_t *s_room)
{
    fool_room_data_t *r_data = s_room->data;

    table_t *t = &r_data->table;
    card_suit_t trump_suit = r_data->deck.trump.suit;
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

static void advance_turns(server_room_t *s_room, int num_turns);
static void send_win_lose_messages_to_players(server_room_t *s_room);
static void send_draw_messages_to_players(server_room_t *s_room);
static void log_game_results(server_room_t *s_room);

static void switch_turn(server_room_t *s_room, bool defender_lost)
{
    fool_room_data_t *r_data = s_room->data;

    table_t *t = &r_data->table;
    linked_list_t *def_hand = data_at_index(s_room, r_data->defender_index)->hand;

    if (defender_lost)
        flush_table(t, def_hand);
    else
        reset_table(t);

    replenish_hands(s_room);

    // Game end check
    for (int i = 0; i < s_room->sess_cnt; i++) {
        fool_session_data_t *rs_data = data_at_index(s_room, i);
        if (rs_data->state != ps_spectating && ll_is_empty(rs_data->hand)) {
            rs_data->state = ps_spectating;
            r_data->num_active_players--;
        }
    }

    if (r_data->num_active_players == 1) {
        send_win_lose_messages_to_players(s_room);
        log_game_results(s_room);
        end_game_with_message(s_room, NULL);
    } else if (r_data->num_active_players <= 0) {
        send_draw_messages_to_players(s_room);
        log_game_results(s_room);
        end_game_with_message(s_room, NULL);
    } else {
        r_data->state = gs_first_card;
        advance_turns(s_room, defender_lost ? 2 : 1);
    }
}

static void enable_free_for_all(server_room_t *s_room)
{
    fool_room_data_t *r_data = s_room->data;

    r_data->state = gs_free_for_all;
    r_data->attackers_left = 0;

    table_t *table = &r_data->table;
    for (int i = 0; i < s_room->sess_cnt; i++) {
        fool_session_data_t *rs_data = data_at_index(s_room, i);
        if (rs_data->state == ps_waiting)
            rs_data->state = ps_attacking;

        // Only count those attackers that can do smth
        if (rs_data->state == ps_attacking) {
            if (player_can_attack(table, rs_data->hand)) {
                rs_data->can_attack = true;
                r_data->attackers_left++;
            } else 
                rs_data->can_attack = false;
        } 
    }
}

static list_node_t *try_retrieve_card_from_hand(fool_session_data_t *rs_data, const char *line)
{
    if (strlen(line) != 1)
        return NULL;

    int card_int_index = card_char_index_to_int(*line); 
    if (card_int_index < 0)
        return NULL;

    return ll_find_at(rs_data->hand, card_int_index);
}

static void advance_turns(server_room_t *s_room, int num_turns)
{
    fool_room_data_t *r_data = s_room->data;
    ASSERT(num_turns > 0 && r_data->num_active_players > 1);

    while (num_turns > 0) {
        dec_cycl(&r_data->attacker_index, s_room->sess_cnt);
        fool_session_data_t *a_data = data_at_index(s_room, r_data->attacker_index);
        if (a_data->state != ps_spectating)
            num_turns--;
    }

    r_data->defender_index = prev_cycl(r_data->attacker_index, s_room->sess_cnt);
    fool_session_data_t *d_data = data_at_index(s_room, r_data->defender_index);
    while (d_data->state == ps_spectating) {
        dec_cycl(&r_data->defender_index, s_room->sess_cnt);
        d_data = data_at_index(s_room, r_data->defender_index);
    }

    for (int i = 0; i < s_room->sess_cnt; i++) {
        fool_session_data_t *rs_data = data_at_index(s_room, i);
        if (i == r_data->attacker_index)
            rs_data->state = ps_attacking;
        else if (i == r_data->defender_index)
            rs_data->state = ps_defending;
        else if (rs_data->state != ps_spectating)
            rs_data->state = ps_waiting;
    }

    r_data->attackers_left = 1;
}

static void send_win_lose_messages_to_players(server_room_t *s_room)
{
    for (int i = 0; i < s_room->sess_cnt; i++) {
        room_session_t *r_sess = s_room->sess_refs[i];
        fool_session_data_t *rs_data = r_sess->data;
        if (rs_data->state == ps_spectating)
            OUTBUF_POSTF(r_sess, "%sYou've won! Kinda. Press ENTER to exit\r\n", clrscr);
        else
            OUTBUF_POSTF(r_sess, "%sYou're the fool! Oopsy-daisy) Press ENTER to exit\r\n", clrscr);
    }
}

static void send_draw_messages_to_players(server_room_t *s_room)
{
    for (int i = 0; i < s_room->sess_cnt; i++)
        OUTBUF_POSTF(s_room->sess_refs[i], "%sSeems that nobody is the fool today! What a pity. Press ENTER to exit\r\n", clrscr);
}

static void log_game_results(server_room_t *s_room)
{
    fool_room_data_t *r_data = s_room->data;
    fprintf(s_room->logs_file_handle, "FOOL: room %s, players(%d):", 
            s_room->name, s_room->sess_cnt);
    for (int i = 0; i < s_room->sess_cnt; i++) {
        room_session_t *r_sess = s_room->sess_refs[i];
        fool_session_data_t *rs_data = r_sess->data;
        const char *status = r_data->num_active_players == 0 ? "draw" :
                             (rs_data->state == ps_spectating ? "won" : "lost");
                            
        fprintf(s_room->logs_file_handle, " %s(%s)", r_sess->username, status);
    }
    fputc('\n', s_room->logs_file_handle);
    fflush(s_room->logs_file_handle);
}


static fool_session_data_t *data_at_index(server_room_t *s_room, int sess_idx)
{
    ASSERT(sess_idx >= 0 && sess_idx < s_room->sess_cnt);
    return s_room->sess_refs[sess_idx]->data;
}
