#!/bin/sh

set -e

sudo add-apt-repository -y ppa:nextgis/ppa
sudo apt-get update -qq
sudo apt-get install cmake swig libgdal-dev zlib1g-dev liblzma-dev libgeos-dev libjpeg-dev libproj-dev libexpat-dev libpng-dev libgeotiff-dev libtiff5-dev libcurl-ssl-dev libxml2-dev liblzma-dev libsqlite3-dev libjson-c-dev
