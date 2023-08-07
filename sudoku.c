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
}

void sudoku_init_room_session(room_session_t *r_sess)
{
}

void sudoku_deinit_room_session(room_session_t *r_sess)
{
}

void sudoku_process_line(room_session_t *r_sess, const char *line)
{
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
    r_data->player_index = 0;
    generate_board(&r_data->board);
}
