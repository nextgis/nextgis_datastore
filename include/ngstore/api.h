/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
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

/* Spatial extent */
typedef struct _ngsExtent {
    double minX;
    double minY;
    double maxX;
    double maxY;
} ngsExtent;

/**
 * @brief Catalog object short information. Int type coded both
 * ngsCatalogObjectType and subtype (according to type).
 */

typedef void* CatalogObjectH;
typedef struct _ngsCatalogObjectInfo {
    const char* name;
    int type;
    CatalogObjectH object;
} ngsCatalogObjectInfo;

typedef struct _ngsURLRequestResult {
    int status;
    char** headers;
    unsigned char* data;
    int dataLen;
} ngsURLRequestResult;

typedef unsigned int ngsGeometryType;

/**
 * @brief Prototype of function, which executed periodically during some long
 * process.
 * @param status Task current status
 * @param complete Progress percent from 0 to 1
 * @param message Some user friendly message from task
 * @param progressArguments Some user data or null pointer
 * @return 1 to continue execute process or 0 - to cancel
 */
typedef int (*ngsProgressFunc)(enum ngsCode status, double complete,
                               const char* message, void* progressArguments);
/**
 * @brief Prototype of function, which executed when changes occurred.
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
NGS_EXTERNC int ngsInit(char** options);
NGS_EXTERNC void ngsUnInit();
NGS_EXTERNC void ngsFreeResources(char full);
NGS_EXTERNC const char* ngsGetLastErrorMessage();
NGS_EXTERNC void ngsAddNotifyFunction(ngsNotifyFunc function, int notifyTypes);
NGS_EXTERNC void ngsRemoveNotifyFunction(ngsNotifyFunc function);
NGS_EXTERNC const char* ngsSettingsGetString(const char* key, const char* defaultVal);
NGS_EXTERNC void ngsSettingsSetString(const char* key, const char* value);

/**
 * Proxy to GDAL functions
 */

NGS_EXTERNC const char* ngsGetCurrentDirectory();
NGS_EXTERNC char** ngsAddNameValue(char** list, const char* name,
                                   const char* value);
NGS_EXTERNC void ngsListFree(char** list);
NGS_EXTERNC const char* ngsFormFileName(const char* path, const char* name,
                                        const char* extension);
NGS_EXTERNC void ngsFree(void* pointer);

/**
 * Miscellaneous functions
 */
typedef void* JsonDocumentH;
typedef void* JsonObjectH;
NGS_EXTERNC ngsURLRequestResult* ngsURLRequest(enum ngsURLRequestType type,
                                              const char* url,
                                              char** options);
NGS_EXTERNC ngsURLRequestResult* ngsURLUploadFile(const char* path,
                                                  const char* url,
                                                  char** options,
                                                  ngsProgressFunc callback,
                                                  void* callbackData);
NGS_EXTERNC void ngsURLRequestResultFree(ngsURLRequestResult* result);
NGS_EXTERNC int ngsURLAuthAdd(const char* url, char** options);
NGS_EXTERNC char** ngsURLAuthGet(const char* url);
NGS_EXTERNC int ngsURLAuthDelete(const char* url);
NGS_EXTERNC const char* ngsMD5(const char* value);

NGS_EXTERNC JsonDocumentH ngsJsonDocumentCreate();
NGS_EXTERNC void ngsJsonDocumentFree(JsonDocumentH document);
NGS_EXTERNC int ngsJsonDocumentLoadUrl(JsonDocumentH document, const char* url,
                                       char** options,
                                       ngsProgressFunc callback,
                                       void* callbackData);
NGS_EXTERNC JsonObjectH ngsJsonDocumentRoot(JsonDocumentH document);
NGS_EXTERNC void ngsJsonObjectFree(JsonObjectH object);
NGS_EXTERNC int ngsJsonObjectType(JsonObjectH object);
NGS_EXTERNC int ngsJsonObjectValid(JsonObjectH object);
NGS_EXTERNC const char* ngsJsonObjectName(JsonObjectH object);
NGS_EXTERNC JsonObjectH* ngsJsonObjectChildren(JsonObjectH object);
NGS_EXTERNC void ngsJsonObjectChildrenListFree(JsonObjectH* list);
NGS_EXTERNC const char* ngsJsonObjectGetString(JsonObjectH object,
                                               const char* defaultValue);
