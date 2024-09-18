#!/bin/sh

mkdir -p build/testing

echo $PWD

echo
echo "[0] ==== Testing C++ Compatibility ===="
g++ -Wall -pedantic -std=c++11 testing/test_cpp_compat.cpp -o build/testing/test_cpp_compat || echo "-> Failed to compile as C++"
build/testing/test_cpp_compat || echo "-> Failed to run C++ executable"
