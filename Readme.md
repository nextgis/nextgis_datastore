# NextGIS store and visualisation support library

## Intro

NextGIS store and visualisation support library is a C++ library with C, Python 
and Java bindings. The library provides API to optimised store vector and raster 
data, read and modify this geodata (datasource) and form different maps from 
this geodata.

Developer can create many maps with different content (layers, based on geodata 
from datasource) and define different styles. Map can be panned and zoomed, the 
current extent stores in map context. Map can return rendered images to display 
for user in interface. Rendering optimised for big amount of geodata and use non 
blocking (async) functions. Map spatial reference is Web Mercator.

## Formats

Datasource support limited input of GDAL vector and raster formats. Supported
vector formats:

 * GeoJSON
 * ESRI Shape file
 * MapInfo Tab
 * KML

Supported raster formats (only in EPSG:3857):

 * GeoTiff
 * Png 
 * Jpeg

Supported online services:
    
 * WMS (EPSG:3857)
 * WFS
 
Supported tiled services

 * TMS (EPSG:3857, 3395)
 * MVT

## NextGIS Web and NextGIS Online integration

Also library support NextGIS Web integration: 

 * access NGW instance
 * list supported resources
 * render or load vector geodata (NGW Json, GeoJSON, MVT, WFS) 
 * render or load raster (WMS, TMS)
 * sync vector geodata with NGW instance (history tables support)
 * send tracking data

## Details

During the load into the internal database (geopackage) the vector geodata 
projected into EPSG:3857 and generalised for several zoom levels (equivalent to 
4, 8, 12 and 16). This need for fast drawing on some map scales. For raster data
appropriate overviews generated.

The online data cached for fast access in common place to use from different 
instances of library.

# Usage

The library can linked as is with desktop software or use via bindings. There are 
special support scripts for mobile platforms (Android and iOS). See. /opt/android
and /opt/ios folders.

For Android the gradle script includes several support functions and main build 
and copy tasks. Also special Android.mk and application.mk files with linked
libraries and supported ABIs set.

For iOS special Python script *build_framework.py* to build XCode framework.
Also special swift file with cast main C types exists.

# License

The library code, CMake scripts and other files are distributed under the terms
of GNU Lesser Public License as published by the Free Software Foundation, 
either version 3 of the License, or (at your option) any later version. 
