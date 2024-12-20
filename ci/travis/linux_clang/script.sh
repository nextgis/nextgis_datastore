#!/bin/sh

set -e

mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON -DBUILD_TARGET_PLATFORM=DESKTOP -DBUILD_TESTING=ON ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target test