NGS_EXTERNC double ngsJsonObjectGetDouble(JsonObjectH object,
                                          double defaultValue);
NGS_EXTERNC int ngsJsonObjectGetInteger(JsonObjectH object, int defaultValue);
NGS_EXTERNC long ngsJsonObjectGetLong(JsonObjectH object, long defaultValue);
NGS_EXTERNC int ngsJsonObjectGetBool(JsonObjectH object, int defaultValue);
NGS_EXTERNC JsonObjectH ngsJsonObjectGetArray(JsonObjectH object,
                                              const char* name);
NGS_EXTERNC JsonObjectH ngsJsonObjectGetObject(JsonObjectH object,
                                               const char* name);
NGS_EXTERNC int ngsJsonArraySize(JsonObjectH object);
NGS_EXTERNC JsonObjectH ngsJsonArrayItem(JsonObjectH object, int index);
NGS_EXTERNC const char* ngsJsonObjectGetStringForKey(JsonObjectH object,
                                                     const char* name,
                                                     const char* defaultValue);
NGS_EXTERNC double ngsJsonObjectGetDoubleForKey(JsonObjectH object,
                                                const char* name,
                                                double defaultValue);
NGS_EXTERNC int ngsJsonObjectGetIntegerForKey(JsonObjectH object,
                                              const char* name,
                                              int defaultValue);
NGS_EXTERNC long ngsJsonObjectGetLongForKey(JsonObjectH object,
                                            const char* name, long defaultValue);
NGS_EXTERNC int ngsJsonObjectGetBoolForKey(JsonObjectH object,
                                           const char* name, int defaultValue);
NGS_EXTERNC int ngsJsonObjectSetStringForKey(JsonObjectH object,
                                             const char* name,
                                             const char* value);
NGS_EXTERNC int ngsJsonObjectSetDoubleForKey(JsonObjectH object,
                                             const char* name, double value);
NGS_EXTERNC int ngsJsonObjectSetIntegerForKey(JsonObjectH object,
                                              const char* name, int value);
NGS_EXTERNC int ngsJsonObjectSetLongForKey(JsonObjectH object,
                                           const char* name, long value);
NGS_EXTERNC int ngsJsonObjectSetBoolForKey(JsonObjectH object,
                                           const char* name, int value);

/**
 * Catalog functions
 */

NGS_EXTERNC const char* ngsCatalogPathFromSystem(const char* path);
NGS_EXTERNC CatalogObjectH ngsCatalogObjectGet(const char* path);
NGS_EXTERNC ngsCatalogObjectInfo* ngsCatalogObjectQuery(CatalogObjectH object,
                                                        int filter);
NGS_EXTERNC ngsCatalogObjectInfo*  ngsCatalogObjectQueryMultiFilter(
        CatalogObjectH object, int* filters, int filterCount);
NGS_EXTERNC int ngsCatalogObjectDelete(CatalogObjectH object);
NGS_EXTERNC int ngsCatalogObjectCreate(CatalogObjectH object, const char* name,
                                       char** options);
NGS_EXTERNC int ngsCatalogObjectCopy(CatalogObjectH srcObject,
                                     CatalogObjectH dstObjectContainer,
                                     char** options,
                                     ngsProgressFunc callback,
                                     void* callbackData);
NGS_EXTERNC int ngsCatalogObjectRename(CatalogObjectH object,
                                       const char* newName);
NGS_EXTERNC const char* ngsCatalogObjectOptions(CatalogObjectH object,
                                                int optionType);
NGS_EXTERNC enum ngsCatalogObjectType ngsCatalogObjectType(
        CatalogObjectH object);
NGS_EXTERNC const char* ngsCatalogObjectName(CatalogObjectH object);
NGS_EXTERNC char** ngsCatalogObjectMetadata(CatalogObjectH object,
                                            const char* domain);
