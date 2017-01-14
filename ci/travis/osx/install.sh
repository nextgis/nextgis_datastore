#!/bin/sh

set -e

brew update
brew install cmake swig

git clone https://github.com/nextgis-borsch/lib_jsonc.git
git clone https://github.com/nextgis-borsch/lib_geos.git
git clone https://github.com/nextgis-borsch/lib_proj.git
git clone https://github.com/nextgis-borsch/lib_jpeg.git
git clone https://github.com/nextgis-borsch/lib_png.git
git clone https://github.com/nextgis-borsch/lib_tiff.git
git clone https://github.com/nextgis-borsch/lib_geotiff.git
git clone https://github.com/nextgis-borsch/lib_gdal.git
