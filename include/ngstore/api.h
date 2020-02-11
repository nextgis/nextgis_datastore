/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
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

typedef void *CatalogObjectH;
typedef struct _ngsCatalogObjectInfo {
    const char *name;
    int type;
    CatalogObjectH object;
} ngsCatalogObjectInfo;

typedef struct _ngsURLRequestResult {
    int status;
    char **headers;
    unsigned char *data;
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
                               const char *message, void *progressArguments);
/**
 * @brief Prototype of function, which executed when changes occurred.
 * @param uri Catalog path (for features/rows ended with feature ID, for
 * attachments ended with attachments/{int:id}).
 * @param operation Operation which trigger notification.
 */
typedef void (*ngsNotifyFunc)(const char *uri, enum ngsChangeCode operation);

/*
 * Common functions
 */

NGS_EXTERNC int ngsGetVersion(const char *request);
NGS_EXTERNC const char *ngsGetVersionString(const char *request);
NGS_EXTERNC int ngsInit(char **options);
NGS_EXTERNC void ngsUnInit();
NGS_EXTERNC void ngsFreeResources(char full);
NGS_EXTERNC const char *ngsGetLastErrorMessage();
NGS_EXTERNC void ngsAddNotifyFunction(ngsNotifyFunc function, int notifyTypes);
NGS_EXTERNC void ngsRemoveNotifyFunction(ngsNotifyFunc function);
NGS_EXTERNC const char *ngsSettingsGetString(const char *key, const char *defaultVal);
NGS_EXTERNC void ngsSettingsSetString(const char *key, const char *value);
NGS_EXTERNC int ngsBackup(const char *name, CatalogObjectH dstObjectContainer,
        CatalogObjectH *objects, ngsProgressFunc callback, void *callbackData);

/*
 * Proxy to GDAL functions
 */

NGS_EXTERNC const char *ngsGetCurrentDirectory();
NGS_EXTERNC char **ngsListAddNameValue(char **list, const char *name,
                                   const char *value);
NGS_EXTERNC char **ngsListAddNameIntValue(char **list, const char *name, int value);
NGS_EXTERNC void ngsListFree(char **list);
NGS_EXTERNC const char *ngsFormFileName(const char *path, const char *name,
                                        const char *extension);
NGS_EXTERNC void ngsFree(void *pointer);

/*
 * Miscellaneous functions
 */
typedef void *JsonDocumentH;
typedef void *JsonObjectH;
NGS_EXTERNC ngsURLRequestResult *ngsURLRequest(enum ngsURLRequestType type,
                                              const char *url,
                                              char **options,
                                               ngsProgressFunc callback,
                                               void *callbackData);
NGS_EXTERNC ngsURLRequestResult *ngsURLUploadFile(const char *path,
                                                  const char *url,
                                                  char **options,
                                                  ngsProgressFunc callback,
                                                  void *callbackData);
NGS_EXTERNC void ngsURLRequestResultFree(ngsURLRequestResult *result);
NGS_EXTERNC int ngsURLAuthAdd(const char *url, char **options);
NGS_EXTERNC char **ngsURLAuthGet(const char *url);
NGS_EXTERNC int ngsURLAuthDelete(const char *url);
NGS_EXTERNC const char *ngsMD5(const char *value);
NGS_EXTERNC const char *ngsGetDeviceId(bool regenerate);
NGS_EXTERNC const char *ngsGeneratePrivateKey();
NGS_EXTERNC const char *ngsEncryptString(const char *text);
NGS_EXTERNC const char *ngsDecryptString(const char *text);

NGS_EXTERNC JsonDocumentH ngsJsonDocumentCreate();
NGS_EXTERNC void ngsJsonDocumentFree(JsonDocumentH document);
NGS_EXTERNC int ngsJsonDocumentLoadUrl(JsonDocumentH document, const char *url,
                                       char **options,
                                       ngsProgressFunc callback,
                                       void *callbackData);
NGS_EXTERNC JsonObjectH ngsJsonDocumentRoot(JsonDocumentH document);
NGS_EXTERNC void ngsJsonObjectFree(JsonObjectH object);
NGS_EXTERNC int ngsJsonObjectType(JsonObjectH object);
NGS_EXTERNC char ngsJsonObjectValid(JsonObjectH object);
NGS_EXTERNC const char *ngsJsonObjectName(JsonObjectH object);
NGS_EXTERNC JsonObjectH *ngsJsonObjectChildren(JsonObjectH object);
NGS_EXTERNC void ngsJsonObjectChildrenListFree(JsonObjectH *list);
NGS_EXTERNC const char *ngsJsonObjectGetString(JsonObjectH object,
                                               const char *defaultValue);