NGS_EXTERNC int ngsCatalogObjectSetMetadataItem(CatalogObjectH object,
                                                const char* name,
                                                const char* value,
                                                const char* domain);
NGS_EXTERNC void ngsCatalogObjectRefresh(CatalogObjectH object);

/**
 * Feature class
 */

typedef void* FeatureH;
typedef void* GeometryH;
typedef void* CoordinateTransformationH;
typedef struct _ngsField {
    const char* name;
    const char* alias;
    int type;
} ngsField;

typedef struct _ngsEditOperation {
    long long fid;
    long long aid;
    enum ngsChangeCode code;
    long long rid;
    long long arid;
} ngsEditOperation;

NGS_EXTERNC int ngsDatasetOpen(CatalogObjectH object, unsigned int openFlags,
                               char** openOptions);
NGS_EXTERNC char ngsDatasetIsOpened(CatalogObjectH object);
NGS_EXTERNC int ngsDatasetClose(CatalogObjectH object);

NGS_EXTERNC ngsField* ngsFeatureClassFields(CatalogObjectH object);
NGS_EXTERNC ngsGeometryType ngsFeatureClassGeometryType(CatalogObjectH object);
NGS_EXTERNC int ngsFeatureClassCreateOverviews(CatalogObjectH object,
                                               char** options,
                                               ngsProgressFunc callback,
                                               void* callbackData);
NGS_EXTERNC FeatureH ngsFeatureClassCreateFeature(CatalogObjectH object);
NGS_EXTERNC void ngsFeatureClassBatchMode(CatalogObjectH object, char enable);
NGS_EXTERNC int ngsFeatureClassInsertFeature(CatalogObjectH object,
                                             FeatureH feature, char logEdits);
NGS_EXTERNC int ngsFeatureClassUpdateFeature(CatalogObjectH object,
                                             FeatureH feature, char logEdits);
NGS_EXTERNC int ngsFeatureClassDeleteFeature(CatalogObjectH object, long long id,
                                             char logEdits);
NGS_EXTERNC int ngsFeatureClassDeleteFeatures(CatalogObjectH object, char logEdits);
NGS_EXTERNC long long ngsFeatureClassCount(CatalogObjectH object);
NGS_EXTERNC void ngsFeatureClassResetReading(CatalogObjectH object);
NGS_EXTERNC FeatureH ngsFeatureClassNextFeature(CatalogObjectH object);
NGS_EXTERNC FeatureH ngsFeatureClassGetFeature(CatalogObjectH object,
                                               long long id);
NGS_EXTERNC int ngsFeatureClassSetFilter(CatalogObjectH object,
                                         GeometryH geometryFilter,
                                         const char* attributeFilter);
NGS_EXTERNC int ngsFeatureClassSetSpatialFilter(CatalogObjectH object,
                                                double minX, double minY,
                                                double maxX, double maxY);
NGS_EXTERNC int ngsFeatureClassDeleteEditOperation(CatalogObjectH object,
                                                  ngsEditOperation operation);
NGS_EXTERNC ngsEditOperation* ngsFeatureClassGetEditOperations(CatalogObjectH object);

NGS_EXTERNC void ngsFeatureFree(FeatureH feature);
NGS_EXTERNC int ngsFeatureFieldCount(FeatureH feature);
NGS_EXTERNC int ngsFeatureIsFieldSet(FeatureH feature, int fieldIndex);
NGS_EXTERNC long long ngsFeatureGetId(FeatureH feature);
NGS_EXTERNC GeometryH ngsFeatureGetGeometry(FeatureH feature);
NGS_EXTERNC int ngsFeatureGetFieldAsInteger(FeatureH feature, int field);
NGS_EXTERNC double ngsFeatureGetFieldAsDouble(FeatureH feature, int field);
NGS_EXTERNC const char* ngsFeatureGetFieldAsString(FeatureH feature, int field);
NGS_EXTERNC int ngsFeatureGetFieldAsDateTime(FeatureH feature, int field,
                                             int* year, int* month, int* day,
                                             int* hour, int* minute,
                                             float* second, int* TZFlag);

