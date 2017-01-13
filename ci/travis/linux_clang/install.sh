#!/bin/sh

set -e

echo "Add NextGIS PPA"
sudo add-apt-repository -y ppa:nextgis/ppa
sudo apt-get update -qq
echo "Install dependencies"
sudo apt-get install -qq cmake swig libgdal-dev zlib1g-dev liblzma-dev libgeos-dev libjpeg-dev libproj-dev libexpat1-dev libpng-dev libgeotiff-dev libtiff5-dev libcurl4-openssl-dev libxml2-dev liblzma-dev libsqlite3-dev libjson-c-dev libgles2-mesa-dev
