/* TextGameServer/sudoku.c */
#include "sudoku.h"
#include "logic.h"
#include "chat_funcs.h"
#include "utils.h"
#include "sudoku_data_structures.c"

#define MAX_PLAYERS_PER_GAME 8

typedef struct sudoku_session_data_tag {
    player_state_t state;
} sudoku_session_data_t;

typedef struct sudoku_room_data_tag {
    game_state_t state;

    sudoku_board_t board;
    int player_index;

    server_room_t *hub_ref;
} sudoku_room_data_t;

static void reset_room(server_room_t *s_room);

void sudoku_init_room(server_room_t *s_room, void *payload)
{
    s_room->sess_cap = MAX_PLAYERS_PER_GAME;
    s_room->sess_refs = malloc(s_room->sess_cap * sizeof(*s_room->sess_refs));
    s_room->data = malloc(sizeof(sudoku_room_data_t));

    sudoku_room_data_t *r_data = s_room->data;
    game_payload_t *payload_data = payload;
    r_data->hub_ref = payload_data->hub_ref;

    reset_room(s_room);
}

void sudoku_deinit_room(server_room_t *s_room)
{
    free(s_room->data);
    free(s_room->sess_refs);
}

void sudoku_init_room_session(room_session_t *r_sess)
{
    r_sess->data = malloc(sizeof(sudoku_session_data_t));

    sudoku_session_data_t *rs_data = r_sess->data;
    server_room_t *s_room = r_sess->room;
    sudoku_room_data_t *r_data = s_room->data;

    rs_data->state = ps_lobby;

    if (s_room->sess_cnt >= s_room->sess_cap) {
        OUTBUF_POSTF(r_sess, "The server is full (%d/%d)!\r\n",
                     s_room->sess_cap, s_room->sess_cap);
        
        r_sess->interf->next_room = r_data->hub_ref;
        return;
    }

    OUTBUF_POSTF(r_sess,
            "%sWelcome to the game of SUDOKU! "
            "Press ENTER to %s the game\r\n"
            "Commands\r\n"
            "   <quit>: quit the game, works at any moment\r\n"
            "   <chat>: switch to in-game chat, works only when the game is in progress\r\n"
            "   <game>: switch back from chat to game\r\n",
            // @TODO: describe sudoku-specific commands
            clrscr, r_data->state == gs_awaiting_players ? "start" : "join");

    s_room->sess_refs[s_room->sess_cnt++] = r_sess;
}

static void send_updates_to_all_players(server_room_t *s_room);

void sudoku_deinit_room_session(room_session_t *r_sess)
{
    sudoku_session_data_t *rs_data = r_sess->data;
    server_room_t *s_room = r_sess->room;
    sudoku_room_data_t *r_data = s_room->data;

    bool offset = false;
    for (int i = 0; i < s_room->sess_cnt; i++) {
        if (s_room->sess_refs[i] == r_sess) {
            offset = true;

            if (r_data->player_index > i)
                r_data->player_index--;
        } else if (offset) {
            s_room->sess_refs[i-1] = s_room->sess_refs[i];
            s_room->sess_refs[i] = NULL;
        }
    }
    if (offset) 
        s_room->sess_cnt--;

    // If last player quit, reset server
    if (s_room->sess_cnt == 0)
        reset_room(s_room);
    else
        send_updates_to_all_players(s_room);

    free(rs_data);
    r_sess->data = NULL;
}

static void start_game(server_room_t *s_room);
static void send_updates_to_player(server_room_t *s_room, int i);
static int get_player_index(room_session_t *r_sess, server_room_t *s_room);

