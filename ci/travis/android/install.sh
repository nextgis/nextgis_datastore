#!/bin/sh

set -e

sudo apt-get install -qq cmake swig python 
wget -c -N -P $HOME/downloads https://dl.google.com/android/repository/android-ndk-r13b-linux-x86_64.zip
echo "Extract files"
unzip -q $HOME/downloads/android-ndk-r13b-linux-x86_64.zip -d $HOME/android-ndk
echo "Make toolchain"
python $HOME/android-ndk/android-ndk-r13b/build/tools/make_standalone_toolchain.py --arch arm --api 21 --install-dir $HOME/android-toolchain
