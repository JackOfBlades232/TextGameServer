/* TextGameServer/sudoku_data_structures.c */
#include "utils.h"
#include "defs.h"
#include <string.h>

#define BLOCK_SIZE        3
#define BOARD_BLOCKS      3
#define BOARD_SIZE        BLOCK_SIZE*BOARD_BLOCKS

#define NON_MAIN_ELEM_CNT BOARD_SIZE*BOARD_SIZE - BOARD_BLOCKS*BLOCK_SIZE*BLOCK_SIZE

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

static bool try_put_number(sudoku_board_t *board, int number, int x, int y)
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

static bool try_remove_number(sudoku_board_t *board, int x, int y)
{
    ASSERT(x >= 0 && y >= 0 && x < BOARD_SIZE && y < BOARD_SIZE);
    if ((*board)[y][x].is_initial || (*board)[y][x].val == 0)
        return false;

    (*board)[y][x].val = 0;
    return true;
}

static bool fill_board(sudoku_board_t *board);

static void generate_board(sudoku_board_t *board)
{
    memset(*board, 0, sizeof(*board));

    // First, fill all diagonal blocks as an optimization
    int numbers[BOARD_SIZE];

    for (int i = 0; i < BOARD_SIZE; i++)
        numbers[i] = i+1;
    for (int i = 0; i < BOARD_BLOCKS; i++) {
        DO_RANDOM_PERMUTATION(int, numbers, BOARD_SIZE);

        int bx = BLOCK_SIZE*i;
        int by = bx;
        for (int j = 0; j < BOARD_SIZE; j++) {
            int y = by+(j/BLOCK_SIZE);
            int x = bx+(j%BLOCK_SIZE);
            sudoku_cell_t *cell = &((*board)[y][x]);

            cell->val = numbers[j];
            cell->is_initial = true;
        }
    }

    fill_board(board);

    // @TODO: implement a proper board generation algo
    // @TODO: implement removal with unique solution
}

typedef struct coord_tag {
    int x, y;
} coord_t;

typedef struct solution_cell_data_tag {
    sudoku_cell_t cell;
    int options[BOARD_SIZE];
    int num_opts, opt_idx;
} solution_cell_data_t;

typedef solution_cell_data_t solution_board_state_t[BOARD_SIZE][BOARD_SIZE];

// @TODO: move down
static void remove_num_from_cell_data(solution_cell_data_t *cd, int num)
{
    int i;
    for (i = 0; i < cd->num_opts; i++) {
        if (cd->options[i] == num)
            break;
    }

    if (i == cd->num_opts)
        return;

    if (cd->opt_idx > i)
        cd->opt_idx--;

    for (i++; i < cd->num_opts; i++)
        cd->options[i-1] = cd->options[i];
    cd->num_opts--;
}

#define DEBUG_PRINT_BSTATE(_bstate) \
    for (int _y = 0; _y < BOARD_SIZE; _y++) { \
        for (int _x = 0; _x < BOARD_SIZE; _x++) { \
            solution_cell_data_t *_cd = &_bstate[_y][_x]; \
            for (int _i = 0; _i < BOARD_SIZE; _i++) { \
                if (_i < _cd->num_opts) \
                    printf("%d", _cd->options[_i]); \
                else \
                    printf(" "); \
            } \
            printf(" "); \
        } \
        printf("\n"); \
    } \

static bool fill_board(sudoku_board_t *board)
{
    coord_t cell_stack[NON_MAIN_ELEM_CNT] = { 0 };
    // @TODO: extend board state to possible numbers info/chosen index
    solution_board_state_t board_state_stack[NON_MAIN_ELEM_CNT] = { 0 };

    int i = 0;
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val)
                continue;

            coord_t *cellc = &cell_stack[i++];
            cellc->x = x;
            cellc->y = y;
        }
    DO_RANDOM_PERMUTATION(coord_t, cell_stack, NON_MAIN_ELEM_CNT);

    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val) {
                solution_cell_data_t *cd = &board_state_stack[0][y][x];
                cd->cell = (*board)[y][x];
                cd->options[0] = cd->cell.val;
                cd->num_opts = 1;
                cd->opt_idx = 0;
            }
        }
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val == 0) {
                solution_cell_data_t *cd = &board_state_stack[0][y][x];
                cd->num_opts = BOARD_SIZE;
                cd->opt_idx = 0;
                for (int i = 0; i < cd->num_opts; i++)
                    cd->options[i] = i+1;
                DO_RANDOM_PERMUTATION(int, cd->options, cd->num_opts);

                for (int ox = 0; ox < BOARD_SIZE; ox++) {
                    int num = (*board)[y][ox].val;
                    if (num) 
                        remove_num_from_cell_data(cd, num);
                }
                for (int oy = 0; oy < BOARD_SIZE; oy++) {
                    int num = (*board)[oy][x].val;
                    if (num) 
                        remove_num_from_cell_data(cd, num);
                }

            }
        }

    DEBUG_PRINT_BSTATE(board_state_stack[0]);

    int stack_top = 0;
    return false;
}

static bool board_is_solved(sudoku_board_t *board)
{
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val == 0)
                return false;
        }

    return true;
}
