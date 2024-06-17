#!/bin/sh

mkdir -p build
gcc -g -Og frontend/gui/main.c -o build/tex2sdf_gui -lSDL2 -lm
gcc -g -Og frontend/cui/main.c -o build/tex2sdf -lm
