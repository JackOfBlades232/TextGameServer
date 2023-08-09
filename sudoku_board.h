/* TextGameServer/sudoku_board.c */
#ifndef SUDOKU_BOARD_SENTRY
#define SUDOKU_BOARD_SENTRY

#include "defs.h"
#include <pthread.h>

#define BLOCK_SIZE        3
#define BOARD_BLOCKS      3
#define BOARD_SIZE        (BLOCK_SIZE*BOARD_BLOCKS)
#define NUM_CELLS         (BOARD_SIZE*BOARD_SIZE)

typedef struct sudoku_cell_tag {
    int val;
    bool is_initial;
} sudoku_cell_t;

typedef sudoku_cell_t sudoku_board_t[BOARD_SIZE][BOARD_SIZE];

typedef enum player_state_tag {
    ps_lobby,
    ps_idle,
    ps_acting
} player_state_t;

typedef enum game_state_tag {
    gs_awaiting_players,
    gs_in_progress,
    gs_game_end     
} game_state_t;

bool board_try_put_number(sudoku_board_t *board, int number, int x, int y);
bool board_try_remove_number(sudoku_board_t *board, int x, int y);
bool board_is_solved(sudoku_board_t *board);

#endif
