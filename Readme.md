# NextGIS storage and visualisation support library

## Intro

NextGIS storage and visualisation support library is a C++ library with C, Python
and Java bindings. The library provides API to optimized vector and raster
geodata storage, reading and modification (datasource) and maps creation from
these geodata.

Developer can create many maps with different content (layers, based on geodata
from datasource) and define different styles. Map can be panned and zoomed, the
current extent is stored in map context. Map can return rendered images to display
to user in various graphic interfaces. Rendering is optimised for big amounts of geodata and uses non
blocking (async) functions. Map spatial reference is Web Mercator.

## Formats

Datasource supports limited input of GDAL vector and raster formats. Supported
vector formats:

 * GeoJSON
 * ESRI Shape file
 * MapInfo Tab
 * KML

Supported raster formats (only in EPSG:3857):

 * GeoTIFF
 * Png
 * Jpeg

Supported online services:

 * WMS (EPSG:3857)
 * WFS

Supported tiled services

 * TMS (EPSG:3857, 3395)
 * MVT

## Unit tests

| Environment           | Status        |
| --------------------- |--------------:|
| Ubuntu trusty GCC     | [![Build Status](http://badges.herokuapp.com/travis/nextgis/nextgis_datastore?branch=master&env=BUILD_NAME=linux_gcc&label=linux_gcc)](https://travis-ci.org/nextgis/nextgis_datastore) |
| Ubuntu trusty Clang   | [![Build Status](http://badges.herokuapp.com/travis/nextgis/nextgis_datastore?branch=master&env=BUILD_NAME=linux_clang&label=linux_clang)](https://travis-ci.org/nextgis/nextgis_datastore) |
| Mac OS X              | [![Build Status](http://badges.herokuapp.com/travis/nextgis/nextgis_datastore?branch=master&env=BUILD_NAME=osx&label=osx)](https://travis-ci.org/nextgis/nextgis_datastore) |
| Android              | [![Build Status](http://badges.herokuapp.com/travis/nextgis/nextgis_datastore?branch=master&env=BUILD_NAME=android&label=android)](https://travis-ci.org/nextgis/nextgis_datastore) |

Details [link](https://travis-ci.org/nextgis/nextgis_datastore)

## NextGIS Web and My NextGIS integration

Library also supports NextGIS Web integration:

 * NGW instance access
 * supported resources listing
 * rendering or loading vector geodata (NGW Json, GeoJSON, MVT, WFS)
 * rendering or loading raster (WMS, TMS)
 * syncing vector geodata with NGW instance (history tables support)
 * sending tracking data

## Details

During the upload to the internal database (geopackage) the vector geodata is
projected into EPSG:3857 and generalised for several zoom levels (equivalent to
4, 8, 12 and 16). This is needed for fast drawing on all map scales. For raster data
appropriate overviews generated as well.

The online data is cached for fast access in common place to use from different
instances of library.

# Usage

The library can be linked from desktop software or used via bindings. There are
special support scripts for mobile platforms (Android and iOS). See. /opt/android
and /opt/ios folders.

For Android the gradle script includes several support functions and main build
and copy tasks. Also special Android.mk and application.mk files with linked
libraries and supported ABIs set.

For iOS special Python script *build_framework.py* to build XCode framework.
Special swift file with cast main C types is provided as well.

# License

The library code, CMake scripts and other files are distributed under the terms
of GNU Lesser Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

# Commercial support

Need to fix a bug or add a feature to NextGIS Datastore? We provide custom development and support for this software. [Contact us](http://nextgis.ru/en/contact/) to discuss options!
