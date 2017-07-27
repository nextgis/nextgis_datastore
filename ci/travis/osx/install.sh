#!/bin/sh

set -e

brew update
brew install swig

git clone --depth 1 https://github.com/nextgis-borsch/lib_jsonc.git $HOME/build/nextgis/lib_jsonc
git clone --depth 1 https://github.com/nextgis-borsch/lib_geos.git $HOME/build/nextgis/lib_geos
git clone --depth 1 https://github.com/nextgis-borsch/lib_proj.git $HOME/build/nextgis/lib_proj
git clone --depth 1 https://github.com/nextgis-borsch/lib_jpeg.git $HOME/build/nextgis/lib_jpeg
git clone --depth 1 https://github.com/nextgis-borsch/lib_png.git $HOME/build/nextgis/lib_png
git clone --depth 1 https://github.com/nextgis-borsch/lib_tiff.git $HOME/build/nextgis/lib_tiff
git clone --depth 1 https://github.com/nextgis-borsch/lib_geotiff.git $HOME/build/nextgis/lib_geotiff
git clone --depth 1 https://github.com/nextgis-borsch/lib_openssl.git $HOME/build/nextgis/lib_openssl
git clone --depth 1 https://github.com/nextgis-borsch/lib_curl.git $HOME/build/nextgis/lib_curl
git clone --depth 1 https://github.com/nextgis-borsch/lib_gdal.git $HOME/build/nextgis/lib_gdal
git clone --depth 1 https://github.com/nextgis-borsch/lib_sqlite.git $HOME/build/nextgis/lib_sqlite
#git clone --depth 1 https://github.com/nextgis-borsch/lib_boost.git $HOME/build/nextgis/lib_boost
#git clone --depth 1 https://github.com/nextgis-borsch/lib_cgal.git $HOME/build/nextgis/lib_cgal
