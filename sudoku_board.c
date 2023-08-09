/* TextGameServer/sudoku_board.c */
#include "sudoku_board.h"
#include "utils.h"
#include "defs.h"
#include <string.h>

#define NON_MAIN_ELEM_CNT (NUM_CELLS - BOARD_BLOCKS*BLOCK_SIZE*BLOCK_SIZE)

#define MIN_EMPTY 48
#define MAX_EMPTY 64

bool board_try_put_number(sudoku_board_t *board, int number, int x, int y)
{
    ASSERT(number >= 1 && number <= BOARD_SIZE);
    ASSERT(x >= 0 && y >= 0 && x < BOARD_SIZE && y < BOARD_SIZE);
    if ((*board)[y][x].val != 0)
        return false;

    // horiz
    for (int nx = 0; nx < BOARD_SIZE; nx++) {
        if (nx != x && (*board)[y][nx].val == number)
            return false;
    }
    // vert
    for (int ny = 0; ny < BOARD_SIZE; ny++) {
        if (ny != y && (*board)[ny][x].val == number)
            return false;
    }

    // block
    int bby = BLOCK_SIZE*(y/BLOCK_SIZE);
    int bbx = BLOCK_SIZE*(x/BLOCK_SIZE);
    for (int by = bby; by < bby+BLOCK_SIZE; by++)
        for (int bx = bbx; bx < bbx+BLOCK_SIZE; bx++) {
            if ((bx != x || by != y) && (*board)[by][bx].val == number)
                return false;
        }

    (*board)[y][x].val = number;
    (*board)[y][x].is_initial = false;
    return true;
}

bool board_try_remove_number(sudoku_board_t *board, int x, int y)
{
    ASSERT(x >= 0 && y >= 0 && x < BOARD_SIZE && y < BOARD_SIZE);
    if ((*board)[y][x].is_initial || (*board)[y][x].val == 0)
        return false;

    (*board)[y][x].val = 0;
    return true;
}

bool board_is_solved(sudoku_board_t *board)
{
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val == 0)
                return false;
        }

    return true;
}
