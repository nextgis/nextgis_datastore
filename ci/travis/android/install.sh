#!/bin/sh

set -e

wget -c -N -P $HOME/downloads https://dl.google.com/android/repository/android-ndk-r13b-linux-x86_64.zip
#pv file.tgz | tar xzf - -C target_directory
echo "Extract files"
unzip -q $HOME/downloads/android-ndk-r13b-linux-x86_64.zip -d $HOME/android-ndk
echo "Make toolchain"
$HOME/android-ndk/android-ndk-r13b/build/tools/make-standalone-toolchain.sh --system=linux-x86_64 --platform=android-8 --install-dir="$HOME/android-toolchain"
ls -lh $HOME
echo "List of $HOME/android-toolchain"
ls -lh $HOME/android-toolchain
