/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cmath>

#define NOT_FOUND -1

// Structure
#define MAIN_DATABASE "ngs.gpkg"
#define METHADATA_TABLE_NAME "ngs_meta"
#define ATTACHEMENTS_TABLE_NAME "ngs_attach"
#define RASTERS_TABLE_NAME "ngs_raster"
#define MAPS_TABLE_NAME "ngs_maps"
#define LAYERS_TABLE_NAME "ngs_layers"
#define SYS_TABLE_COUNT 5

// Metadata
#define META_KEY "key"
#define META_KEY_LIMIT 64
#define META_VALUE "value"
#define META_VALUE_LIMIT 255

// Rasters
#define LAYER_URL "url"
#define LAYER_NAME "name"
#define LAYER_ALIAS "alias"
#define LAYER_TYPE "type"
#define LAYER_COPYING "copyright"
#define LAYER_EPSG "epsg"
#define LAYER_MIN_Z "z_min"
#define LAYER_MAX_Z "z_max"
#define LAYER_YORIG_TOP "y_origin_top"
#define LAYER_ACCOUNT "account"

// Attachments
#define ATTACH_TABLE "table"
#define ATTACH_FEATURE "fid"
#define ATTACH_ID "attid"
#define ATTACH_SIZE "size"
#define ATTACH_FILE_NAME "file_name"
#define ATTACH_FILE_MIME "file_mime"
#define ATTACH_DESCRIPTION "descript"
#define ATTACH_DATA "data"
#define ATTACH_FILE_DATE "date"

// Maps
#define MAP_NAME "name"
#define MAP_DESCRIPTION "descript"
#define MAP_LAYERS "layers"
#define MAP_EPSG "epsg"
#define MAP_MIN_X "min_x"
#define MAP_MIN_Y "min_y"
#define MAP_MAX_X "max_x"
#define MAP_MAX_Y "max_y"
#define MAP_BKCOLOR "bk_color"
#define DEFAULT_MAP_NAME "default"
#define DEFAULT_EPSG 3857
#define DEFAULT_MAX_X 20037508.34 // 180.0
#define DEFAULT_MAX_Y 20037508.34 // 90.0
#define DEFAULT_MIN_X -DEFAULT_MAX_X
#define DEFAULT_MIN_Y -DEFAULT_MAX_Y
#define DEFAULT_RATIO DEFAULT_MAX_X / DEFAULT_MAX_Y

// Layers
#define MAP_ID "map_id"
#define LAYER_STYLE "style"
#define DATASET_NAME "ds_name"

// Common
#define NGS_VERSION_KEY "ngs_version"
#define NAME_FIELD_LIMIT 64
#define ALIAS_FIELD_LIMIT 255
#define DESCRIPTION_FIELD_LIMIT 1024

// GDAL
#define HTTP_TIMEOUT "5"
#define HTTP_USE_GZIP "ON"
#define CACHEMAX "24"

// Common
#define DELTA 0.00000001
#define BIG_VALUE 10000000.0

inline bool isEqual(double val1, double val2) {return fabs(val1 - val2) < DELTA; };
#define ARRAY_SIZE(array) (sizeof((array))/sizeof((array[0])))

#endif // CONSTANTS_H
