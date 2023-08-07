#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "sudoku_data_structures.c"

int main()
{
    sudoku_board_t board;

    srand(time(NULL));
    generate_board(&board);

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

    return 0;
}