NGS_EXTERNC double ngsJsonObjectGetDouble(JsonObjectH object,
                                          double defaultValue);
NGS_EXTERNC int ngsJsonObjectGetInteger(JsonObjectH object, int defaultValue);
NGS_EXTERNC long ngsJsonObjectGetLong(JsonObjectH object, long defaultValue);
NGS_EXTERNC char ngsJsonObjectGetBool(JsonObjectH object, char defaultValue);
NGS_EXTERNC JsonObjectH ngsJsonObjectGetArray(JsonObjectH object,
                                              const char *name);
NGS_EXTERNC JsonObjectH ngsJsonObjectGetObject(JsonObjectH object,
                                               const char *name);
NGS_EXTERNC int ngsJsonArraySize(JsonObjectH object);
NGS_EXTERNC JsonObjectH ngsJsonArrayItem(JsonObjectH object, int index);
NGS_EXTERNC const char *ngsJsonObjectGetStringForKey(JsonObjectH object,
                                                     const char *name,
                                                     const char *defaultValue);
NGS_EXTERNC double ngsJsonObjectGetDoubleForKey(JsonObjectH object,
                                                const char *name,
                                                double defaultValue);
NGS_EXTERNC int ngsJsonObjectGetIntegerForKey(JsonObjectH object,
                                              const char *name,
                                              int defaultValue);
NGS_EXTERNC long ngsJsonObjectGetLongForKey(JsonObjectH object,
                                            const char *name, long defaultValue);
NGS_EXTERNC char ngsJsonObjectGetBoolForKey(JsonObjectH object,
                                           const char *name, char defaultValue);
NGS_EXTERNC char ngsJsonObjectSetStringForKey(JsonObjectH object,
                                             const char *name,
                                             const char *value);
NGS_EXTERNC char ngsJsonObjectSetDoubleForKey(JsonObjectH object,
                                             const char *name, double value);
NGS_EXTERNC char ngsJsonObjectSetIntegerForKey(JsonObjectH object,
                                              const char *name, int value);
NGS_EXTERNC char ngsJsonObjectSetLongForKey(JsonObjectH object,
                                           const char *name, long value);
NGS_EXTERNC char ngsJsonObjectSetBoolForKey(JsonObjectH object,
                                           const char *name, char value);

/*
 * Catalog functions
 */

NGS_EXTERNC const char *ngsCatalogPathFromSystem(const char *path);
NGS_EXTERNC CatalogObjectH ngsCatalogObjectGet(const char *path);
NGS_EXTERNC CatalogObjectH ngsCatalogObjectGetByName(CatalogObjectH parent, 
    const char *name, char fullMatch);
NGS_EXTERNC ngsCatalogObjectInfo *ngsCatalogObjectQuery(CatalogObjectH object,
    int filter);
NGS_EXTERNC ngsCatalogObjectInfo *ngsCatalogObjectQueryMultiFilter(
    CatalogObjectH object, int *filters, int filterCount);
NGS_EXTERNC int ngsCatalogObjectDelete(CatalogObjectH object);
NGS_EXTERNC char ngsCatalogObjectCanCreate(CatalogObjectH object,
    enum ngsCatalogObjectType type);
NGS_EXTERNC CatalogObjectH ngsCatalogObjectCreate(CatalogObjectH object, 
    const char *name, char **options);
NGS_EXTERNC int ngsCatalogObjectCopy(CatalogObjectH srcObject,
    CatalogObjectH dstObjectContainer, char **options, ngsProgressFunc callback,
    void *callbackData);
NGS_EXTERNC int ngsCatalogObjectRename(CatalogObjectH object,
    const char *newName);
NGS_EXTERNC const char *ngsCatalogObjectOptions(CatalogObjectH object,
    int optionType);
NGS_EXTERNC enum ngsCatalogObjectType ngsCatalogObjectType(
    CatalogObjectH object);
NGS_EXTERNC const char *ngsCatalogObjectName(CatalogObjectH object);
NGS_EXTERNC const char *ngsCatalogObjectPath(CatalogObjectH object);
NGS_EXTERNC char **ngsCatalogObjectProperties(CatalogObjectH object,
    const char *domain);
