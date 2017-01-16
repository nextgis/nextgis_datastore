#!/bin/sh

set -e

brew update
brew install swig

git clone https://github.com/nextgis-borsch/lib_jsonc.git $HOME/build/nextgis/lib_jsonc
git clone https://github.com/nextgis-borsch/lib_geos.git $HOME/build/nextgis/lib_geos
git clone https://github.com/nextgis-borsch/lib_proj.git $HOME/buildnextgis/lib_proj
git clone https://github.com/nextgis-borsch/lib_jpeg.git $HOME/build/nextgis/lib_jpeg
git clone https://github.com/nextgis-borsch/lib_png.git $HOME/build/nextgis/lib_png
git clone https://github.com/nextgis-borsch/lib_tiff.git $HOME/build/nextgis/lib_tiff
git clone https://github.com/nextgis-borsch/lib_geotiff.git $HOME/build/nextgis/lib_geotiff
git clone https://github.com/nextgis-borsch/lib_openssl.git $HOME/build/nextgis/lib_openssl
git clone https://github.com/nextgis-borsch/lib_curl.git $HOME/build/nextgis/lib_curl
git clone https://github.com/nextgis-borsch/lib_gdal.git $HOME/build/nextgis/lib_gdal

echo "List directory"
ls -lh $HOME/build/nextgis
