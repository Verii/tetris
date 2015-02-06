#! /bin/bash

gcc -o test_pack -std=c99 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809 \
	test_pack.c ../src/net/pack.c \
	-I../include/

gcc -o test_tetris -std=c99 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809 \
	test_tetris.c ../src/logs.c \
	-I../src -I../include -lm

gcc -o test_serialize -std=c99 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809 \
	test_serialize.c ../src/tetris.c ../src/net/serialize.c ../src/logs.c \
	-I../include -ljson-c -lm
