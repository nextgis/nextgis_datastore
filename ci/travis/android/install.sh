#!/bin/sh

set -e

# By default, we get an older version of libstdc++6 so we need to update it
# http://askubuntu.com/questions/575505/glibcxx-3-4-20-not-found-how-to-fix-this-error
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test

sudo apt-get update -qq
sudo apt-get install -qq libstdc++6

mkdir -p $HOME/downloads
wget -c -N -P $HOME/downloads https://dl.google.com/android/repository/android-ndk-r17c-linux-x86_64.zip
echo "Extract files"
unzip -q $HOME/downloads/android-ndk-r17c-linux-x86_64.zip -d $HOME/android-ndk
echo "Make toolchain"
python $HOME/android-ndk/android-ndk-r17c/build/tools/make_standalone_toolchain.py --arch arm --api 21 --install-dir $HOME/android-toolchain
