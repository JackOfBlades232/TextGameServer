/* TextGameServer/sudoku_data_structures.c */
#include "utils.h"
#include "defs.h"
#include <string.h>

#define BLOCK_SIZE        3
#define BOARD_BLOCKS      3
#define BOARD_SIZE        BLOCK_SIZE*BOARD_BLOCKS

#define NON_MAIN_ELEM_CNT BOARD_SIZE*BOARD_SIZE - BOARD_BLOCKS*BLOCK_SIZE*BLOCK_SIZE

/*
#define MIN_EMPTY 24
#define MAX_EMPTY 57
*/
#define MIN_EMPTY 3
#define MAX_EMPTY 3

// @TODO: test and refac full board generation
// @TODO: implement removal with unique solution

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

    // Then, we fill board
    fill_board(board);

    // @TEST: random removal
    int k = randint(MIN_EMPTY, MAX_EMPTY);
    for (int i = 0; i < k; i++) {
        int x, y;
        do {
            x = randint(0, BOARD_SIZE-1);
            y = randint(0, BOARD_SIZE-1);
        } while ((*board)[y][x].val == 0);
        (*board)[y][x].val = 0;
        (*board)[y][x].is_initial = false;
    }
}

typedef struct coord_tag {
    int x, y;
} coord_t;

typedef struct solution_cell_data_tag {
    int options[BOARD_SIZE];
    int num_opts;
} solution_cell_data_t;

typedef solution_cell_data_t solution_board_state_t[BOARD_SIZE][BOARD_SIZE];

// @TODO: move down
static int remove_num_from_cell_data(solution_cell_data_t *cd, int num)
{
    int i;
    for (i = 0; i < cd->num_opts; i++) {
        if (cd->options[i] == num)
            break;
    }

    if (i == cd->num_opts)
        return cd->num_opts;

    for (i++; i < cd->num_opts; i++)
        cd->options[i-1] = cd->options[i];
    cd->num_opts--;

    return cd->num_opts+1;
}

#define DEBUG_PRINT_BSTATE(_bstate) \
    for (int _y = 0; _y < BOARD_SIZE; _y++) { \
        for (int _x = 0; _x < BOARD_SIZE; _x++) { \
            solution_cell_data_t *_cd = &_bstate[_y][_x]; \
            for (int _i = 0; _i < BOARD_SIZE-BLOCK_SIZE; _i++) { \
                if (_i < _cd->num_opts) \
                    printf("%d", _cd->options[_i]); \
                else \
                    printf(" "); \
            } \
            printf(" "); \
        } \
        printf("\n"); \
    } \

static bool assign_and_eliminate_options(solution_board_state_t *board,
                                         int x, int y, int opt_idx)
{
    solution_cell_data_t *cd = &(*board)[y][x];
    int chosen_num = cd->options[opt_idx];
    int bx = x - x%BLOCK_SIZE;
    int by = y - y%BLOCK_SIZE;

    cd->options[0] = chosen_num;
    cd->num_opts = 1;

    // @HACK: using that BOARD_SIZE = BLOCK_SIZE**2
    for (int c = 0; c < BOARD_SIZE; c++) {
        if (c != x) {
            solution_cell_data_t *other_cd = &(*board)[y][c];
            int prev_num_opts = remove_num_from_cell_data(other_cd, chosen_num);
            if (other_cd->num_opts <= 0)
                return false;
            else if (prev_num_opts > 1 && other_cd->num_opts == 1) {
                bool prop_res = assign_and_eliminate_options(board, c, y, 0);
                if (!prop_res)
                    return false;
            }
        }
        if (c != y) {
            solution_cell_data_t *other_cd = &(*board)[c][x];
            int prev_num_opts = remove_num_from_cell_data(other_cd, chosen_num);
            if (other_cd->num_opts <= 0)
                return false;
            else if (prev_num_opts > 1 && other_cd->num_opts == 1) {
                bool prop_res = assign_and_eliminate_options(board, x, c, 0);
                if (!prop_res)
                    return false;
            }
        }

        int bcy = by + c/BLOCK_SIZE;
        int bcx = bx + c%BLOCK_SIZE;
        if (bcx != x && bcy != y) {
            solution_cell_data_t *other_cd = &(*board)[bcy][bcx];
            int prev_num_opts = remove_num_from_cell_data(other_cd, chosen_num);
            if (other_cd->num_opts <= 0)
                return false;
            else if (prev_num_opts > 1 && other_cd->num_opts == 1) {
                bool prop_res = assign_and_eliminate_options(board, bcx, bcy, 0);
                if (!prop_res)
                return false;
            }
        }
    }

    return true;
}

static bool fill_board(sudoku_board_t *board)
{
    coord_t cell_stack[NON_MAIN_ELEM_CNT] = { 0 };
    // @TODO: extend board state to possible numbers info/chosen index
    solution_board_state_t board_state_stack[NON_MAIN_ELEM_CNT+1] = { 0 };

    int i = 0;
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val)
                continue;

            coord_t *cellc = &cell_stack[i++];
            cellc->x = x;
            cellc->y = y;
        }
    // @SPEED: possible optimization: instead of random shuffle, choose one with
    //      less options (randomly out of equal)
    DO_RANDOM_PERMUTATION(coord_t, cell_stack, NON_MAIN_ELEM_CNT);

    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val) {
                solution_cell_data_t *cd = &board_state_stack[0][y][x];
                cd->options[0] = (*board)[y][x].val;
                cd->num_opts = 1;
            }
        }
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val == 0) {
                solution_cell_data_t *cd = &board_state_stack[0][y][x];
                cd->num_opts = BOARD_SIZE;
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

    int stack_top = 0;
    while (stack_top < NON_MAIN_ELEM_CNT) {
        solution_board_state_t *prev = &board_state_stack[stack_top];
        solution_board_state_t *cur = &board_state_stack[stack_top+1];
        memcpy(*cur, *prev, sizeof(*cur));

        int x = cell_stack[stack_top].x;
        int y = cell_stack[stack_top].y;
        solution_cell_data_t *cd = &(*cur)[y][x];

        int opt_idx = 0;
        bool found_good_choice = false;
        while (opt_idx < cd->num_opts) {
            found_good_choice = assign_and_eliminate_options(cur, x, y, opt_idx);
            if (found_good_choice)
                break;

            opt_idx++;
            memcpy(*cur, *prev, sizeof(*cur));
        }
        
        if (found_good_choice)
            stack_top++;
        else {
            stack_top--;
            int px = cell_stack[stack_top].x;
            int py = cell_stack[stack_top].y;

            solution_cell_data_t *pcd = &board_state_stack[stack_top][py][px];
            remove_num_from_cell_data(pcd, pcd->options[0]);
        }
    }

    /*
    putchar('\n');
    DEBUG_PRINT_BSTATE(board_state_stack[stack_top]);
    */

    solution_board_state_t *fin = &board_state_stack[stack_top];
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            (*board)[y][x].val = (*fin)[y][x].options[0];
            (*board)[y][x].is_initial = true;
        }
    return true;
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
