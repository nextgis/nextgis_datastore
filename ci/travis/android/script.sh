#!/bin/sh

set -e

unset CC
unset CXX
export PATH=$HOME/android-8-toolchain/bin:$PATH

mkdir build
cd build
CC="arm-linux-androideabi-gcc" CXX="arm-linux-androideabi-g++" cmake -DBUILD_SHARED_LIBS=ON -DBUILD_TARGET_PLATFORM=ANDROID -DCMAKE_BUILD_TYPE=Release -DANDROID_STANDALONE_TOOLCHAIN=$HOME/android-toolchain .. # TODO: -DBUILD_TESTING=ON -DBUILD_TESTS=ON
cmake --build . --config release -- -j 4
# TODO: cmake --build . --config release --target test