NGS_EXTERNC const char *ngsCatalogObjectProperty(CatalogObjectH object, 
    const char *name, const char *defaultValue, const char *domain);
NGS_EXTERNC int ngsCatalogObjectSetProperty(CatalogObjectH object,
    const char *name, const char *value, const char *domain);
NGS_EXTERNC void ngsCatalogObjectRefresh(CatalogObjectH object);
NGS_EXTERNC char ngsCatalogCheckConnection(enum ngsCatalogObjectType type,
    char **options);
NGS_EXTERNC char ngsCatalogObjectOpen(CatalogObjectH object, char **openOptions);
NGS_EXTERNC char ngsCatalogObjectIsOpened(CatalogObjectH object);
NGS_EXTERNC char ngsCatalogObjectClose(CatalogObjectH object);

/*
 * Feature class
 */

typedef void *FeatureH;
typedef void *GeometryH;
typedef void *CoordinateTransformationH;
typedef struct _ngsField {
    const char *name;
    const char *alias;
    int type;
} ngsField;

typedef struct _ngsEditOperation {
    long long fid;
    long long aid;
    enum ngsChangeCode code;
    long long rid;
    long long arid;
} ngsEditOperation;

NGS_EXTERNC ngsField *ngsFeatureClassFields(CatalogObjectH object);
NGS_EXTERNC ngsGeometryType ngsFeatureClassGeometryType(CatalogObjectH object);
NGS_EXTERNC int ngsFeatureClassCreateOverviews(CatalogObjectH object,
                                               char **options,
                                               ngsProgressFunc callback,
                                               void *callbackData);
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
                                         const char *attributeFilter);
NGS_EXTERNC int ngsFeatureClassSetSpatialFilter(CatalogObjectH object,
                                                double minX, double minY,
                                                double maxX, double maxY);
NGS_EXTERNC int ngsFeatureClassDeleteEditOperation(CatalogObjectH object,
                                                  ngsEditOperation operation);
NGS_EXTERNC ngsEditOperation *ngsFeatureClassGetEditOperations(CatalogObjectH object);

NGS_EXTERNC void ngsFeatureFree(FeatureH feature);
NGS_EXTERNC int ngsFeatureFieldCount(FeatureH feature);
NGS_EXTERNC char ngsFeatureIsFieldSet(FeatureH feature, int fieldIndex);
NGS_EXTERNC long long ngsFeatureGetId(FeatureH feature);
NGS_EXTERNC GeometryH ngsFeatureGetGeometry(FeatureH feature);
NGS_EXTERNC int ngsFeatureGetFieldAsInteger(FeatureH feature, int field);
NGS_EXTERNC double ngsFeatureGetFieldAsDouble(FeatureH feature, int field);
NGS_EXTERNC const char *ngsFeatureGetFieldAsString(FeatureH feature, int field);
NGS_EXTERNC int ngsFeatureGetFieldAsDateTime(FeatureH feature, int field,
                                             int *year, int *month, int *day,
                                             int *hour, int *minute,
                                             float *second, int *TZFlag);

NGS_EXTERNC void ngsFeatureSetGeometry(FeatureH feature, GeometryH geometry);
NGS_EXTERNC void ngsFeatureSetFieldInteger(FeatureH feature, int field,
                                           int value);
NGS_EXTERNC void ngsFeatureSetFieldDouble(FeatureH feature, int field,
                                          double value);
NGS_EXTERNC void ngsFeatureSetFieldString(FeatureH feature, int field,
                                          const char *value);
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
NGS_EXTERNC const char *ngsGeometryToJson(GeometryH geometry);

NGS_EXTERNC CoordinateTransformationH ngsCoordinateTransformationCreate(
        int fromEPSG, int toEPSG);
NGS_EXTERNC void ngsCoordinateTransformationFree(CoordinateTransformationH ct);
NGS_EXTERNC ngsCoordinate ngsCoordinateTransformationDo(
        CoordinateTransformationH ct, ngsCoordinate coordinates);

typedef struct _ngsFeatureAttachmentInfo {
    long long id;
    const char *name;
    const char *description;
    const char *path;
    long long size;
    long long rid;
} ngsFeatureAttachmentInfo;

NGS_EXTERNC long long ngsFeatureAttachmentAdd(FeatureH feature,
                                              const char *name,
                                              const char *description,
                                              const char *path,
                                              char **options,
                                              char logEdits);
