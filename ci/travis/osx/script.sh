#!/bin/sh

set -e

echo "\n== Build JSON-C ========================================================\n"
cd $HOME/build/nextgis/lib_jsonc
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_jsonc/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build GEOS ==========================================================\n"
cd $HOME/build/nextgis/lib_geos
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_geos/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build OpenSSL =======================================================\n"
cd $HOME/build/nextgis/lib_openssl
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DOPENSSL_NO_DYNAMIC_ENGINE=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_openssl/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build PROJ ==========================================================\n"
cd $HOME/build/nextgis/lib_proj
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_proj/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build JPEG ==========================================================\n"
cd $HOME/build/nextgis/lib_jpeg
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_jpeg/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build PNG ===========================================================\n"
cd $HOME/build/nextgis/lib_png
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_png/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build CURL ==========================================================\n"

cd $HOME/build/nextgis/lib_curl
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DWITH_OpenSSL=ON -DWITH_ZLIB=ON -DENABLE_THREADED_RESOLVER=ON -DCMAKE_USE_GSSAPI=ON -DCMAKE_USE_LIBSSH2=OFF -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_curl/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build SQLite ========================================================\n"
cd $HOME/build/nextgis/lib_sqlite
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_sqlite/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build TIFF ==========================================================\n"
cd $HOME/build/nextgis/lib_tiff
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DWITH_ZLIB=ON -DWITH_JPEG=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_tiff/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build GeoTIFF =======================================================\n"
cd $HOME/build/nextgis/lib_geotiff
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DWITH_ZLIB=ON -DWITH_JPEG=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_geotiff/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

echo "\n== Build GDAL ==========================================================\n"
cd $HOME/build/nextgis/lib_gdal
mkdir build
cd build
# -DSUPPRESS_VERBOSE_OUTPUT=ON
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSKIP_DEFAULTS=ON -DWITH_PYTHON=OFF -DENABLE_CAD=OFF -DWITH_ZLIB=ON -DWITH_EXPAT=ON -DWITH_GeoTIFF=ON -DWITH_ICONV=ON -DWITH_JSONC=ON -DWITH_LibXml2=ON -DWITH_TIFF=ON -DWITH_JPEG=ON -DWITH_PNG=ON -DWITH_SQLite3=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_gdal/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

# echo "\n== Build Boost =========================================================\n"
# cd $HOME/build/nextgis/lib_boost
# mkdir build
# cd build
# cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_boost/inst ..
# cmake --build . --config release -- -j 4
# cmake --build . --config release --target install
#
# echo "\n== Build CGAL ==========================================================\n"
# cd $HOME/build/nextgis/lib_cgal
# mkdir build
# cd build
# cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/build/nextgis/lib_cgal/inst ..
# cmake --build . --config release -- -j 4
# cmake --build . --config release --target install

echo "\n++ Build NextGIS Datastore +++++++++++++++++++++++++++++++++++++++++++++\n"
export GDAL_DATA=$HOME/build/nextgis/lib_gdal/data
export PROJ_LIB=$HOME/build/nextgis/lib_proj/nad

cd $HOME/build/nextgis/nextgis_datastore
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON -DBUILD_TARGET_PLATFORM=Desktop -DBUILD_TESTING=ON -DBUILD_TESTS=ON ..
cmake --build . --config release -- -j 4
#cmake --build . --config release --target test
ctest -V
