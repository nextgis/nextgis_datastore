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
    SUCCESS = 0,        /**< success */
    UNEXPECTED_ERROR,   /**< unexpected error */
    PATH_NOT_SPECIFIED, /**< path is not specified */
    INVALID,            /**< path, map, structure, etc. is invalid */
    UNSUPPORTED,        /**< the feature is unsupported */
    CREATE_FAILED,      /**< Create failed */
    DELETE_FAILED,      /**< Faild to delete file, folder or something else */
    SAVE_FAILED,        /**< Faild to save file, folder or something else */
    OPEN_FAILED,        /**< Faild to open file, folder or something else */
    INSERT_FAILED,      /**< insert new feature failed */
    UPDATE_FAILED,      /**< update feature failed */
    INIT_FAILED,        /**< Initialise failed */
    COPY_FAILED,        /**< Copy failed */
    MOVE_FAILED         /**< Move failed */
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

typedef struct _ngsRGBA
{
    unsigned char R;
    unsigned char G;
    unsigned char B;
    unsigned char A;
} ngsRGBA;

typedef struct _ngsRawPoint
{
    double x;
    double y;
} ngsRawPoint;

typedef struct _ngsRawEnvelope
{
    double MinX;
    double MaxX;
    double MinY;
    double MaxY;
} ngsRawEnvelope;

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
NGS_EXTERNC int ngsInitDataStore(const char* path);
NGS_EXTERNC int ngsDestroyDataStore(const char* path, const char* cachePath);
NGS_EXTERNC int ngsCreateRemoteTMSRaster(const char* url, const char* name,
                                         const char* alias, const char* copyright,
                                         int epsg, int z_min, int z_max,
                                         bool y_origin_top);
NGS_EXTERNC int ngsLoad(const char* name, const char* path,
                        const char *subDatasetName, bool move,
                        unsigned int skipFlags, ngsProgressFunc callback,
                        void* callbackData);
/**
 * Map functions
 *
 *  ngsCreateMap -> ngsInitMap -> ngsSaveMap [optional]
 *  ngsLoadMap -> ngsInitMap -> ngsSaveMap [optional]
 */
NGS_EXTERNC int ngsCreateMap(const char* name, const char* description,
                             unsigned short epsg, double minX, double minY,
                             double maxX, double maxY);
NGS_EXTERNC int ngsOpenMap(const char* path);
NGS_EXTERNC int ngsSaveMap(unsigned int mapId, const char* path);
NGS_EXTERNC int ngsInitMap(unsigned int mapId, void* imageBufferPointer, int width, int height, int isYAxisInverted);
NGS_EXTERNC int ngsDrawMap(unsigned int mapId, ngsProgressFunc callback,
                           void* callbackData);
NGS_EXTERNC int ngsSetMapBackgroundColor(unsigned int mapId, unsigned char R,
                                    unsigned char G, unsigned char B,
                                    unsigned char A);
NGS_EXTERNC ngsRGBA ngsGetMapBackgroundColor(unsigned int mapId);
NGS_EXTERNC int ngsCreateLayer(unsigned int mapId, const char* name,
                               const char* path);

NGS_EXTERNC int ngsSetMapCenter(unsigned int mapId, const ngsRawPoint pt);
NGS_EXTERNC int ngsGetMapCenter(unsigned int mapId, ngsRawPoint* pt);
NGS_EXTERNC int ngsSetMapDisplayCenter(unsigned int mapId, const ngsRawPoint pt);
NGS_EXTERNC int ngsGetMapDisplayCenter(unsigned int mapId, ngsRawPoint* pt);

NGS_EXTERNC int ngsSetMapExtent(unsigned int mapId, const ngsRawEnvelope env);
NGS_EXTERNC int ngsGetMapExtent(unsigned int mapId, ngsRawEnvelope* env);

NGS_EXTERNC int ngsSetMapScale(unsigned int mapId, double scale);
//NGS_EXTERNC int ngsGetMapScale(unsigned int mapId, double* scale);

#endif // API_H
