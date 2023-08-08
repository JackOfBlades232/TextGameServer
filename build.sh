#!/bin/bash

DEFINES=""
CFLAGS="$DEFINES -g -Wall"
LFLAGS=""

gcc $CFLAGS -c utils.c
gcc $CFLAGS -c logic.c
gcc $CFLAGS -c hub.c
gcc $CFLAGS -c fool.c
gcc $CFLAGS -c sudoku.c
gcc $CFLAGS -c chat.c
gcc $CFLAGS server.c utils.o logic.o hub.o fool.o sudoku.o chat.o $LFLAGS -o server

gcc $CFLAGS test.c -o test
