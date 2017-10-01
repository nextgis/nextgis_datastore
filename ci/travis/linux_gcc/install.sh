#!/bin/sh

set -e

# swig3.0 v3.02 for Ubuntu 12.04
#sudo add-apt-repository -y ppa:teward/swig3.0
# swig3.0 v3.08 for Ubuntu 14.04
sudo add-apt-repository -y ppa:nschloe/swig-backports

echo "Add NextGIS PPA"
sudo add-apt-repository -y ppa:nextgis/dev
sudo apt-get update -qq
echo "Install dependencies"
sudo apt-get install -qq cmake swig3.0 libgdal-dev zlib1g-dev liblzma-dev libgeos-dev libjpeg-dev libproj-dev libexpat1-dev libpng-dev libgeotiff-dev libtiff5-dev libcurl4-openssl-dev libxml2-dev liblzma-dev libsqlite3-dev libjson-c-dev libgles2-mesa-dev libboost-dev libboost-system-dev libboost-thread-dev libcgal-dev
