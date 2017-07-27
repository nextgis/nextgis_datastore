#!/bin/sh

set -e

mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON -DBUILD_TARGET_PLATFORM=Desktop -DBUILD_TESTING=ON -DBUILD_TESTS=ON ..
cmake --build . --clean-first --config release -- -j 4
echo "execute ./test/main_test"
./test/main_test
cmake --build . --config release --target test
