/* TextGameServer/room_presets.h */
#include "logic.h"
#include "hub.h"
#include "fool.h"
#include "sudoku.h"

static const room_preset_t hub_preset = {
    .name                 = "",

    .init_room_f          = &hub_init_room,
    .deinit_room_f        = &hub_deinit_room,
    .init_sess_f          = &hub_init_room_session,
    .deinit_sess_f        = &hub_deinit_room_session,
    .process_line_f       = &hub_process_line,
    .room_is_available_f  = &hub_is_available
};

static const room_preset_t game_presets[] = {
    {
        .name                 = "fool",

        .init_room_f          = &fool_init_room,
        .deinit_room_f        = &fool_deinit_room,
        .init_sess_f          = &fool_init_room_session,
        .deinit_sess_f        = &fool_deinit_room_session,
        .process_line_f       = &fool_process_line,
        .room_is_available_f  = &fool_room_is_available
    }, 
    {
        .name                 = "sudoku",

        .init_room_f          = &sudoku_init_room,
        .deinit_room_f        = &sudoku_deinit_room,
        .init_sess_f          = &sudoku_init_room_session,
        .deinit_sess_f        = &sudoku_deinit_room_session,
        .process_line_f       = &sudoku_process_line,
        .room_is_available_f  = &sudoku_room_is_available
    }
};
#define NUM_GAMES (sizeof(game_presets)/sizeof(*game_presets))
