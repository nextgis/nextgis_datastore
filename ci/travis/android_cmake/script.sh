#!/bin/sh

set -e

#unset CC
#unset CXX
export PATH=$HOME/android-cmake/bin:$PATH

mkdir -p build
cd build

# TODO: -DBUILD_TESTING=ON -DBUILD_TESTS=ON CC="arm-linux-androideabi-gcc" CXX="arm-linux-androideabi-g++" -DANDROID_ABI=x86_64
cmake .. \
 -DANDROID_NDK=$HOME/android-ndk/android-ndk-r17c \
 -DCMAKE_TOOLCHAIN_FILE=$HOME/android-ndk/android-ndk-r13b/build/cmake/android.toolchain.cmake \
 -DANDROID_ABI=armeabi-v7a \
 -DANDROID_NATIVE_API_LEVEL=9 \
 -DANDROID_TOOLCHAIN=clang \
 -DANDROID_STL=c++_static \
 "-DANDROID_CPP_FEATURES=rtti exceptions" \
 "-GAndroid Gradle - Unix Makefiles" \
 -DCMAKE_MAKE_PROGRAM=make \
 -DCMAKE_BUILD_TYPE=Release \
 -DBUILD_SHARED_LIBS=ON \
\
 -DBUILD_TARGET_PLATFORM=ANDROID \
 -DANDROID_STUDIO_CMAKE=ON \

cmake --build . -- -j 4
# TODO: cmake --build . --config release --target test
