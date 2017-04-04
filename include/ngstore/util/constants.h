/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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

#ifndef NGSCONSTANTS_H
#define NGSCONSTANTS_H

#include <cmath>

constexpr int NOT_FOUND = -1;


// Maps
/*#define MAP_NAME "name"
#define MAP_DESCRIPTION "descript"
#define MAP_LAYERS "layers"
#define MAP_RELATIVEPATHS "relative_paths"
#define MAP_EPSG "epsg"
#define MAP_MIN_X "min_x"
#define MAP_MIN_Y "min_y"
#define MAP_MAX_X "max_x"
#define MAP_MAX_Y "max_y"
#define MAP_BKCOLOR "bk_color"
#define MAP_SINGLESOURCE "single_src"
#define DEFAULT_MAP_NAME "default"
*/
constexpr unsigned short DEFAULT_EPSG = 3857;
constexpr double DEFAULT_MAX_X2 = 40075016.68;
constexpr double DEFAULT_MAX_Y2 = 40075016.68;
constexpr double DEFAULT_MAX_X = 20037508.34;
constexpr double DEFAULT_MAX_Y = 20037508.34;
constexpr double DEFAULT_MIN_X = -DEFAULT_MAX_X;
constexpr double DEFAULT_MIN_Y = -DEFAULT_MAX_Y;

/*
#define DEFAULT_MAX_X 20037508.34 // 180.0
#define DEFAULT_MAX_Y 20037508.34 // 90.0
#define DEFAULT_MIN_X -DEFAULT_MAX_X
#define DEFAULT_MIN_Y -DEFAULT_MAX_Y
#define DEFAULT_RATIO 1
#define MAP_DOCUMENT_EXT "ngmd"
#define DEFAULT_TILE_SIZE 256

// Layers
#define LAYER_NAME "name"
#define LAYER_SOURCE_TYPE "source_type"
#define LAYER_SOURCE "source"
#define DEFAULT_LAYER_NAME "new layer"
//#define MAP_ID "map_id"
//#define LAYER_STYLE "style"
//#define DATASET_NAME "ds_name"

// Common
#define NAME_FIELD_LIMIT 64
#define ALIAS_FIELD_LIMIT 255
#define DESCRIPTION_FIELD_LIMIT 1024

// Draw
#define NOTIFY_PERCENT 0.1

// Common
#define BIG_VALUE 10000000.0

#define DEG2RAD M_PI / 180.0
*/

constexpr double DEG2RAD = M_PI / 180.0;


#endif // NGSCONSTANTS_H
