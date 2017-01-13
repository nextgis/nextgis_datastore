#!/bin/sh

set -e

wget https://dl.google.com/android/repository/android-ndk-r13b-linux-x86_64.zip
unzip android-ndk-r13b-linux-x86_64.zip
android-ndk-r13b-linux-x86_64/build/tools/make-standalone-toolchain.sh --arch arm --api 21 --install-dir=$HOME/android-toolchain
