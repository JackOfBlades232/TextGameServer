#!/bin/bash

DEFINES=""
CFLAGS="$DEFINES -g -Wall"
LFLAGS=""
LOGIC_MODULE="simple_fool"

gcc $CFLAGS -c linked_list.c
gcc $CFLAGS -c "$LOGIC_MODULE.c"
gcc $CFLAGS server.c linked_list.o "$LOGIC_MODULE.o" $LFLAGS -o server
