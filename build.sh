#!/bin/bash
# Linux / macOS :  ./build.sh   puis   ./build/falling_light
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j && echo "OK -> ./build/falling_light"