NGS_EXTERNC int ngsFeatureAttachmentDelete(FeatureH feature, long long aid,
                                           char logEdits);
NGS_EXTERNC int ngsFeatureAttachmentDeleteAll(FeatureH feature, char logEdits);
NGS_EXTERNC ngsFeatureAttachmentInfo *ngsFeatureAttachmentsGet(FeatureH feature);
NGS_EXTERNC int ngsFeatureAttachmentUpdate(FeatureH feature,
                                           long long aid,
                                           const char *name,
                                           const char *description,
                                           char logEdits);
NGS_EXTERNC void ngsStoreFeatureSetAttachmentRemoteId(FeatureH feature,
                                                      long long aid,
                                                      long long rid);

/*
 * Raster
 */
NGS_EXTERNC int ngsRasterCacheArea(CatalogObjectH object, char **options,
                                   ngsProgressFunc callback, void *callbackData);

/*
 * Map functions
 *
 *  ngsCreateMap -> ngsInitMap -> ngsSaveMap [optional]
 *  ngsLoadMap -> ngsInitMap -> ngsSaveMap [optional]
 */

typedef void *LayerH;
NGS_EXTERNC char ngsMapCreate(const char *name, const char *description,
                             unsigned short epsg, double minX, double minY,
                             double maxX, double maxY);
NGS_EXTERNC char ngsMapOpen(const char *path);
NGS_EXTERNC int ngsMapSave(char mapId, const char *path);
NGS_EXTERNC int ngsMapClose(char mapId);
NGS_EXTERNC int ngsMapReopen(char mapId, const char *path);
NGS_EXTERNC int ngsMapLayerCount(char mapId);
NGS_EXTERNC int ngsMapCreateLayer(char mapId, const char *name, const char *path);
NGS_EXTERNC LayerH ngsMapLayerGet(char mapId, int layerId);
NGS_EXTERNC int ngsMapLayerDelete(char mapId, LayerH layer);
NGS_EXTERNC int ngsMapLayerReorder(char mapId, LayerH beforeLayer, LayerH movedLayer);
NGS_EXTERNC int ngsMapSetSize(char mapId, int width, int height, char YAxisInverted);
NGS_EXTERNC int ngsMapDraw(char mapId, enum ngsDrawState state,
                           ngsProgressFunc callback, void *callbackData);
NGS_EXTERNC int ngsMapInvalidate(char mapId, ngsExtent bounds);
NGS_EXTERNC int ngsMapSetBackgroundColor(char mapId, const ngsRGBA color);
NGS_EXTERNC ngsRGBA ngsMapGetBackgroundColor(char mapId);
NGS_EXTERNC int ngsMapSetCenter(char mapId, double x, double y);
NGS_EXTERNC ngsCoordinate ngsMapGetCenter(char mapId);
NGS_EXTERNC ngsCoordinate ngsMapGetCoordinate(char mapId, double x, double y);
NGS_EXTERNC ngsCoordinate ngsMapGetDistance(char mapId, double w, double h);
NGS_EXTERNC int ngsMapSetRotate(char mapId, enum ngsDirection dir, double rotate);
NGS_EXTERNC double ngsMapGetRotate(char mapId, enum ngsDirection dir);
NGS_EXTERNC int ngsMapSetScale(char mapId, double scale);
NGS_EXTERNC double ngsMapGetScale(char mapId);

NGS_EXTERNC int ngsMapSetOptions(char mapId, char **options);
NGS_EXTERNC int ngsMapSetExtentLimits(char mapId, double minX, double minY, double maxX, double maxY);
NGS_EXTERNC ngsExtent ngsMapGetExtent(char mapId, int epsg);
NGS_EXTERNC int ngsMapSetExtent(char mapId, ngsExtent extent);

//NGS_EXTERNC void ngsMapSetLocation(char mapId, double x, double y, double azimuth);
NGS_EXTERNC JsonObjectH ngsMapGetSelectionStyle(char mapId, enum ngsStyleType styleType);
NGS_EXTERNC int ngsMapSetSelectionsStyle(char mapId, enum ngsStyleType styleType, JsonObjectH style);
NGS_EXTERNC const char *ngsMapGetSelectionStyleName(char mapId, enum ngsStyleType styleType);
NGS_EXTERNC int ngsMapSetSelectionStyleName(char mapId, enum ngsStyleType styleType, const char *name);
NGS_EXTERNC int ngsMapIconSetAdd(char mapId, const char *name, const char *path, char ownByMap);
NGS_EXTERNC int ngsMapIconSetRemove(char mapId, const char *name);
NGS_EXTERNC char ngsMapIconSetExists(char mapId, const char *name);

