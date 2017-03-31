/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
 * @brief The NextGIS store and visualisation library error codes enum
 */
enum ngsErrorCodes {
    EC_SUCCESS = 200,      /**< Success */
    EC_IN_PROCESS,         /**< In process */
    EC_PENDING,            /**< Pending */
    EC_CANCELED,           /**< Canceled */
    EC_UNEXPECTED_ERROR = 400,   /**< Unexpected error */
    EC_NOT_SPECIFIED,      /**< Path, value, etc. is not specified */
    EC_INVALID,            /**< Path, map, structure, etc. is invalid */
    EC_UNSUPPORTED,        /**< The feature is unsupported */
    EC_CREATE_FAILED,      /**< Create failed */
    EC_DELETE_FAILED,      /**< Failed to delete file, folder or something else */
    EC_SAVE_FAILED,        /**< Failed to save file, folder or something else */
    EC_SET_FAILED,         /**< Failed to set value */
    EC_OPEN_FAILED,        /**< Failed to open file, folder or something else */
    EC_INSERT_FAILED,      /**< Insert new feature failed */
    EC_UPDATE_FAILED,      /**< Update feature failed */
    EC_INIT_FAILED,        /**< Initialise failed */
    EC_COPY_FAILED,        /**< Copy failed */
    EC_MOVE_FAILED,        /**< Move failed */
    EC_CLOSE_FAILED,       /**< Close failed */
};

/**
 * @brief The table, datasource, map and etc. change codes enum
 */
enum ngsChangeCodes {
    CC_NOP                  = 0 << 1,
    CC_CREATE_OBJECT        = 1 << 1,
    CC_DELETE_OBJECT        = 2 << 1,
    CC_CHANGE_OBJECT        = 3 << 1,
    CC_CREATE_FEATURE       = 4 << 1,
    CC_CHANGE_FEATURE       = 5 << 1,
    CC_DELETE_FEATURE       = 6 << 1,
    CC_DELETEALL_FEATURES   = 7 << 1,
    CC_CREATE_ATTACHMENT    = 8 << 1,
    CC_CHANGE_ATTACHMENT    = 9 << 1,
    CC_DELETE_ATTACHMENT    = 10 << 1,
    CC_DELETEALL_ATTACHMENTS = 11 << 1
};

/**
 * @brief The draw state enum
 */
enum ngsDrawState {
    DS_NORMAL = 1,  /**< normal draw */
    DS_REDRAW,      /**< free all caches and draw from the scratch */
    DS_PRESERVED    /**< draw from caches */
};

/**
 * @brief The ngsDataStoreOptionsTypes enum
 */
enum ngsOptionTypes {
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
    CAT_CONTAINER_ANY,          /** Any container border */
    CAT_CONTAINER_ROOT,
    CAT_CONTAINER_LOCALCONNECTION,
    CAT_CONTAINER_DIR,
    CAT_CONTAINER_GDB,
    CAT_CONTAINER_GDB_SET,
    CAT_CONTAINER_POSTGRES,
    CAT_CONTAINER_POSTGRES_SCHEMA,
    CAT_CONTAINER_WFS,
    CAT_CONTAINER_WMS,
    CAT_CONTAINER_NGW,
    CAT_CONTAINER_KML,
    CAT_CONTAINER_KMZ,
    CAT_CONTAINER_SXF,
    CAT_CONTAINER_ALL,
    CAT_FC_ANY,                 /** Any Feature class */
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
    CAT_FC_ALL,
    CAT_RASTER_ANY,             /** Any raster */
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
    CAT_RASTER_ALL,
    CAT_TABLE_ANY,              /** Any table */
    CAT_TABLE_POSTGRES,
    CAT_TABLE_MAPINFO_TAB,
    CAT_TABLE_MAPINFO_MIF,
    CAT_TABLE_CSV,
    CAT_TABLE_GDB,
    CAT_TABLE_ODS,
    CAT_TABLE_XLS,
    CAT_TABLE_XLSX,
    CAT_TABLE_ALL,
    CAT_QUERY_RESULT
};

#endif // NGSCODES_H
