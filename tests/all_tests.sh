#! /bin/bash

gcc -o test_pack.bin -std=c99 test_pack.c ../src/pack.c -I../include/
./test_pack.bin

## Do something to make sure it passed ..

rm -f *.bin