/*
 * Layer functions
 */

NGS_EXTERNC const char *ngsLayerGetName(LayerH layer);
NGS_EXTERNC int ngsLayerSetName(LayerH layer, const char *name);
NGS_EXTERNC char ngsLayerGetVisible(LayerH layer);
NGS_EXTERNC int ngsLayerSetVisible(LayerH layer, char visible);
NGS_EXTERNC float ngsLayerGetMaxZoom(LayerH layer);
NGS_EXTERNC int ngsLayerSetMaxZoom(LayerH layer, float zoom);
NGS_EXTERNC float ngsLayerGetMinZoom(LayerH layer);
NGS_EXTERNC int ngsLayerSetMinZoom(LayerH layer, float zoom);
NGS_EXTERNC CatalogObjectH ngsLayerGetDataSource(LayerH layer);
NGS_EXTERNC JsonObjectH ngsLayerGetStyle(LayerH layer);
NGS_EXTERNC int ngsLayerSetStyle(LayerH layer, JsonObjectH style);
NGS_EXTERNC const char *ngsLayerGetStyleName(LayerH layer);
NGS_EXTERNC int ngsLayerSetStyleName(LayerH layer, const char *name);
NGS_EXTERNC int ngsLayerSetSelectionIds(LayerH layer, long long *ids, int size);
NGS_EXTERNC int ngsLayerSetHideIds(LayerH layer, long long *ids, int size);

/*
 * Overlay functions
 */

NGS_EXTERNC int ngsOverlaySetVisible(char mapId, int typeMask, char visible);
NGS_EXTERNC char ngsOverlayGetVisible(char mapId, enum ngsMapOverlayType type);
NGS_EXTERNC int ngsOverlaySetOptions(char mapId, enum ngsMapOverlayType type, char **options);
NGS_EXTERNC char **ngsOverlayGetOptions(char mapId, enum ngsMapOverlayType type);
/* Edit */
typedef struct _ngsPointId
{
    int pointId;
    char isHole;
} ngsPointId;

NGS_EXTERNC ngsPointId ngsEditOverlayTouch(char mapId, double x, double y, enum ngsMapTouchType type);
NGS_EXTERNC char ngsEditOverlayUndo(char mapId);
NGS_EXTERNC char ngsEditOverlayRedo(char mapId);
NGS_EXTERNC char ngsEditOverlayCanUndo(char mapId);
NGS_EXTERNC char ngsEditOverlayCanRedo(char mapId);
NGS_EXTERNC FeatureH ngsEditOverlaySave(char mapId);
NGS_EXTERNC int ngsEditOverlayCancel(char mapId);
NGS_EXTERNC int ngsEditOverlayCreateGeometryInLayer(char mapId, LayerH layer, char empty);
NGS_EXTERNC int ngsEditOverlayCreateGeometry(char mapId, ngsGeometryType type);
NGS_EXTERNC int ngsEditOverlayEditGeometry(char mapId, LayerH layer, long long feateureId);
NGS_EXTERNC int ngsEditOverlayDeleteGeometry(char mapId);
NGS_EXTERNC int ngsEditOverlayAddPoint(char mapId);
NGS_EXTERNC int ngsEditOverlayAddVertex(char mapId, ngsCoordinate coordinates);
NGS_EXTERNC enum ngsEditDeleteResult ngsEditOverlayDeletePoint(char mapId);
NGS_EXTERNC int ngsEditOverlayAddHole(char mapId);
NGS_EXTERNC enum ngsEditDeleteResult ngsEditOverlayDeleteHole(char mapId);
NGS_EXTERNC int ngsEditOverlayAddGeometryPart(char mapId);
NGS_EXTERNC enum ngsEditDeleteResult ngsEditOverlayDeleteGeometryPart(char mapId);
NGS_EXTERNC GeometryH ngsEditOverlayGetGeometry(char mapId);
NGS_EXTERNC int ngsEditOverlaySetStyle(char mapId, enum ngsEditStyleType type, JsonObjectH style);
NGS_EXTERNC int ngsEditOverlaySetStyleName(char mapId, enum ngsEditStyleType type, const char *name);
NGS_EXTERNC JsonObjectH ngsEditOverlayGetStyle(char mapId, enum ngsEditStyleType type);
NGS_EXTERNC void ngsEditOverlaySetWalkingMode(char mapId, char enable);
NGS_EXTERNC char ngsEditOverlayGetWalkingMode(char mapId);

