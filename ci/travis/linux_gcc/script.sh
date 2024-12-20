#!/bin/sh

set -e

mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON -DBUILD_TARGET_PLATFORM=DESKTOP -DBUILD_TESTING=ON ..
cmake --build . --config release -- -j 4
# cd test
# ./main_test
# cd ..
# cmake --build . --config release --target test
ctest --output-on-failure
