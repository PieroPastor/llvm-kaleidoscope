#!/bin/bash
cp -r /mnt/c/Users/Dell/Downloads/kaleidoscope ~/
clang++ -g -O3 *.cpp `$LLVM_CONFIG --cxxflags --ldflags --system-libs --libs core orcjit native` -o toy 