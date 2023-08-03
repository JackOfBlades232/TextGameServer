#!/bin/bash

DEFINES=""
CFLAGS="$DEFINES -g -Wall"
LFLAGS=""

gcc $CFLAGS -c utils.c
gcc $CFLAGS -c logic.c
gcc $CFLAGS -c hub.c
gcc $CFLAGS -c fool.c
gcc $CFLAGS server.c utils.o logic.o hub.o fool.o $LFLAGS -o server
