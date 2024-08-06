#!/bin/sh

mkdir -p build
gcc -g -O3 -Wall -pedantic frontend/console/main.c -o build/tex2sdf -lm
