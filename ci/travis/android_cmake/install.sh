#!/bin/sh

set -e

sudo apt-get install -qq swig python
mkdir -p $HOME/downloads
wget -c -N -P $HOME/downloads https://dl.google.com/android/repository/android-ndk-r13b-linux-x86_64.zip
wget -c -N -P $HOME/downloads https://dl.google.com/android/repository/cmake-3.6.3155560-linux-x86_64.zip
echo "Extract files"
unzip -q $HOME/downloads/android-ndk-r13b-linux-x86_64.zip -d $HOME/android-ndk
unzip -q $HOME/downloads/cmake-3.6.3155560-linux-x86_64.zip -d $HOME/android-cmake
