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
    INVALID_PATH,       /**< path is invalid */
    UNSUPPORTED,        /**< the feature is unsupported */
    CREATE_DB_FAILED,   /**< Create database failed */
    CREATE_DIR_FAILED,  /**< Create directory failed */
    CREATE_TABLE_FAILED,/**< Create table failed */
    CREATE_MAP_FAILED,  /**< Create map failed */
    CREATE_FAILED,      /**< Create failed */
    DELETE_FAILED,      /**< Faild to delete file, folder or something else */
    SAVE_FAILED,        /**< Faild to save file, folder or something else */
    INVALID_STUCTURE,   /**< Invalid storage, file etc. structure */
    INSERT_FAILED,      /**< insert new feature failed */
    UPDATE_FAILED,      /**< update feature failed */
    GL_GET_DISPLAY_FAILED,  /**< Get OpenGL display failed */
    INIT_FAILED,        /**< Initialise failed */
    INVALID_MAP         /**< Invalid map */

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
    MAP_STORE
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

/**
 * Common functions
 */
NGS_EXTERNC int ngsGetVersion(const char* request);
NGS_EXTERNC const char* ngsGetVersionString(const char* request);
NGS_EXTERNC int ngsInit(const char* path, const char* dataPath, const char* cachePath);
NGS_EXTERNC void ngsUninit();
NGS_EXTERNC int ngsDestroy(const char* path, const char* cachePath);
NGS_EXTERNC void ngsOnLowMemory();
NGS_EXTERNC void ngsOnPause();
NGS_EXTERNC void ngsSetNotifyFunction(ngsNotifyFunc callback);
/**
 * Storage functions
 */
NGS_EXTERNC int ngsCreateRemoteTMSRaster(const char* url, const char* name,
                                         const char* alias, const char* copyright,
                                         int epsg, int z_min, int z_max,
                                         bool y_origin_top);
NGS_EXTERNC int ngsLoadRaster(const char* path, const char* name,
                              const char* alias, bool move);
/**
 * Map functions
 */
NGS_EXTERNC int ngsInitMap(const char* name, void* buffer, int width, int height);
NGS_EXTERNC int ngsDrawMap(const char* name, ngsProgressFunc callback, 
                           void* callbackData);
NGS_EXTERNC int ngsSetMapBackgroundColor(const char* name, unsigned char R,
                                    unsigned char G, unsigned char B,
                                    unsigned char A);
NGS_EXTERNC ngsRGBA ngsGetMapBackgroundColor(const char* name);

/**
  * useful functions
  */
inline int ngsRGBA2HEX(const ngsRGBA& color){
    return ((color.R & 0xff) << 24) + ((color.G & 0xff) << 16) +
            ((color.B & 0xff) << 8) + (color.A & 0xff);
}

inline ngsRGBA ngsHEX2RGBA(int color){
    ngsRGBA out;
    out.R = (color >> 24) & 0xff;
    out.G = (color >> 16) & 0xff;
    out.B = (color >> 8) & 0xff;
    out.A = (color) & 0xff;
    return out;
}

#endif // API_H
