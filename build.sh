#!/bin/sh

mkdir -p build
gcc -g -Og gui/main.c -o build/tex2sdf_gui -lSDL2 -lm

