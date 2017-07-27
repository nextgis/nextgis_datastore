#!/bin/sh

set -e

mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON -DBUILD_TARGET_PLATFORM=Desktop -DBUILD_TESTING=ON -DBUILD_TESTS=ON ..
cmake --build . --config release -- -j 4
ls
ls ./test
./test/main_test
ls ./test/data
cmake --build . --config release --target test
