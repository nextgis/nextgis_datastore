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
    CC_NOP = 0,
    CC_CREATE_RESOURCE,
    CC_DELETE_RESOURCE,
    CC_CHANGE_RESOURCE,
    CC_CREATE_FEATURE,
    CC_CHANGE_FEATURE,
    CC_DELETE_FEATURE,
    CC_DELETEALL_FEATURES,
    CC_CREATE_ATTACHMENT,
    CC_CHANGE_ATTACHMENT,
    CC_DELETE_ATTACHMENT,
    CC_DELETEALL_ATTACHMENTS
};

/**
 * @brief The notification source code enum
 */
enum ngsSourceCodes {
    SC_UNDEFINED = 0,
    SC_DATA_STORE,
    SC_MAP_STORE,
    SC_DATASET
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
enum ngsDataStoreOptionsTypes {
    OT_CREATE_DATASOURCE,
    OT_CREATE_DATASET,
    OT_OPEN,
    OT_LOAD
};

typedef struct _ngsRGBA {
    unsigned char R;
    unsigned char G;
    unsigned char B;
    unsigned char A;
} ngsRGBA;

/* Spatial coordinates */
typedef struct _ngsCoordinate {
    double X;
    double Y;
    double Z;
} ngsCoordinate;

/* Display coordinates */
typedef struct _ngsPosition {
    double X;
    double Y;
} ngsPosition;

typedef struct _ngsLoadTaskInfo {
    const char* name;
    const char* newNames;
    const char* dstPath;
    enum ngsErrorCodes status;
} ngsLoadTaskInfo;

enum ngsDirection {
    X = 0,
    Y,
    Z
};

enum ngsDriverType {
    DT_VECTOR     = 1 << 1,
    DT_RASTER     = 1 << 2,
    DT_SERVICE    = 1 << 3,
    DT_NETWORK    = 1 << 4,
    DT_GNM        = 1 << 5,
    DT_VECTOR_ALL = 1 << 6,
    DT_RASTER_ALL = 1 << 7
};

enum ngsFileMode {
    FM_READ   = 1 << 1,
    FM_WRITE  = 1 << 2
};

#endif // NGSCODES_H
