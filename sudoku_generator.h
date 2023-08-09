/* TextGameServer/sudoku_generator.h */
#ifndef SUDOKU_GENERATOR_SENTRY
#define SUDOKU_GENERATOR_SENTRY

#include "sudoku_board.h"

#define USE_CACHE_AND_THREAD  true

// Pre-fetch shuffle settings
#define USE_COL_SHUFFLE       true
#define USE_ROW_SHUFFLE       true
#define USE_BLOCK_COL_SHUFFLE true
#define USE_BLOCK_ROW_SHUFFLE true
#define USE_NUMBER_SHUFFLE    true

void sgen_init();
void sgen_get_new_board(sudoku_board_t *dest);

#endif
