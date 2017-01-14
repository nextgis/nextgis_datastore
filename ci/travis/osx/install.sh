#!/bin/sh

set -e

brew update
brew install swig

git clone https://github.com/nextgis-borsch/lib_jsonc.git $HOME/nextgis/lib_jsonc
git clone https://github.com/nextgis-borsch/lib_geos.git $HOME/nextgis/lib_geos
git clone https://github.com/nextgis-borsch/lib_proj.git $HOME/nextgis/lib_proj
git clone https://github.com/nextgis-borsch/lib_jpeg.git $HOME/nextgis/lib_jpeg
git clone https://github.com/nextgis-borsch/lib_png.git $HOME/nextgis/lib_png
git clone https://github.com/nextgis-borsch/lib_tiff.git $HOME/nextgis/lib_tiff
git clone https://github.com/nextgis-borsch/lib_geotiff.git $HOME/nextgis/lib_geotiff
git clone https://github.com/nextgis-borsch/lib_gdal.git $HOME/nextgis/lib_gdal
