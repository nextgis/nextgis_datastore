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
    UNSUPPORTED_GDAL_DRIVER, /**< the gdal driver is unsupported */
    CREATE_DB_FAILED,   /**< Create database failed */
    CREATE_DIR_FAILED,  /**< Create directory failed */
    CREATE_TABLE_FAILED,/**< Create table failed */
    CREATE_MAP_FAILED,  /**< Create map failed */
    CREATE_FAILED,      /**< Create failed */
    DELETE_FAILED,      /**< Faild to delete file, folder or something else */
    SAVE_FAILED,        /**< Faild to save file, folder or something else */
    INVALID_DB_STUCTURE,/**< Invalid storage structure */
    INSERT_FAILED,      /**< insert new feature failed */
    UPDATE_FAILED       /**< update feature failed */
};


NGS_EXTERNC int ngsGetVersion(const char* request);
NGS_EXTERNC const char* ngsGetVersionString(const char* request);
NGS_EXTERNC int ngsInit(const char* path, const char* dataPath, const char* cachePath);
NGS_EXTERNC void ngsUninit();
NGS_EXTERNC int ngsDestroy(const char* path, const char* cachePath);
NGS_EXTERNC int ngsCreateRemoteTMSRaster(const char* url, const char* name,
                                         const char* alias, const char* copyright,
                                         int epsg, int z_min, int z_max,
                                         bool y_origin_top);
NGS_EXTERNC int ngsLoadRaster(const char* path, const char* name,
                              const char* alias, bool move);

#endif // API_H
