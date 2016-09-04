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
 
#ifndef API_H
#define API_H

#include "common.h"

/**
 * @brief The NextGIS store and visualisation library error codes enum
 */
enum ngsErrorCodes {
    SUCCESS = 0,        /**< Success */
    UNEXPECTED_ERROR,   /**< Unexpected error */
    PATH_NOT_SPECIFIED, /**< Path is not specified */
    INVALID,            /**< Path, map, structure, etc. is invalid */
    UNSUPPORTED,        /**< The feature is unsupported */
    CREATE_FAILED,      /**< Create failed */
    DELETE_FAILED,      /**< Failed to delete file, folder or something else */
    SAVE_FAILED,        /**< Failed to save file, folder or something else */
    SET_FAILED,         /**< Failed to set value */
    OPEN_FAILED,        /**< Failed to open file, folder or something else */
    INSERT_FAILED,      /**< Insert new feature failed */
    UPDATE_FAILED,      /**< Update feature failed */
    INIT_FAILED,        /**< Initialise failed */
    COPY_FAILED,        /**< Copy failed */
    MOVE_FAILED,        /**< Move failed */
    CLOSE_FAILED,       /**< Close failed */
};

/**
 * @brief The table, datsource, map and etc. change codes enum
 */
enum ngsChangeCodes {
    NOP = 0,
    CREATE_RESOURCE,
    DELETE_RESOURCE,
    CHANGE_RESOURCE,
    CREATE_FEATURE,
    CHANGE_FEATURE,
    DELETE_FEATURE,
    DELETEALL_FEATURES,
    CREATE_ATTACHMENT,
    CHANGE_ATTACHMENT,
    DELETE_ATTACHMENT,
    DELETEALL_ATTACHMENTS
};

/**
 * @brief The notification source code enum
 */
enum ngsSourceCodes {
    UNDEFINED = 0,
    DATA_STORE,
    MAP_STORE,
    DATASET
};

/**
 * @brief Prototype of function, which executed periodically during some long
 * process.
 * @param complete Progress percent from 0 to 1
 * @param message Some user friendly message from process
 * @param progressArguments Data from user
 * @return 1 to continue execute process or any other value - to cancel
 */
typedef int (*ngsProgressFunc)(double complete, const char* message,
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

typedef struct _ngsCoordinate {
    double X;
    double Y;
    double Z;
} ngsCoordinate;

typedef struct _ngsPosition {
    int X;
    int Y;
} ngsPosition;

enum ngsDirection {
    X = 0,
    Y,
    Z
};

/**
 * Common functions
 */
NGS_EXTERNC int ngsGetVersion(const char* request);
NGS_EXTERNC const char* ngsGetVersionString(const char* request);
NGS_EXTERNC int ngsInit(const char* dataPath, const char* cachePath);
NGS_EXTERNC void ngsUninit();
NGS_EXTERNC void ngsOnLowMemory();
NGS_EXTERNC void ngsOnPause();
NGS_EXTERNC void ngsSetNotifyFunction(ngsNotifyFunc callback);
/**
 * Storage functions
 */
NGS_EXTERNC int ngsDataStoreInit(const char* path);
NGS_EXTERNC int ngsDataStoreDestroy(const char* path, const char* cachePath);
NGS_EXTERNC int ngsCreateRemoteTMSRaster(const char* url, const char* name,
                                         const char* alias, const char* copyright,
                                         int epsg, int z_min, int z_max,
                                         bool y_origin_top);
NGS_EXTERNC int ngsDataStoreLoad(const char* name, const char* path,
                        const char *subDatasetName, bool move,
                        unsigned int skipFlags, ngsProgressFunc callback,
                        void* callbackData);
/**
 * Map functions
 *
 *  ngsCreateMap -> ngsInitMap -> ngsSaveMap [optional]
 *  ngsLoadMap -> ngsInitMap -> ngsSaveMap [optional]
 */
NGS_EXTERNC unsigned char ngsMapCreate(const char* name, const char* description,
                             unsigned short epsg, double minX, double minY,
                             double maxX, double maxY);
NGS_EXTERNC unsigned char ngsMapOpen(const char* path);
NGS_EXTERNC int ngsMapSave(unsigned char mapId, const char* path);
NGS_EXTERNC int ngsMapClose(unsigned char mapId);
NGS_EXTERNC int ngsMapInit(unsigned char mapId);
NGS_EXTERNC int ngsMapSetSize(unsigned char mapId, int width, int height,
                           int isYAxisInverted);
NGS_EXTERNC int ngsMapDraw(unsigned char mapId, ngsProgressFunc callback,
                           void* callbackData);
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
NGS_EXTERNC ngsCoordinate ngsMapGetCoordinate(unsigned char mapId, int x, int y);
NGS_EXTERNC ngsCoordinate ngsMapGetDistance(unsigned char mapId, int w, int h);
NGS_EXTERNC ngsPosition ngsDisplayGetPosition(unsigned char mapId, double x, double y);
NGS_EXTERNC ngsPosition ngsDisplayGetLength(unsigned char mapId, double w, double h);

#endif // API_H
