#!/bin/sh

set -e

cd $HOME/nextgis/lib_jsonc
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/nextgis/lib_jsonc/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

cd $HOME/nextgis/lib_geos
mkdir build
cd build
cmake -DOSX_FRAMEWORK=ON -DREGISTER_PACKAGE=ON -DSUPPRESS_VERBOSE_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=$HOME/nextgis/lib_geos/inst ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target install

cd $HOME/nextgis/nextgis_datastore
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON -DBUILD_TARGET_PLATFORM=Desktop -DBUILD_TESTING=ON -DBUILD_TESTS=ON ..
cmake --build . --config release -- -j 4
cmake --build . --config release --target test
