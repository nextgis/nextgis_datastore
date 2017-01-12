/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016,2017 NextGIS, <info@nextgis.com>
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

#ifndef NGSAPI_H
#define NGSAPI_H

#include "common.h"

/**
 * @brief The NextGIS store and visualisation library error codes enum
 */
enum ngsErrorCodes {
    EC_SUCCESS = 0,        /**< Success */
    EC_IN_PROCESS,         /**< In process */
    EC_PENDING,            /**< Pending */
    EC_CANCELED,           /**< Canceled */
    EC_UNEXPECTED_ERROR,   /**< Unexpected error */
    EC_PATH_NOT_SPECIFIED, /**< Path is not specified */
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

enum ngsDataStoreOptionsTypes {
    OT_CREATE_DATASOURCE,
    OT_CREATE_DATASET,
    OT_OPEN,
    OT_LOAD
};

/**
 * @brief Prototype of function, which executed periodically during some long
 * process.
 * @param id Process task id
 * @param complete If complete <= 1 then it means progress percent from 0 to 1,
 * long process is not complete.
 * If complete > 1 then long process is complete, progress percent equals (complete - 1)
 * @param message Some user friendly message from process
 * @param progressArguments Data from user
 * @return 1 to continue execute process or 0 - to cancel
 */
typedef int (*ngsProgressFunc)(unsigned int id, double complete,
                               const char* message,
                               void* progressArguments);
/**
 * @brief Prototype of function, which executed usually then some data changed
 * (dataset, map, etc.).
 * @param src Source of notification
 * @param table Table (Layer, Map) index
 * @param row Row (Feature) index or NOT_FOUND
 * @param operation Operation which trigger notification.
 */
typedef void (*ngsNotifyFunc)(enum ngsSourceCodes src, const char* table, long row,
                              enum ngsChangeCodes operation);

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

enum ngsOptions {
    OPT_NONE = 0,
    OPT_DEBUGMODE = 1 << 1
};

/**
 * Common functions
 */
NGS_EXTERNC int ngsGetVersion(const char* request);
NGS_EXTERNC const char* ngsGetVersionString(const char* request);
NGS_EXTERNC int ngsInit(const char* dataPath, const char* cachePath);
NGS_EXTERNC void ngsUninit();
NGS_EXTERNC void ngsOnPause();
NGS_EXTERNC void ngsOnLowMemory();
NGS_EXTERNC void ngsSetOptions(ngsOptions options);
NGS_EXTERNC ngsOptions ngsGetOptions();
NGS_EXTERNC void ngsSetNotifyFunction(ngsNotifyFunc callback);
NGS_EXTERNC const char* ngsGetFilters(unsigned int flags, unsigned int mode, const char *separator);
/**
 * Storage functions
 */
NGS_EXTERNC int ngsDataStoreInit(const char* path);
NGS_EXTERNC int ngsDataStoreDestroy(const char* path, const char* cachePath);
NGS_EXTERNC int ngsCreateRemoteTMSRaster(const char* url, const char* name,
                                         const char* alias, const char* copyright,
                                         int epsg, int z_min, int z_max,
                                         bool y_origin_top);
NGS_EXTERNC unsigned int ngsDataStoreLoad(const char* name, const char* path,
                        const char *subDatasetName, const char** options,
                        ngsProgressFunc callback, void* callbackData);
NGS_EXTERNC ngsLoadTaskInfo ngsDataStoreGetLoadTaskInfo(unsigned int taskId);
NGS_EXTERNC const char* ngsDataStoreGetOptions(ngsDataStoreOptionsTypes optionType);

/**
 * Map functions
 *
 *  ngsCreateMap -> ngsInitMap -> ngsSaveMap [optional]
 *  ngsLoadMap -> ngsInitMap -> ngsSaveMap [optional]
 */
NGS_EXTERNC int ngsMapInit(unsigned char mapId);
NGS_EXTERNC unsigned char ngsMapCreate(const char* name, const char* description,
                             unsigned short epsg, double minX, double minY,
                             double maxX, double maxY);
NGS_EXTERNC unsigned char ngsMapOpen(const char* path);
NGS_EXTERNC int ngsMapSave(unsigned char mapId, const char* path);
NGS_EXTERNC int ngsMapClose(unsigned char mapId);
NGS_EXTERNC int ngsMapSetSize(unsigned char mapId, int width, int height,
                           int isYAxisInverted);
NGS_EXTERNC int ngsMapDraw(unsigned char mapId, enum ngsDrawState state,
                           ngsProgressFunc callback, void* callbackData);
NGS_EXTERNC int ngsMapSetBackgroundColor(unsigned char mapId, unsigned char R,
                                    unsigned char G, unsigned char B,
                                    unsigned char A);
NGS_EXTERNC ngsRGBA ngsMapGetBackgroundColor(unsigned char mapId);
NGS_EXTERNC int ngsMapCreateLayer(unsigned char mapId, const char* name,
                               const char* path);
NGS_EXTERNC int ngsMapSetCenter(unsigned char mapId, double x, double y);
NGS_EXTERNC ngsCoordinate ngsMapGetCenter(unsigned char mapId);
NGS_EXTERNC int ngsMapSetScale(unsigned char mapId, double scale);
NGS_EXTERNC double ngsMapGetScale(unsigned char mapId);
NGS_EXTERNC int ngsMapSetRotate(unsigned char mapId, enum ngsDirection dir,
                                double rotate);
NGS_EXTERNC double ngsMapGetRotate(unsigned char mapId, enum ngsDirection dir);
NGS_EXTERNC ngsCoordinate ngsMapGetCoordinate(unsigned char mapId, double x, double y);
NGS_EXTERNC ngsCoordinate ngsMapGetDistance(unsigned char mapId, double w, double h);
NGS_EXTERNC ngsPosition ngsDisplayGetPosition(unsigned char mapId, double x, double y);
NGS_EXTERNC ngsPosition ngsDisplayGetLength(unsigned char mapId, double w, double h);

#endif // NGSAPI_H
