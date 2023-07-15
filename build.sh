#!/bin/bash

DEFINES=""
CFLAGS="$DEFINES -g -Wall"
LFLAGS=""

gcc $CFLAGS server.c $LFLAGS -o server
