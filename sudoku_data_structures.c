/* TextGameServer/sudoku_data_structures.c */
#include "utils.h"
#include "defs.h"
#include <string.h>

#define SUDOKU_BLOCK_SIZE   3
#define SUDOKU_BOARD_BLOCKS 3
#define SUDOKU_BOARD_SIZE   SUDOKU_BLOCK_SIZE*SUDOKU_BOARD_BLOCKS

#define MIN_EMPTY 24
#define MAX_EMPTY 57
/*
#define MIN_EMPTY 3
#define MAX_EMPTY 3
*/

typedef struct sudoku_cell_tag {
    int val;
    bool is_initial;
} sudoku_cell_t;

typedef sudoku_cell_t sudoku_board_t[SUDOKU_BOARD_SIZE][SUDOKU_BOARD_SIZE];

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

static bool try_put_number(sudoku_board_t *board, int number, int x, int y)
{
    ASSERT(number >= 1 && number <= SUDOKU_BOARD_SIZE);
    ASSERT(x >= 0 && y >= 0 && x < SUDOKU_BOARD_SIZE && y < SUDOKU_BOARD_SIZE);
    if ((*board)[y][x].val != 0)
        return false;

    // horiz
    for (int nx = 0; nx < SUDOKU_BOARD_SIZE; nx++) {
        if (nx != x && (*board)[y][nx].val == number)
            return false;
    }
    // vert
    for (int ny = 0; ny < SUDOKU_BOARD_SIZE; ny++) {
        if (ny != y && (*board)[ny][x].val == number)
            return false;
    }

    // block
    int bby = SUDOKU_BLOCK_SIZE*(y/SUDOKU_BLOCK_SIZE);
    int bbx = SUDOKU_BLOCK_SIZE*(x/SUDOKU_BLOCK_SIZE);
    for (int by = bby; by < bby+SUDOKU_BLOCK_SIZE; by++)
        for (int bx = bbx; bx < bbx+SUDOKU_BLOCK_SIZE; bx++) {
            if ((bx != x || by != y) && (*board)[by][bx].val == number)
                return false;
        }

    (*board)[y][x].val = number;
    (*board)[y][x].is_initial = false;
    return true;
}

static bool try_remove_number(sudoku_board_t *board, int x, int y)
{
    ASSERT(x >= 0 && y >= 0 && x < SUDOKU_BOARD_SIZE && y < SUDOKU_BOARD_SIZE);
    if ((*board)[y][x].is_initial || (*board)[y][x].val == 0)
        return false;

    (*board)[y][x].val = 0;
    return true;
}

static void generate_board(sudoku_board_t *board)
{
    memset(*board, 0, sizeof(*board));

    // First, fill all diagonal blocks as an optimization
    int numbers[SUDOKU_BOARD_SIZE];

    for (int i = 0; i < SUDOKU_BOARD_BLOCKS; i++) {
        for (int i = 0; i < SUDOKU_BOARD_SIZE; i++)
            numbers[i] = i+1;
        DO_RANDOM_PERMUTATION(int, numbers, SUDOKU_BOARD_SIZE);

        int bx = SUDOKU_BLOCK_SIZE*i;
        int by = bx;
        for (int j = 0; j < SUDOKU_BOARD_SIZE; j++) {
            int y = by+(j/SUDOKU_BLOCK_SIZE);
            int x = bx+(j%SUDOKU_BLOCK_SIZE);
            sudoku_cell_t *cell = &((*board)[y][x]);

            cell->val = numbers[j];
            cell->is_initial = true;
        }
    }

    // @TODO: implement a proper board generation algo
    // @TODO: implement removal with unique solution
}

static bool board_is_solved(sudoku_board_t *board)
{
    for (int y = 0; y < SUDOKU_BOARD_SIZE; y++)
        for (int x = 0; x < SUDOKU_BOARD_SIZE; x++) {
            if ((*board)[y][x].val == 0)
                return false;
        }

    return true;
}
