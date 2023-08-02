#!/bin/bash

DEFINES=""
CFLAGS="$DEFINES -g -Wall"
LFLAGS=""
LOGIC_MODULE="fool"

gcc $CFLAGS -c utils.c
gcc $CFLAGS -c logic.c
gcc $CFLAGS -c "$LOGIC_MODULE.c"
gcc $CFLAGS server.c utils.o logic.o "$LOGIC_MODULE.o" $LFLAGS -o server