void sudoku_process_line(room_session_t *r_sess, const char *line)
{
    sudoku_session_data_t *rs_data = r_sess->data;
    server_room_t *s_room = r_sess->room;
    sudoku_room_data_t *r_data = s_room->data;

    if (streq(line, "quit") || r_data->state == gs_game_end) {
        r_sess->interf->next_room = r_data->hub_ref;
        return;
    }

    // If waiting for game to start and somebody pressed ENTER, start
    if (r_data->state == gs_awaiting_players) {
        rs_data->state = ps_acting;
        r_data->player_index = get_player_index(r_sess, s_room);
        start_game(s_room);
        return;
    }

    if (rs_data->state == ps_lobby) {
        rs_data->state = ps_idle;
        send_updates_to_all_players(s_room);
        return;
    }

    if (r_sess->is_in_chat) {
        if (streq(line, "game")) {
            r_sess->is_in_chat = false;
            send_updates_to_player(s_room, get_player_index(r_sess, s_room));
        } else if (strlen(line) > 0) {
            if (!chat_try_post_message(s_room->chat, s_room, r_sess, line))
                OUTBUF_POST(r_sess, "The message is too long!\r\n");
        }
        return;
    } else if (rs_data->state != ps_lobby && streq(line, "chat")) {
        r_sess->is_in_chat = true;
        chat_send_updates(s_room->chat, r_sess, "In-game chat\r\n\r\n");
        return;
    }

    // @TODO: sudoku specific parsing
}

bool sudoku_room_is_available(server_room_t *s_room)
{
    sudoku_room_data_t *r_data = s_room->data;
    return s_room->sess_cnt < s_room->sess_cap && r_data->state != gs_game_end;
}

static void reset_room(server_room_t *s_room)
{
    for (int i = 0; i < s_room->sess_cap; i++)
        s_room->sess_refs[i] = NULL;

    s_room->sess_cnt = 0;

    sudoku_room_data_t *r_data = s_room->data;
    r_data->state = gs_awaiting_players;
    r_data->player_index = 0;
}

static void start_game(server_room_t *s_room)
{
    sudoku_room_data_t *r_data = s_room->data;
    r_data->state = gs_in_progress;
    generate_board(&r_data->board);

    send_updates_to_all_players(s_room);
}

static void send_updates_to_all_players(server_room_t *s_room)
{
    for (int i = 0; i < s_room->sess_cnt; i++)
        send_updates_to_player(s_room, i);
}

static void sb_add_line_sep(string_builder_t *sb, const char *s_init, const char *s_rep);
static void sb_add_line(string_builder_t *sb, sudoku_board_t *board, int y);

// @TODO: add coordinate drawing -- capital letters and digits
static void send_updates_to_player(server_room_t *s_room, int i)
{
    ASSERT(i >= 0 && i < s_room->sess_cnt);
    room_session_t *r_sess = s_room->sess_refs[i];
    sudoku_session_data_t *rs_data = r_sess->data;
    
    if (r_sess->is_in_chat || rs_data->state == ps_lobby)
        return;

    sudoku_room_data_t *r_data = s_room->data;
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

    sb_add_line_sep(sb, "+", "---+");
    for (int y = 0; y < SUDOKU_BOARD_SIZE; y++) {
        sb_add_line(sb, &r_data->board, y);
        sb_add_line_sep(sb, "+", "---+");
    }
    sb_add_str(sb, " > ");

    OUTBUF_POST_SB(r_sess, sb);
    sb_free(sb);
}

static void sb_add_line_sep(string_builder_t *sb, const char *s_init, const char *s_rep)
{
    sb_add_str(sb, s_init);
    for (int x = 0; x < SUDOKU_BOARD_SIZE; x++)
        sb_add_str(sb, s_rep);
    sb_add_str(sb, "\r\n");
}

static void sb_add_line(string_builder_t *sb, sudoku_board_t *board, int y)
{
    sb_add_str(sb, "+");
    for (int x = 0; x < SUDOKU_BOARD_SIZE; x++)
        sb_add_strf(sb, " %d |", (*board)[y][x].val);
    sb_add_str(sb, "\r\n");
}

static int get_player_index(room_session_t *r_sess, server_room_t *s_room)
{
    for (int i = 0; i < s_room->sess_cnt; i++) {
        if (r_sess == s_room->sess_refs[i])
            return i;
    }
    return -1;
}
