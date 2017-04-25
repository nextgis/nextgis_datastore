/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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

#include "ngstore/common.h"
#include "ngstore/codes.h"

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

/**
 * @brief Catalog object short information. Int type coded both
 * ngsCatalogObjectType and subtype (according to type).
 */
typedef struct _ngsCatalogObjectInfo {
    const char* name;
    int type;
} ngsCatalogObjectInfo;

/**
 * @brief Prototype of function, which executed periodically during some long
 * process.
 * @param status Task current status
 * @param complete Progress percent from 0 to 1
 * @param message Some user friendly message from task
 * @param progressArguments Some user data or null pointer
 * @return 1 to continue execute process or 0 - to cancel
 */
typedef int (*ngsProgressFunc)(enum ngsErrorCode status,
                               double complete,
                               const char* message,
                               void* progressArguments);
/**
 * @brief Prototype of function, which executed when changes accured.
 * @param uri Catalog path (for features/rows ended with feature ID, for
 * attachments ended with attachments/{int:id}).
 * @param operation Operation which trigger notification.
 */
typedef void (*ngsNotifyFunc)(const char* uri, enum ngsChangeCode operation);

/**
 * Common functions
 */

NGS_EXTERNC int ngsGetVersion(const char* request);
NGS_EXTERNC const char* ngsGetVersionString(const char* request);
NGS_EXTERNC int ngsInit(char **options = nullptr);
NGS_EXTERNC void ngsUnInit();
NGS_EXTERNC void ngsFreeResources(bool full = false);
NGS_EXTERNC const char* ngsGetLastErrorMessage();
NGS_EXTERNC void ngsAddNotifyFunction(ngsNotifyFunc function, int notifyTypes);
NGS_EXTERNC void ngsRemoveNotifyFunction(ngsNotifyFunc function);

/**
 * Proxy to GDAL functions
 */

NGS_EXTERNC const char* ngsGetCurrentDirectory();
NGS_EXTERNC char** ngsAddNameValue(char** list, const char *name,
                                   const char *value);
NGS_EXTERNC void ngsDestroyList(char** list);
NGS_EXTERNC const char *ngsFormFileName(const char *path, const char *name,
                                        const char *extension);
NGS_EXTERNC void ngsFree(void *pointer);

/**
 * Catalog functions
 */
typedef void *CatalogObjectH;
NGS_EXTERNC ngsCatalogObjectInfo* ngsCatalogObjectQuery(const char* path,
                                                        int filter = 0);
NGS_EXTERNC int ngsCatalogObjectDelete(const char* path);
NGS_EXTERNC int ngsCatalogObjectCreate(const char* path, const char* name,
                                       char **options = nullptr);
NGS_EXTERNC const char* ngsCatalogPathFromSystem(const char* path);
NGS_EXTERNC int ngsCatalogObjectLoad(const char* srcPath, const char* dstPath,
                                     char **options = nullptr,
                                     ngsProgressFunc callback = nullptr,
                                     void* callbackData = nullptr);
NGS_EXTERNC int ngsCatalogObjectRename(const char* path, const char* newName);
NGS_EXTERNC const char* ngsCatalogObjectOptions(const char* path, int optionType);
NGS_EXTERNC CatalogObjectH ngsCatalogObjectGet(const char* path);
NGS_EXTERNC enum ngsCatalogObjectType ngsCatalogObjectType(CatalogObjectH object);


//NGS_EXTERNC int ngsCreateRemoteTMSRaster(const char* url, const char* name,
//                                         const char* alias, const char* copyright,
//                                         int epsg, int z_min, int z_max,
//                                         bool y_origin_top);
//NGS_EXTERNC const char* ngsDataStoreGetOptions(ngsDataStoreOptionsTypes optionType);

/**
 * Map functions
 *
 *  ngsCreateMap -> ngsInitMap -> ngsSaveMap [optional]
 *  ngsLoadMap -> ngsInitMap -> ngsSaveMap [optional]
 */

typedef void *LayerH;
/*
mapSet/GetProperties - scale, rotate
mapGet distance
dispalyGet position, length
*/
NGS_EXTERNC unsigned char ngsMapCreate(const char* name, const char* description,
                             unsigned short epsg, double minX, double minY,
                             double maxX, double maxY);
NGS_EXTERNC unsigned char ngsMapOpen(const char* path);
NGS_EXTERNC int ngsMapSave(unsigned char mapId, const char* path);
NGS_EXTERNC int ngsMapClose(unsigned char mapId);
NGS_EXTERNC int ngsMapLayerCount(unsigned char mapId);
NGS_EXTERNC int ngsMapCreateLayer(unsigned char mapId, const char* name,
                                  const char* path);
NGS_EXTERNC LayerH ngsMapLayerGet(unsigned char mapId, int layerId);
NGS_EXTERNC int ngsMapLayerDelete(unsigned char mapId, LayerH layer);
NGS_EXTERNC int ngsMapLayerReorder(unsigned char mapId, LayerH beforeLayer,
                                   LayerH movedLayer);
NGS_EXTERNC int ngsMapSetSize(unsigned char mapId, int width, int height,
                           int YAxisInverted);
NGS_EXTERNC int ngsMapDraw(unsigned char mapId, enum ngsDrawState state,
                           ngsProgressFunc callback, void* callbackData);
NGS_EXTERNC int ngsMapSetBackgroundColor(unsigned char mapId, const ngsRGBA &color);
NGS_EXTERNC ngsRGBA ngsMapGetBackgroundColor(unsigned char mapId);
NGS_EXTERNC int ngsMapSetCenter(unsigned char mapId, double x, double y);
NGS_EXTERNC ngsCoordinate ngsMapGetCenter(unsigned char mapId);
NGS_EXTERNC ngsCoordinate ngsMapGetCoordinate(unsigned char mapId, double x,
                                              double y);

/**
 * Layer functions
 */
NGS_EXTERNC const char* ngsLayerGetName(LayerH layer);
NGS_EXTERNC int ngsLayerSetName(LayerH layer, const char* name);



//NGS_EXTERNC int ngsMapSetScale(unsigned char mapId, double scale);
//NGS_EXTERNC double ngsMapGetScale(unsigned char mapId);
//NGS_EXTERNC int ngsMapSetRotate(unsigned char mapId, enum ngsDirection dir,
//                                double rotate);
//NGS_EXTERNC double ngsMapGetRotate(unsigned char mapId, enum ngsDirection dir);
//NGS_EXTERNC ngsCoordinate ngsMapGetDistance(unsigned char mapId, double w, double h);
///** Map canvas functions */
//NGS_EXTERNC ngsPosition ngsDisplayGetPosition(unsigned char mapId, double x, double y);
//NGS_EXTERNC ngsPosition ngsDisplayGetLength(unsigned char mapId, double w, double h);

#endif // NGSAPI_H