NGS_EXTERNC void ngsFeatureSetGeometry(FeatureH feature, GeometryH geometry);
NGS_EXTERNC void ngsFeatureSetFieldInteger(FeatureH feature, int field,
                                           int value);
NGS_EXTERNC void ngsFeatureSetFieldDouble(FeatureH feature, int field,
                                          double value);
NGS_EXTERNC void ngsFeatureSetFieldString(FeatureH feature, int field,
                                          const char* value);
NGS_EXTERNC void ngsFeatureSetFieldDateTime(FeatureH feature, int field, int year,
                                            int month, int day, int hour,
                                            int minute, float second, int TZFlag);

NGS_EXTERNC FeatureH ngsStoreFeatureClassGetFeatureByRemoteId(
        CatalogObjectH object, long long rid);
NGS_EXTERNC long long ngsStoreFeatureGetRemoteId(FeatureH feature);
NGS_EXTERNC void ngsStoreFeatureSetRemoteId(FeatureH feature, long long rid);


NGS_EXTERNC GeometryH ngsFeatureCreateGeometry(FeatureH feature);
NGS_EXTERNC GeometryH ngsFeatureCreateGeometryFromJson(JsonObjectH geometry);
NGS_EXTERNC void ngsGeometryFree(GeometryH geometry);
NGS_EXTERNC void ngsGeometrySetPoint(GeometryH geometry, int point, double x,
                                     double y, double z, double m);
NGS_EXTERNC ngsExtent ngsGeometryGetEnvelope(GeometryH geometry);
NGS_EXTERNC int ngsGeometryTransformTo(GeometryH geometry, int EPSG);
NGS_EXTERNC int ngsGeometryTransform(GeometryH geometry,
                                     CoordinateTransformationH ct);
NGS_EXTERNC char ngsGeometryIsEmpty(GeometryH geometry);
NGS_EXTERNC ngsGeometryType ngsGeometryGetType(GeometryH geometry);
NGS_EXTERNC const char* ngsGeometryToJson(GeometryH geometry);

NGS_EXTERNC CoordinateTransformationH ngsCoordinateTransformationCreate(
        int fromEPSG, int toEPSG);
NGS_EXTERNC void ngsCoordinateTransformationFree(CoordinateTransformationH ct);
NGS_EXTERNC ngsCoordinate ngsCoordinateTransformationDo(
        CoordinateTransformationH ct, ngsCoordinate coordinates);

typedef struct _ngsFeatureAttachmentInfo {
    long long id;
    const char* name;
    const char* description;
    const char* path;
    long long size;
    long long rid;
} ngsFeatureAttachmentInfo;

NGS_EXTERNC long long ngsFeatureAttachmentAdd(FeatureH feature,
                                              const char* name,
                                              const char* description,
                                              const char* path,
                                              char** options,
                                              char logEdits);
NGS_EXTERNC int ngsFeatureAttachmentDelete(FeatureH feature, long long aid,
                                           char logEdits);
NGS_EXTERNC int ngsFeatureAttachmentDeleteAll(FeatureH feature, char logEdits);
NGS_EXTERNC ngsFeatureAttachmentInfo* ngsFeatureAttachmentsGet(FeatureH feature);
NGS_EXTERNC int ngsFeatureAttachmentUpdate(FeatureH feature,
                                           long long aid,
                                           const char* name,
                                           const char* description,
                                           char logEdits);
NGS_EXTERNC void ngsStoreFeatureSetAttachmentRemoteId(FeatureH feature,
                                                      long long aid,
                                                      long long rid);

//NGS_EXTERNC const char* ngsDataStoreGetOptions(ngsDataStoreOptionsTypes optionType);

/**
  * Raster
  */
NGS_EXTERNC int ngsRasterCacheArea(CatalogObjectH object, char** options,
                                   ngsProgressFunc callback, void* callbackData);

/**
 * Map functions
 *
 *  ngsCreateMap -> ngsInitMap -> ngsSaveMap [optional]
 *  ngsLoadMap -> ngsInitMap -> ngsSaveMap [optional]
 */

