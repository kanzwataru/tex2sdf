#!/bin/sh

mkdir -p build
gcc -g -Og -Wall -pedantic frontend/console/main.c -o build/tex2sdf -lm
