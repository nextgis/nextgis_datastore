#!/bin/sh

set -e

# By default, we get an older version of libstdc++6 so we need to update it
# http://askubuntu.com/questions/575505/glibcxx-3-4-20-not-found-how-to-fix-this-error
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test

# swig3.0 v3.02 for Ubuntu 12.04
#sudo add-apt-repository -y ppa:teward/swig3.0
# swig3.0 v3.08 for Ubuntu 14.04
sudo add-apt-repository -y ppa:nschloe/swig-backports

sudo apt-get update -qq
sudo apt-get install -qq libstdc++6 swig3.0

mkdir -p $HOME/downloads
wget -c -N -P $HOME/downloads https://dl.google.com/android/repository/android-ndk-r13b-linux-x86_64.zip
echo "Extract files"
unzip -q $HOME/downloads/android-ndk-r13b-linux-x86_64.zip -d $HOME/android-ndk
echo "Make toolchain"
python $HOME/android-ndk/android-ndk-r13b/build/tools/make_standalone_toolchain.py --arch arm --api 21 --install-dir $HOME/android-toolchain