typedef void* LayerH;
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
NGS_EXTERNC int ngsMapInvalidate(unsigned char mapId, ngsExtent bounds);
NGS_EXTERNC int ngsMapSetBackgroundColor(unsigned char mapId, const ngsRGBA color);
NGS_EXTERNC ngsRGBA ngsMapGetBackgroundColor(unsigned char mapId);
NGS_EXTERNC int ngsMapSetCenter(unsigned char mapId, double x, double y);
NGS_EXTERNC ngsCoordinate ngsMapGetCenter(unsigned char mapId);
NGS_EXTERNC ngsCoordinate ngsMapGetCoordinate(unsigned char mapId, double x,
                                              double y);
NGS_EXTERNC ngsCoordinate ngsMapGetDistance(unsigned char mapId, double w,
                                            double h);
NGS_EXTERNC int ngsMapSetRotate(unsigned char mapId, enum ngsDirection dir,
                                double rotate);
NGS_EXTERNC double ngsMapGetRotate(unsigned char mapId, enum ngsDirection dir);
NGS_EXTERNC int ngsMapSetScale(unsigned char mapId, double scale);
NGS_EXTERNC double ngsMapGetScale(unsigned char mapId);

NGS_EXTERNC int ngsMapSetOptions(unsigned char mapId, char** options);
NGS_EXTERNC int ngsMapSetExtentLimits(unsigned char mapId,
                                      double minX, double minY,
                                      double maxX, double maxY);
NGS_EXTERNC ngsExtent ngsMapGetExtent(unsigned char mapId, int epsg);
NGS_EXTERNC int ngsMapSetExtent(unsigned char mapId, ngsExtent extent);

//NGS_EXTERNC void ngsMapSetLocation(unsigned char mapId, double x, double y, double azimuth);
NGS_EXTERNC JsonObjectH ngsMapGetSelectionStyle(unsigned char mapId,
                                                enum ngsStyleType styleType);
NGS_EXTERNC int ngsMapSetSelectionsStyle(unsigned char mapId,
                                         enum ngsStyleType styleType,
                                         JsonObjectH style);
NGS_EXTERNC const char* ngsMapGetSelectionStyleName(unsigned char mapId,
                                                    enum ngsStyleType styleType);
NGS_EXTERNC int ngsMapSetSelectionStyleName(unsigned char mapId,
                                            enum ngsStyleType styleType,
                                            const char* name);
NGS_EXTERNC int ngsMapIconSetAdd(unsigned char mapId, const char* name,
                                 const char* path, char ownByMap);
NGS_EXTERNC int ngsMapIconSetRemove(unsigned char mapId, const char* name);
NGS_EXTERNC char ngsMapIconSetExists(unsigned char mapId, const char* name);

/**
 * Layer functions
 */

NGS_EXTERNC const char* ngsLayerGetName(LayerH layer);
NGS_EXTERNC int ngsLayerSetName(LayerH layer, const char* name);
NGS_EXTERNC char ngsLayerGetVisible(LayerH layer);
NGS_EXTERNC int ngsLayerSetVisible(LayerH layer, char visible);
NGS_EXTERNC CatalogObjectH ngsLayerGetDataSource(LayerH layer);
NGS_EXTERNC JsonObjectH ngsLayerGetStyle(LayerH layer);
NGS_EXTERNC int ngsLayerSetStyle(LayerH layer, JsonObjectH style);
NGS_EXTERNC const char* ngsLayerGetStyleName(LayerH layer);
NGS_EXTERNC int ngsLayerSetStyleName(LayerH layer, const char* name);
NGS_EXTERNC int ngsLayerSetSelectionIds(LayerH layer, long long *ids, int size);
NGS_EXTERNC int ngsLayerSetHideIds(LayerH layer, long long *ids, int size);

/**
 * Overlay functions
 */

NGS_EXTERNC int ngsOverlaySetVisible(unsigned char mapId, int typeMask,
                                     char visible);
NGS_EXTERNC char ngsOverlayGetVisible(unsigned char mapId,
                                      enum ngsMapOverlayType type);
NGS_EXTERNC int ngsOverlaySetOptions(unsigned char mapId,
                                     enum ngsMapOverlayType type, char** options);
