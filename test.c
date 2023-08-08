#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "sudoku_data_structures.c"

long get_nsec()
{
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long) ts.tv_sec * 1000000000 + (long) ts.tv_nsec;
}

bool sudoku_is_correct(sudoku_board_t *board)
{
    for (int i = 0; i < 81; i++) {
        int x1 = i%9;
        int y1 = i/9;
        for (int j = i+1; j < 81; j++) {
            int x2 = j%9;
            int y2 = j/9;

            if (x1 != x2 && y1 != y2 && (x1/3 != x2/3 || y1/3 != y2/3))
                continue;

            if ((*board)[y1][x1].val == (*board)[y2][x2].val)
                return false;
        }
    }

    return true;
}

int main()
{
    sudoku_board_t board;

    srand(time(NULL));

    for (;;) {
        long prev_time = get_nsec();
        generate_board(&board);

        long cur_time = get_nsec();
        printf("%lf elapsed\n", ((double) cur_time - prev_time) * 1e-9);
        prev_time = cur_time;

        if (!sudoku_is_correct(&board)) {
            printf("Invalid sudoku!\n");
            exit(1);
        }

        getchar();
    }

    /*
    for (int y = 0; y < 9; y++) {
        for (int x = 0; x < 9; x++)
            printf("%d ", board[y][x].val);
        putchar('\n');
    }

    for (;;) {
        int x, y, number;
        if (scanf("%d %d %d", &number, &x, &y) != 3)
            exit(1);
        if (number == -1) {
            if (try_remove_number(&board, x, y))
            {
                for (int i = 0; i < 40; i++)
                    putchar('\n');
                for (int y = 0; y < 9; y++) {
                    for (int x = 0; x < 9; x++)
                        printf("%d ", board[y][x].val);
                    putchar('\n');
                }
            } else
                printf("Can't remove this!\n");
        } else if (try_put_number(&board, number, x, y))
        {
            for (int i = 0; i < 40; i++)
                putchar('\n');
            for (int y = 0; y < 9; y++) {
                for (int x = 0; x < 9; x++)
                    printf("%d ", board[y][x].val);
                putchar('\n');
            }
        } else
            printf("Can't put this!\n");

        if (board_is_solved(&board)) {
            printf("Solved!\n");
            break;
        }
    }
    */

    return 0;
}
