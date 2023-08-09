/* TextGameServer/sudoku_data_structures.c */
#include "utils.h"
#include "defs.h"
#include <string.h>

#define BLOCK_SIZE        3
#define BOARD_BLOCKS      3
#define BOARD_SIZE        BLOCK_SIZE*BOARD_BLOCKS
#define NUM_CELLS         BOARD_SIZE*BOARD_SIZE

#define NON_MAIN_ELEM_CNT NUM_CELLS - BOARD_BLOCKS*BLOCK_SIZE*BLOCK_SIZE

#define MIN_EMPTY 48
#define MAX_EMPTY 64

// @TODO: implement random row-col-block and number permutation

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

static bool board_is_solved(sudoku_board_t *board)
{
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val == 0)
                return false;
        }

    return true;
}

static void fill_board(sudoku_board_t *board);
static void remove_board_elements(sudoku_board_t *board);

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

    // Then, we fill board and remove elements to obtain a unique solution
    fill_board(board);
    remove_board_elements(board);
}

typedef struct coord_tag {
    int x, y;
} coord_t;

typedef struct solution_cell_data_tag {
    int options[BOARD_SIZE];
    int num_opts;
} solution_cell_data_t;

typedef solution_cell_data_t solution_board_state_t[BOARD_SIZE][BOARD_SIZE];

static int add_num_to_cell_data(solution_cell_data_t *cd, int num);
static int remove_num_from_cell_data(solution_cell_data_t *cd, int num);

#define DEBUG_PRINT_BOARD(_board) \
    for (int _y = 0; _y < BOARD_SIZE; _y++) { \
        for (int _x = 0; _x < BOARD_SIZE; _x++) { \
            printf("%d%c ", _board[_y][_x].val, _board[_y][_x].is_initial ? ' ' : '\''); \
        } \
        printf("\n"); \
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
    }

static bool assign_and_eliminate_options(solution_board_state_t *board,
                                         int x, int y, int opt_idx);

// Non-recursive backtracking algorithm that finds some solution of given
// board (with 3 diagonal blocks already filled). This algorithm randomizes
// the cell order and the possible variants in each cell in order to produce
// different boards every time
static void fill_board(sudoku_board_t *board)
{
    coord_t cell_stack[NON_MAIN_ELEM_CNT] = { 0 };
    solution_board_state_t board_state_stack[NON_MAIN_ELEM_CNT+1] = { 0 };

    // Create stack for cells to try solutions for (in random order)
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

    // Fill initial state: set all chosen numbers to have one option (itself)
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            if ((*board)[y][x].val) {
                solution_cell_data_t *cd = &board_state_stack[0][y][x];
                cd->options[0] = (*board)[y][x].val;
                cd->num_opts = 1;
            }
        }
    // Fill initial state: calculate possible numbers for every other cell
    // (and write them in random order, since the algo will be checking them
    // in linear order)
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

    // The backtracking algo:
    // 1. Take the next cell off the stack, and prepare the next board state
    //      by copying the previous one
    // 2. Try to assign numbers to the cell from it's list of possible numbers.
    //      When assigning, try to modify the possible number lists of other
    //      cells. If one of them becomes empty, the assignment fails. If some
    //      of them become singular, we propagate the assignment for them.
    // 3. If successfully assigned a value, inc the stack to process next number
    // 4. Otherwise (if number list became empty), roll back the stack by one,
    //      and try the next number for the previous cell (thus it is a dfs)
    int stack_top = 0;
    while (stack_top < NON_MAIN_ELEM_CNT) {
        solution_board_state_t *prev_sol = &board_state_stack[stack_top];
        solution_board_state_t *cur_sol = &board_state_stack[stack_top+1];
        memcpy(*cur_sol, *prev_sol, sizeof(*cur_sol));

        int x = cell_stack[stack_top].x;
        int y = cell_stack[stack_top].y;
        solution_cell_data_t *cd = &(*cur_sol)[y][x];

        int opt_idx = 0;
        bool found_good_choice = false;
        while (opt_idx < cd->num_opts) {
            found_good_choice = assign_and_eliminate_options(cur_sol, x, y, opt_idx);
            if (found_good_choice)
                break;

            opt_idx++;
            memcpy(*cur_sol, *prev_sol, sizeof(*cur_sol));
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

    // Finally, copy the final solution to the board
    solution_board_state_t *fin_sol = &board_state_stack[stack_top];
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            (*board)[y][x].val = (*fin_sol)[y][x].options[0];
            (*board)[y][x].is_initial = true;
        }

    putchar('\n');
    putchar('\n');
    DEBUG_PRINT_BSTATE(board_state_stack[stack_top]);
}

static inline bool try_remove_num_and_propagate_elim(
        solution_board_state_t *bs, int x, int y, int number)
{
    solution_cell_data_t *cd = &(*bs)[y][x];
    int prev_num_opts = remove_num_from_cell_data(cd, number);
    if (cd->num_opts <= 0)
        return false;
    if (prev_num_opts > 1 && cd->num_opts == 1)
        return assign_and_eliminate_options(bs, x, y, 0);
    return true;
}

