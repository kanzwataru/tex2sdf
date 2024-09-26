#!/bin/sh

mkdir -p build
gcc -O2 -Wall -pedantic frontend/console/main.c -o build/tex2sdf -lm