NGS_EXTERNC char** ngsOverlayGetOptions(unsigned char mapId,
                                        enum ngsMapOverlayType type);
/* Edit */
typedef struct _ngsPointId
{
    int pointId;
    char isHole;
} ngsPointId;

NGS_EXTERNC ngsPointId ngsEditOverlayTouch(unsigned char mapId, double x,
                                           double y, enum ngsMapTouchType type);
NGS_EXTERNC char ngsEditOverlayUndo(unsigned char mapId);
NGS_EXTERNC char ngsEditOverlayRedo(unsigned char mapId);
NGS_EXTERNC char ngsEditOverlayCanUndo(unsigned char mapId);
NGS_EXTERNC char ngsEditOverlayCanRedo(unsigned char mapId);
NGS_EXTERNC FeatureH ngsEditOverlaySave(unsigned char mapId);
NGS_EXTERNC int ngsEditOverlayCancel(unsigned char mapId);
NGS_EXTERNC int ngsEditOverlayCreateGeometryInLayer(unsigned char mapId,
                                                    LayerH layer, char empty);
NGS_EXTERNC int ngsEditOverlayCreateGeometry(unsigned char mapId,
                                             ngsGeometryType type);
NGS_EXTERNC int ngsEditOverlayEditGeometry(unsigned char mapId, LayerH layer,
                                           long long feateureId);
NGS_EXTERNC int ngsEditOverlayDeleteGeometry(unsigned char mapId);
NGS_EXTERNC int ngsEditOverlayAddPoint(unsigned char mapId);
NGS_EXTERNC int ngsEditOverlayAddVertex(unsigned char mapId,
                                        ngsCoordinate coordinates);
NGS_EXTERNC enum ngsEditDeleteType ngsEditOverlayDeletePoint(unsigned char mapId);
NGS_EXTERNC int ngsEditOverlayAddHole(unsigned char mapId);
NGS_EXTERNC enum ngsEditDeleteType ngsEditOverlayDeleteHole(unsigned char mapId);
NGS_EXTERNC int ngsEditOverlayAddGeometryPart(unsigned char mapId);
NGS_EXTERNC enum ngsEditDeleteType ngsEditOverlayDeleteGeometryPart(
        unsigned char mapId);
NGS_EXTERNC GeometryH ngsEditOverlayGetGeometry(unsigned char mapId);
NGS_EXTERNC int ngsEditOverlaySetStyle(unsigned char mapId,
                                       enum ngsEditStyleType type,
                                       JsonObjectH style);
NGS_EXTERNC int ngsEditOverlaySetStyleName(unsigned char mapId,
                                           enum ngsEditStyleType type,
                                           const char* name);
NGS_EXTERNC JsonObjectH ngsEditOverlayGetStyle(unsigned char mapId,
                                               enum ngsEditStyleType type);
NGS_EXTERNC void ngsEditOverlaySetWalkingMode(unsigned char mapId, char enable);
NGS_EXTERNC char ngsEditOverlayGetWalkingMode(unsigned char mapId);

/* Location */
NGS_EXTERNC int ngsLocationOverlayUpdate(unsigned char mapId,
                                         ngsCoordinate location,
                                         float direction, float accuracy);
NGS_EXTERNC int ngsLocationOverlaySetStyle(unsigned char mapId, JsonObjectH style);
NGS_EXTERNC int ngsLocationOverlaySetStyleName(unsigned char mapId,
                                               const char* name);
NGS_EXTERNC JsonObjectH ngsLocationOverlayGetStyle(unsigned char mapId);

//NGS_EXTERNC int ngsGraphicsOverlayDrawRectangle(unsigned char mapId, double minX, double minY, double maxX, double maxY);
//NGS_EXTERNC int ngsGraphicsOverlayDrawCircle(unsigned char mapId, double X, double Y, double radius);

///** Map canvas functions */
//NGS_EXTERNC ngsPosition ngsDisplayGetPosition(unsigned char mapId, double x, double y);
//NGS_EXTERNC ngsPosition ngsDisplayGetLength(unsigned char mapId, double w, double h);
#endif // NGSAPI_H
