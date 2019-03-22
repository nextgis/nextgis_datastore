/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author:  NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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
#ifndef NGSCODES_H
#define NGSCODES_H

/**
 * @brief The NextGIS store and visualisation library codes enum
 */
enum ngsCode {
    COD_CONTINUE = 100,     /**< Continue */
    COD_PENDING,            /**< Pending */
    COD_IN_PROCESS,         /**< In process */
    COD_SUCCESS = 200,      /**< Success */
    COD_CANCELED,           /**< Canceled */
    COD_FINISHED,           /**< Finished */
    COD_WARNING = 300,      /**< Warning, not error */
    COD_UNEXPECTED_ERROR = 400,   /**< Unexpected error */
    COD_NOT_SPECIFIED,      /**< Path, value, etc. is not specified */
    COD_INVALID,            /**< Path, map, structure, etc. is invalid */
    COD_UNSUPPORTED,        /**< The feature is unsupported */
    COD_CREATE_FAILED,      /**< Create failed */
    COD_DELETE_FAILED,      /**< Failed to delete file, folder or something else */
    COD_SAVE_FAILED,        /**< Failed to save file, folder or something else */
    COD_SET_FAILED,         /**< Failed to set value */
    COD_GET_FAILED,         /**< Failed to get value */
    COD_OPEN_FAILED,        /**< Failed to open file, folder or something else */
    COD_INSERT_FAILED,      /**< Insert new feature failed */
    COD_UPDATE_FAILED,      /**< Update feature failed */
    COD_INIT_FAILED,        /**< Initialise failed */
    COD_COPY_FAILED,        /**< Copy failed */
    COD_MOVE_FAILED,        /**< Move failed */
    COD_CLOSE_FAILED,       /**< Close failed */
    COD_LOAD_FAILED,        /**< Load failed */
    COD_RENAME_FAILED,      /**< Rename failed */
    COD_DRAW_FAILED,        /**< Draw failed */
    COD_REQUEST_FAILED,     /**< URL Request failed */
    COD_FUNCTION_NOT_AVAILABLE /**< Function is not available for current plan or account is not authorized */
};

/**
 * @brief The table, datasource, map and etc. change codes enum
 */
enum ngsChangeCode {
    CC_NOP                  = 1 << 0,
    CC_CREATE_OBJECT        = 1 << 1,
    CC_DELETE_OBJECT        = 1 << 2,
    CC_CHANGE_OBJECT        = 1 << 3,
    CC_CREATE_FEATURE       = 1 << 4,
    CC_CHANGE_FEATURE       = 1 << 5,
    CC_DELETE_FEATURE       = 1 << 6,
    CC_DELETEALL_FEATURES   = 1 << 7,
    CC_CREATE_ATTACHMENT    = 1 << 8,
    CC_CHANGE_ATTACHMENT    = 1 << 9,
    CC_DELETE_ATTACHMENT    = 1 << 10,
    CC_DELETEALL_ATTACHMENTS = 1 << 11,
    CC_CREATE_MAP           = 1 << 12,
    CC_CHANGE_MAP           = 1 << 13,
    CC_CREATE_LAYER         = 1 << 14,
    CC_DELETE_LAYER         = 1 << 15,
    CC_CHANGE_LAYER         = 1 << 16,
    CC_TOKEN_EXPIRED        = 1 << 17,
    CC_TOKEN_CHANGED        = 1 << 18,
    CC_ALL = CC_CREATE_OBJECT | CC_DELETE_OBJECT | CC_CHANGE_OBJECT | CC_CREATE_FEATURE | CC_CHANGE_FEATURE | CC_DELETE_FEATURE | CC_DELETEALL_FEATURES | CC_CREATE_ATTACHMENT | CC_CHANGE_ATTACHMENT | CC_DELETE_ATTACHMENT | CC_DELETEALL_ATTACHMENTS | CC_CREATE_MAP | CC_CHANGE_MAP | CC_CREATE_LAYER | CC_DELETE_LAYER | CC_CHANGE_LAYER | CC_TOKEN_EXPIRED | CC_TOKEN_CHANGED
};

/**
 * @brief The draw state enum
 */
enum ngsDrawState {
    DS_NORMAL = 1,  /**< Normal draw */
    DS_REDRAW,      /**< Free all caches and draw from the scratch */
    DS_REFILL,      /**< Refill tiles from layers */
    DS_PRESERVED,   /**< Draw from caches */
    DS_NOTHING      /**< Draw nothing */
};

/**
 * @brief The ngsDataStoreOptionsTypes enum
 */
enum ngsOptionType {
    OT_CREATE_DATASOURCE,
    OT_CREATE_RASTER,
    OT_CREATE_LAYER,
    OT_CREATE_LAYER_FIELD,
    OT_OPEN,
    OT_LOAD
};

enum ngsDirection {
    DIR_X = 0,
    DIR_Y,
    DIR_Z
};

enum ngsFileMode {
    FM_READ   = 1 << 1,
    FM_WRITE  = 1 << 2
};

/**
 * @brief The Catalog Object Types enum
 */
