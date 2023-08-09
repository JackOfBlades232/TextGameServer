#!/bin/bash

DEFINES=""
CFLAGS="$DEFINES -g -Wall"
LFLAGS="-lpthread"

gcc $CFLAGS -c utils.c
gcc $CFLAGS -c logic.c
gcc $CFLAGS -c hub.c
gcc $CFLAGS -c fool.c
gcc $CFLAGS -c sudoku.c
gcc $CFLAGS -c sudoku_board.c
gcc $CFLAGS -c sudoku_generator.c
gcc $CFLAGS -c chat.c
gcc $CFLAGS server.c utils.o logic.o hub.o fool.o sudoku.o sudoku_board.o sudoku_generator.o chat.o $LFLAGS -o server

gcc $CFLAGS test.c sudoku_board.o sudoku_generator.o -o test
