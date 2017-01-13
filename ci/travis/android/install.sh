#!/bin/sh

set -e

wget -c -N -P $HOME/downloads https://dl.google.com/android/repository/android-ndk-r13b-linux-x86_64.zip
echo "Extract files"
unzip -q $HOME/downloads/android-ndk-r13b-linux-x86_64.zip -d $HOME/android-ndk
echo "Make toolchain"
python --version
python $HOME/android-ndk/android-ndk-r13b/build/tools/make_standalone_toolchain.py --arch x86_64 --api 8 --install-dir $HOME/android-toolchain
ls -lh $HOME
echo "List of $HOME/android-toolchain"
ls -lh $HOME/android-toolchain