enum ngsCatalogObjectType {
    CAT_UNKNOWN,
    CAT_CONTAINER_ANY = 50,         /**< Any container border */
    CAT_CONTAINER_ROOT,
    CAT_CONTAINER_LOCALCONNECTIONS, /**< Local system folder connections */
    CAT_CONTAINER_DIR,
    CAT_CONTAINER_ARCHIVE,
    CAT_CONTAINER_ARCHIVE_ZIP,
    CAT_CONTAINER_ARCHIVE_DIR,
    CAT_CONTAINER_GDB,
    CAT_CONTAINER_GDB_SET,
    CAT_CONTAINER_POSTGRES,
    CAT_CONTAINER_POSTGRES_SCHEMA,
    CAT_CONTAINER_WFS,
    CAT_CONTAINER_WMS,
    CAT_CONTAINER_NGW,              /**< NextGIS Web connection */
    CAT_CONTAINER_NGS,              /**< NextGIS storage (GPKG with additions) */
    CAT_CONTAINER_KML,
    CAT_CONTAINER_KMZ,
    CAT_CONTAINER_SXF,
    CAT_CONTAINER_GPKG,
    CAT_CONTAINER_SQLITE,
    CAT_CONTAINER_SIMPLE,           /**< For one layer containers */
    CAT_CONTAINER_MEM,              /**< For memory layers */
    CAT_CONTAINER_GISCONNECTIONS,   /**< GIS Servers/services connections */
    CAT_CONTAINER_DBCONNECTIONS,    /**< Database servers connections */
    CAT_CONTAINER_NGWGROUP,         /**< NextGIS Web resource group */
    CAT_CONTAINER_NGWTRACKERGROUP,  /**< NextGIS Web trackers group */
    CAT_CONTAINER_DIR_LINK,         /**< Local connection to folder or symlink */
    CAT_CONTAINER_ALL = 499,
    CAT_FC_ANY = 500,               /**< Any Feature class */
    CAT_FC_ESRI_SHAPEFILE,
    CAT_FC_MAPINFO_TAB,
    CAT_FC_MAPINFO_MIF,
    CAT_FC_DXF,
    CAT_FC_POSTGIS,
    CAT_FC_GML,
    CAT_FC_GEOJSON,
    CAT_FC_WFS,
    CAT_FC_MEM,
    CAT_FC_KMLKMZ,
    CAT_FC_SXF,
    CAT_FC_S57,
    CAT_FC_GDB,
    CAT_FC_CSV,
    CAT_FC_GPKG,
    CAT_FC_LITE,
    CAT_FC_GPX,
    CAT_FC_ALL = 999,
    CAT_RASTER_ANY = 1000,          /**< Any raster */
    CAT_RASTER_BMP,
    CAT_RASTER_TIFF,
    CAT_RASTER_TIL,
    CAT_RASTER_IMG,
    CAT_RASTER_JPEG,
    CAT_RASTER_PNG,
    CAT_RASTER_GIF,
    CAT_RASTER_SAGA,
    CAT_RASTER_VRT,
    CAT_RASTER_WMS,
    CAT_RASTER_TMS,
    CAT_RASTER_POSTGIS,
    CAT_RASTER_GDB,
    CAT_RASTER_GPKG,
    CAT_RASTER_LITE,
    CAT_RASTER_MEM,
    CAT_RASTER_ALL = 1499,
    CAT_TABLE_ANY = 1500,           /**< Any table */
    CAT_TABLE_POSTGRES,
    CAT_TABLE_MAPINFO_TAB,
    CAT_TABLE_MAPINFO_MIF,
    CAT_TABLE_CSV,
    CAT_TABLE_GDB,
    CAT_TABLE_DBF,
    CAT_TABLE_GPKG,
    CAT_TABLE_ODS,
    CAT_TABLE_XLS,
    CAT_TABLE_XLSX,
    CAT_TABLE_LITE,
    CAT_TABLE_MEM,
    CAT_TABLE_ALL = 1999,
    CAT_FILE_ANY = 2000,
    CAT_FILE_NGMAPDOCUMENT,
    CAT_FILE_ALL = 2499,
    CAT_QUERY_RESULT,
    CAT_QUERY_RESULT_FC,
    CAT_RASTER_FC_ANY,
    CAT_NGW_TRACKER             /**< NextGIS Web tracker */
};

/**
 * @brief The URL Request Type enum
 */
enum ngsURLRequestType {
    URT_GET = 1,
    URT_POST,
    URT_PUT,
    URT_DELETE
};

/**
 * @brief The map view overlay type enum
 */
enum ngsMapOverlayType
{
    MOT_UNKNOWN = 1 << 0,
    MOT_LOCATION = 1 << 1,  /**< Overlay with current location */
    MOT_TRACK = 1 << 2,     /**< Overlay with current track */
    MOT_EDIT = 1 << 3,      /**< Overlay for geometry edit */
    MOT_FIGURES = 1 << 4,   /**< Overlay for some layer/datasource independent graphics */
    MOT_ALL = MOT_LOCATION | MOT_TRACK | MOT_EDIT | MOT_FIGURES
};

enum ngsMapTouchType
{
    MTT_ON_DOWN,
    MTT_ON_MOVE,
    MTT_ON_UP,
    MTT_SINGLE
};

/**
 * @brief The map layer draw style Type enum
 */
enum ngsStyleType {
    ST_POINT = 1,
    ST_LINE,
    ST_FILL,
    ST_IMAGE
};

enum ngsEditElementType {
    EET_POLYGON,
    EET_SELECTED_POLYGON,
    EET_LINE,
    EET_SELECTED_LINE,
    EET_MEDIAN_POINT,
    EET_SELECTED_MEDIAN_POINT,
    EET_WALK_POINT,
    EET_POINT,
    EET_SELECTED_POINT,
    EET_CROSS
};

enum ngsEditStyleType {
    EST_POINT,
    EST_LINE,
    EST_FILL,
    EST_CROSS
};

enum ngsEditDeleteResult {
    EDT_FAILED = 1,         /**< Delete operation failed */
    EDT_SELTYPE_NO_CHANGE,  /**< Same piece type is selected after delete operation */
    EDT_HOLE,               /**< Hole is deleted. Outer ring selected */
    EDT_PART,               /**< Part is deleted. Other part selected */
    EDT_GEOMETRY            /**< Whole geometry is deleted */
};

#endif // NGSCODES_H
