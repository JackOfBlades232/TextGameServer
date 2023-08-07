/* TextGameServer/sudoku_data_structures.c */
#include "utils.h"

#define SUDOKU_BLOCK_SIZE   3
#define SUDOKU_BOARD_BLOCKS 3
#define SUDOKU_BOARD_SIZE   SUDOKU_BLOCK_SIZE*SUDOKU_BOARD_BLOCKS

/*
#define MIN_EMPTY 24
#define MAX_EMPTY 57
*/
#define MIN_EMPTY 3
#define MAX_EMPTY 3

// @NOTE: in this model any player can remove any non-intial cell, which may
//      lead to stalling. I could replace is_initial with author field.

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

// @TEST
static const int board_example[SUDOKU_BOARD_SIZE][SUDOKU_BOARD_SIZE] = {
    { 4, 3, 5, 2, 6, 9, 7, 8, 1 },
    { 6, 8, 2, 5, 7, 1, 4, 9, 3 },
    { 1, 9, 7, 8, 3, 4, 5, 6, 2 },
    { 8, 2, 6, 1, 9, 5, 3, 4, 7 },
    { 3, 7, 4, 6, 8, 2, 9, 1, 5 },
    { 9, 5, 1, 7, 4, 3, 6, 2, 8 },
    { 5, 1, 9, 3, 2, 6, 8, 7, 4 },
    { 2, 4, 8, 9, 5, 7, 1, 3, 6 },
    { 7, 6, 3, 4, 1, 8, 2, 5, 9 }
};

static void generate_board(sudoku_board_t *board)
{
    // @TODO: implement a proper board generation algo

    // @TEST
    for (int y = 0; y < SUDOKU_BOARD_SIZE; y++)
        for (int x = 0; x < SUDOKU_BOARD_SIZE; x++) {
            (*board)[y][x].val = board_example[y][x];
            (*board)[y][x].is_initial = true;
        }

    // @TODO: implement removal with unique solution

    // @TEST
    int k = randint(MIN_EMPTY, MAX_EMPTY);
    for (int i = 0; i < k; i++) {
        int x, y;
        do {
            x = randint(0, SUDOKU_BOARD_SIZE-1);
            y = randint(0, SUDOKU_BOARD_SIZE-1);
        } while ((*board)[y][x].val == 0);
        (*board)[y][x].val = 0;
    }
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