static bool assign_and_eliminate_options(solution_board_state_t *bs,
                                         int x, int y, int opt_idx)
{
    solution_cell_data_t *cd = &(*bs)[y][x];
    int chosen_num = cd->options[opt_idx];
    int bx = x - x%BLOCK_SIZE;
    int by = y - y%BLOCK_SIZE;

    cd->options[0] = chosen_num;
    cd->num_opts = 1;

    // @HACK: using that BOARD_SIZE = BLOCK_SIZE**2, thus regarding c to be
    //  the row index, col index and in-block index at the same time
    for (int c = 0; c < BOARD_SIZE; c++) {
        if (c != x) {
            if (!try_remove_num_and_propagate_elim(bs, c, y, chosen_num))
                return false;
        }
        if (c != y) {
            if (!try_remove_num_and_propagate_elim(bs, x, c, chosen_num))
                return false;
        }

        int bcy = by + c/BLOCK_SIZE;
        int bcx = bx + c%BLOCK_SIZE;
        if (bcx != x && bcy != y) {
            if (!try_remove_num_and_propagate_elim(bs, bcx, bcy, chosen_num))
                return false;
        }
    }

    return true;
}

// After generating full board, just try and remove elements while keeping
// the solution unique
static void remove_board_elements(sudoku_board_t *board)
{
    int removed_elements_threshold = randint(MIN_EMPTY, MAX_EMPTY);
    coord_t cells[NUM_CELLS] = { 0 };

    // Init the order of cells that we try to clear
    int idx = 0;
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            coord_t *cellc = &cells[idx++];
            cellc->x = x;
            cellc->y = y;
        }
    DO_RANDOM_PERMUTATION(coord_t, cells, NUM_CELLS);

    // We use 2 "frames" frames of the board state: if we fail to remove
    // a number, we roll the cur state back to the prev valid "frame"
    solution_board_state_t prev_sol = { 0 },
                           cur_sol  = { 0 };

    // Fill init frame with the given full board
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            prev_sol[y][x].options[0] = (*board)[y][x].val;
            prev_sol[y][x].num_opts = 1;
        }
    memcpy(cur_sol, prev_sol, sizeof(cur_sol));

    // The removal algo:
    // 1. Take the next cell to process. If it has multiple candidates, 
    //      do not remove and skip
    // 2. Init the new state frame from prev, and start trying to remove.
    //      For this, we take the number we want to remove and collect all
    //      the rows, columns and blocks that contain other copies of 
    //      it into a bitset.
    //      (bits 0..8 for col inclusions, 9..17 -- row, 18..26 - blocks)
    // 3. Loop over all the other cells in row/col/block of the removed one,
    //      adding the number to the cell's possible numbers list, if another
    //      copy of it is not in it's row/col/block (checking with the bitset).
    // 4. Repeat until processed all cells or reached the empty elem threshold
    int removed_elements = 0;
    for (int i = 0; i < NUM_CELLS; i++) {
        int x = cells[i].x;
        int y = cells[i].y;

        if (prev_sol[y][x].num_opts > 1)
            continue;

        int num = (*board)[y][x].val;

        unsigned long other_sectors_containing_num = 0;
        for (int oy = 0; oy < BOARD_SIZE; oy++)
            for (int ox = 0; ox < BOARD_SIZE; ox++) {
                if (ox == x && oy == y)
                    continue;

                if ((*board)[oy][ox].val == num) {
                    int block_idx = BOARD_BLOCKS*oy/BLOCK_SIZE + ox/BLOCK_SIZE;
                    other_sectors_containing_num |=
                        (1 << ox) | (1 << (oy + BOARD_SIZE)) | (1 << (block_idx + 2*BOARD_SIZE));
                }
            }

        for (int oy = 0; oy < BOARD_SIZE; oy++)
            for (int ox = 0; ox < BOARD_SIZE; ox++) {
                if (ox == x && oy == y)
                    continue;

                solution_cell_data_t *cd = &cur_sol[oy][ox];
                int block_idx = BOARD_BLOCKS*oy/BLOCK_SIZE + ox/BLOCK_SIZE;
                if (
                        !(other_sectors_containing_num & (1 << ox)) &&
                        !(other_sectors_containing_num & (1 << (oy + BLOCK_SIZE))) &&
                        !(other_sectors_containing_num & (1 << (block_idx + 2*BLOCK_SIZE)))
                   )
                {
                    add_num_to_cell_data(cd, num);
                    if (!(*board)[oy][ox].val && cd->num_opts > 1) {
                        memcpy(cur_sol, prev_sol, sizeof(cur_sol));
                        goto loop_end;
                    }
                }
            }

        (*board)[y][x].val = 0; 
        (*board)[y][x].is_initial = false; 
        memcpy(prev_sol, cur_sol, sizeof(prev_sol));

        removed_elements++;
        if (removed_elements >= removed_elements_threshold)
            break;

loop_end:
        NOOP;
    }

    putchar('\n');
    DEBUG_PRINT_BSTATE(cur_sol);
    putchar('\n');
    DEBUG_PRINT_BOARD((*board));
}

static int add_num_to_cell_data(solution_cell_data_t *cd, int num)
{
    int i;
    for (i = 0; i < cd->num_opts; i++) {
        if (cd->options[i] == num)
            return cd->num_opts;
    }

    ASSERT(i < BOARD_SIZE);

    cd->options[cd->num_opts++] = num;
    return cd->num_opts-1;
}

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