/* Location */
NGS_EXTERNC int ngsLocationOverlayUpdate(char mapId, ngsCoordinate location,
                                         float direction, float accuracy);
NGS_EXTERNC int ngsLocationOverlaySetStyle(char mapId, JsonObjectH style);
NGS_EXTERNC int ngsLocationOverlaySetStyleName(char mapId, const char *name);
NGS_EXTERNC JsonObjectH ngsLocationOverlayGetStyle(char mapId);

//NGS_EXTERNC int ngsGraphicsOverlayDrawRectangle(unsigned char mapId, double minX, double minY, double maxX, double maxY);
//NGS_EXTERNC int ngsGraphicsOverlayDrawCircle(unsigned char mapId, double X, double Y, double radius);

///** Map canvas functions */
//NGS_EXTERNC ngsPosition ngsDisplayGetPosition(unsigned char mapId, double x, double y);
//NGS_EXTERNC ngsPosition ngsDisplayGetLength(unsigned char mapId, double w, double h);

/*
 * QMS
 */

typedef struct _ngsQMSItem
{
    int id;
    const char *name;
    const char *desc;
    enum ngsCatalogObjectType type; /**< May be CAT_RASTER_TMS, CAT_RASTER_WMS, CAT_FC_GEOJSON */
    const char *iconUrl;
    enum ngsCode status; /**< May be COD_SUCCESS, COD_WARNING, COD_REQUEST_FAILED */
    ngsExtent extent;
    int total;
} ngsQMSItem;

NGS_EXTERNC ngsQMSItem *ngsQMSQuery(char **options);

typedef struct _ngsQMSItemProperties
{
    int id;
    enum ngsCode status; /**< May be COD_SUCCESS, COD_WARNING, COD_REQUEST_FAILED */
    const char *url;
    const char *name;
    const char *desc;
    enum ngsCatalogObjectType type; /**< May be CAT_RASTER_TMS, CAT_RASTER_WMS, CAT_FC_GEOJSON */
    int EPSG;
    int z_min;
    int z_max;
    const char *iconUrl;
    ngsExtent extent;
    char y_origin_top;
} ngsQMSItemProperties;

NGS_EXTERNC ngsQMSItemProperties ngsQMSQueryProperties(int itemId);

/*
 * Account
 */

NGS_EXTERNC const char *ngsAccountGetFirstName();
NGS_EXTERNC const char *ngsAccountGetLastName();
NGS_EXTERNC const char *ngsAccountGetEmail();
NGS_EXTERNC const char *ngsAccountBitmapPath();
NGS_EXTERNC char ngsAccountIsAuthorized();
NGS_EXTERNC void ngsAccountExit();
NGS_EXTERNC char ngsAccountIsFuncAvailable(const char *application,
                                           const char *function);
NGS_EXTERNC char ngsAccountSupported();
NGS_EXTERNC char ngsAccountUpdateUserInfo();
NGS_EXTERNC char ngsAccountUpdateSupportInfo();

/*
 * Tracks
 */

typedef struct _ngsTrackInfo {
    const char *name;
    long startTimeStamp;
    long stopTimeStamp;
    long count;
} ngsTrackInfo;

NGS_EXTERNC CatalogObjectH ngsStoreGetTracksTable(CatalogObjectH store);
NGS_EXTERNC CatalogObjectH ngsTrackGetPointsTable(CatalogObjectH tracksTable);
NGS_EXTERNC char ngsStoreHasTracksTable(CatalogObjectH store);
NGS_EXTERNC char ngsTrackIsRegistered();
NGS_EXTERNC void ngsTrackSync(CatalogObjectH tracksTable, int maxPointCount);
NGS_EXTERNC ngsTrackInfo *ngsTrackGetList(CatalogObjectH tracksTable);
NGS_EXTERNC char ngsTrackAddPoint(CatalogObjectH tracksTable, const char *trackName, double x, double y, double z,
                                  float acc, float speed, float course, long timeStamp, int satCount, char newTrack,
                                  char newSegment);
NGS_EXTERNC char ngsTrackDeletePoints(CatalogObjectH tracksTable, long start, long stop);


#endif // NGSAPI_H
