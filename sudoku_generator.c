/* TextGameServer/sudoku_generator.h */
#include "sudoku_generator.h"
#include "utils.h"
#if USE_CACHE_AND_THREAD
  #include <pthread.h>
  #include <signal.h>
  #include <unistd.h>
  #include <string.h>
#endif

#define NON_MAIN_ELEM_CNT (NUM_CELLS - BOARD_BLOCKS*BLOCK_SIZE*BLOCK_SIZE)

#define MIN_EMPTY 48
#define MAX_EMPTY 64

#if USE_CACHE_AND_THREAD

  #define BOARD_CACHE_SIZE       16
  #define MAX_SHUFFLES_PER_BOARD 4
  #define MAX_SHUFFLES_PER_GEN   (BOARD_CACHE_SIZE*MAX_SHUFFLES_PER_BOARD)

typedef struct board_cache_tag {
    int quieries_after_last_gen;
    sudoku_board_t main_cache[BOARD_CACHE_SIZE], back_cache[BOARD_CACHE_SIZE];
} board_cache_t;

#endif

typedef struct coord_tag {
    int x, y;
} coord_t;

typedef struct solution_cell_data_tag {
    int options[BOARD_SIZE];
    int num_opts;
} solution_cell_data_t;

typedef solution_cell_data_t solution_board_state_t[BOARD_SIZE][BOARD_SIZE];

#if USE_CACHE_AND_THREAD
static board_cache_t board_cache         = { 0 };

static pthread_t generation_thread       = { 0 };
static bool thread_spawned               = false;
static pthread_mutex_t board_cache_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile static sig_atomic_t caught_usr1 = 0;

static void *sgen_thread_main(void *data);

void usr1_handler(int s)
{
    caught_usr1 = 1;
    signal(SIGUSR1, usr1_handler);
}
#endif

void sgen_init()
{
#if USE_CACHE_AND_THREAD
    if (thread_spawned)
        return;

    thread_spawned = true;
    pthread_create(&generation_thread, NULL, sgen_thread_main, NULL);
    pthread_detach(generation_thread);
#endif
}

static void generate_board(sudoku_board_t *board);
static void copy_board_shuffled(sudoku_board_t *dest, sudoku_board_t *src);

void sgen_get_new_board(sudoku_board_t *dest)
{
#if USE_CACHE_AND_THREAD

    pthread_mutex_lock(&board_cache_mutex);
    {
        int board_idx = board_cache.quieries_after_last_gen % BOARD_CACHE_SIZE;
        copy_board_shuffled(dest, &board_cache.main_cache[board_idx]);
        board_cache.quieries_after_last_gen++;
    }
    pthread_mutex_unlock(&board_cache_mutex);

    if (board_cache.quieries_after_last_gen >= MAX_SHUFFLES_PER_GEN)
        pthread_kill(generation_thread, SIGUSR1);

#else

    sudoku_board_t tmp;
    generate_board(&tmp);
    copy_board_shuffled(dest, &tmp);

#endif
}

#if USE_CACHE_AND_THREAD
static void regenerate_cache();

static void *sgen_thread_main(void *data)
{
    signal(SIGUSR1, usr1_handler);
    regenerate_cache();
     
    // Here we do not try to be strict with correct signal catching
    // (sigprocmask and what not), since we are not required to react to every
    // single signal, and it is ok to skip some if we still regenerate in
    // that approximate time
    for (;;) {
        pause();

        if (caught_usr1) {
            caught_usr1 = 0;
            if (board_cache.quieries_after_last_gen >= MAX_SHUFFLES_PER_GEN)
                regenerate_cache();
        }
    }

    return NULL;
}

static void regenerate_cache()
{
    for (int i = 0; i < BOARD_CACHE_SIZE; i++)
        generate_board(&board_cache.back_cache[i]);

    pthread_mutex_lock(&board_cache_mutex);
    {
        memcpy(board_cache.main_cache,
               board_cache.back_cache,
               sizeof(board_cache.main_cache));

        board_cache.quieries_after_last_gen = 0;
    }
    pthread_mutex_unlock(&board_cache_mutex);
}
#endif

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
            sudoku_cell_t *cell = &(*board)[y][x];

            cell->val = numbers[j];
            cell->is_initial = true;
        }
    }

    // Then, we fill board and remove elements to obtain a unique solution
    fill_board(board);
    remove_board_elements(board);
}

// @BUG: somehow after shuffling I've encountered sudokus with non-unique
// solutions. However, I'm not very interested in fixing this
static void copy_board_shuffled(sudoku_board_t *dest, sudoku_board_t *src)
{
    int rb_shuffle[BOARD_BLOCKS];
    int cb_shuffle[BOARD_BLOCKS];
    int r_shuffle[BOARD_BLOCKS][BLOCK_SIZE];
    int c_shuffle[BOARD_BLOCKS][BLOCK_SIZE];
    int num_shuffle[BOARD_SIZE];

    for (int i = 0; i < BOARD_BLOCKS; i++) {
        rb_shuffle[i] = i;
        cb_shuffle[i] = i;
        for (int j = 0; j < BLOCK_SIZE; j++) {
            r_shuffle[i][j] = j;
            c_shuffle[i][j] = j;
        }
    }
#if USE_BLOCK_ROW_SHUFFLE
    DO_RANDOM_PERMUTATION(int, rb_shuffle, BOARD_BLOCKS);
#endif
#if USE_BLOCK_COL_SHUFFLE
    DO_RANDOM_PERMUTATION(int, cb_shuffle, BOARD_BLOCKS);
#endif
    for (int i = 0; i < BOARD_BLOCKS; i++) {
#if USE_ROW_SHUFFLE
        DO_RANDOM_PERMUTATION(int, r_shuffle[i], BLOCK_SIZE);
#endif
#if USE_COL_SHUFFLE
        DO_RANDOM_PERMUTATION(int, c_shuffle[i], BLOCK_SIZE);
#endif
    }

    for (int i = 0; i < BOARD_SIZE; i++)
        num_shuffle[i] = i+1;
#if USE_NUMBER_SHUFFLE
    DO_RANDOM_PERMUTATION(int, num_shuffle, BOARD_SIZE);
#endif

    bool transpose = randint(0, 1);

    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++) {
            int bx = x/BLOCK_SIZE;
            int lx = x%BLOCK_SIZE;
            int by = y/BLOCK_SIZE;
            int ly = y%BLOCK_SIZE;

            int dest_bx = cb_shuffle[bx];
            int dest_lx = c_shuffle[bx][lx];
            int dest_by = rb_shuffle[by];
            int dest_ly = r_shuffle[by][ly];

            int dest_x = dest_bx*BLOCK_SIZE + dest_lx;
            int dest_y = dest_by*BLOCK_SIZE + dest_ly;
            if (transpose)
                swap_int(&dest_x, &dest_y);

            int old_num = (*src)[y][x].val;
            int new_num = old_num ? num_shuffle[old_num - 1] : 0;

            (*dest)[dest_y][dest_x].val = new_num;
            (*dest)[dest_y][dest_x].is_initial = (*src)[y][x].is_initial;
        }
}

static int add_num_to_cell_data(solution_cell_data_t *cd, int num);
static int remove_num_from_cell_data(solution_cell_data_t *cd, int num);

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
