@echo off

mkdir build
pushd build

cl ..\frontend\console\main.c /O2 /Fetex2sdf.exe

popd
