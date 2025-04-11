/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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

/* Spatial coordinates */
typedef struct _ngsCoordinate {
    double X;
    double Y;
    double Z;
} ngsCoordinate;

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

#ifndef POINTER_SIZE
typedef long long POINTER_SIZE;
#endif // POINTER_SIZE

typedef struct _ngsFeatureChange {
    POINTER_SIZE fid;
    POINTER_SIZE aid;
    enum ngsChangeCode code;
    POINTER_SIZE rid;
    POINTER_SIZE arid;
} ngsFeatureChange;

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
                                        const char *extension, char catalog);
NGS_EXTERNC void* ngsMalloc(POINTER_SIZE size);
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
NGS_EXTERNC const char *ngsSHA256(const char *value);
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
NGS_EXTERNC char ngsCatalogObjectSync(CatalogObjectH object, 
    ngsSyncMergeType type, ngsFeatureChange **conflicts, ngsProgressFunc callback,
    void *callbackData);

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

NGS_EXTERNC ngsField *ngsFeatureClassFields(CatalogObjectH object);
NGS_EXTERNC ngsGeometryType ngsFeatureClassGeometryType(CatalogObjectH object);
NGS_EXTERNC FeatureH ngsFeatureClassCreateFeature(CatalogObjectH object);
NGS_EXTERNC void ngsFeatureClassBatchMode(CatalogObjectH object, char enable);
NGS_EXTERNC int ngsFeatureClassInsertFeature(CatalogObjectH object,
                                             FeatureH feature, char logEdits);
NGS_EXTERNC int ngsFeatureClassUpdateFeature(CatalogObjectH object,
                                             FeatureH feature, char logEdits);
NGS_EXTERNC int ngsFeatureClassDeleteFeature(CatalogObjectH object, POINTER_SIZE id,
                                             char logEdits);
NGS_EXTERNC int ngsFeatureClassDeleteFeatures(CatalogObjectH object, char logEdits);
NGS_EXTERNC POINTER_SIZE ngsFeatureClassCount(CatalogObjectH object);
NGS_EXTERNC void ngsFeatureClassResetReading(CatalogObjectH object);
NGS_EXTERNC FeatureH ngsFeatureClassNextFeature(CatalogObjectH object);
NGS_EXTERNC FeatureH ngsFeatureClassGetFeature(CatalogObjectH object, POINTER_SIZE id);
NGS_EXTERNC int ngsFeatureClassSetFilter(CatalogObjectH object,
                                         GeometryH geometryFilter,
                                         const char *attributeFilter);
NGS_EXTERNC int ngsFeatureClassSetSpatialFilter(CatalogObjectH object,
                                                double minX, double minY,
                                                double maxX, double maxY);
NGS_EXTERNC int ngsFeatureClassDeleteEditOperation(CatalogObjectH object,
                                                  ngsFeatureChange operation);
NGS_EXTERNC ngsFeatureChange *ngsFeatureClassGetEditOperations(CatalogObjectH object);

NGS_EXTERNC void ngsFeatureFree(FeatureH feature);
NGS_EXTERNC int ngsFeatureFieldCount(FeatureH feature);
NGS_EXTERNC char ngsFeatureIsFieldSet(FeatureH feature, int fieldIndex);
NGS_EXTERNC POINTER_SIZE ngsFeatureGetId(FeatureH feature);
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
    POINTER_SIZE id;
    const char *name;
    const char *description;
    const char *path;
    POINTER_SIZE size;
} ngsFeatureAttachmentInfo;

NGS_EXTERNC POINTER_SIZE ngsFeatureAttachmentAdd(FeatureH feature,
                                         const char *name,
                                         const char *description,
                                         const char *path,
                                         char **options,
                                         char logEdits);
NGS_EXTERNC char ngsFeatureAttachmentDelete(FeatureH feature, POINTER_SIZE aid,
                                           char logEdits);
NGS_EXTERNC char ngsFeatureAttachmentDeleteAll(FeatureH feature, char logEdits);
NGS_EXTERNC ngsFeatureAttachmentInfo *ngsFeatureAttachmentsGet(FeatureH feature);
NGS_EXTERNC char ngsFeatureAttachmentUpdate(FeatureH feature,
                                           POINTER_SIZE aid,
                                           const char *name,
                                           const char *description,
                                           char logEdits);

/* Location */
NGS_EXTERNC int ngsLocationOverlayUpdate(char mapId, ngsCoordinate location,
                                         float direction, float accuracy);
NGS_EXTERNC int ngsLocationOverlaySetStyle(char mapId, JsonObjectH style);
NGS_EXTERNC int ngsLocationOverlaySetStyleName(char mapId, const char *name);
NGS_EXTERNC JsonObjectH ngsLocationOverlayGetStyle(char mapId);

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
NGS_EXTERNC char ngsAccountUpdateTeamsInfo();

typedef struct _ngsNGWUserInfo {
    const char *firstName;
    const char *lastName;
    const char *username;
    const char *guid;
    const char *locale;
} ngsNGWUserInfo;

typedef struct _ngsNGWTeamInfo {
    const char *id;
    const char *ownerId;
    const char *webgis;
    const char *startDate;
    const char *endDate;
    int usersSize;
    ngsNGWUserInfo **users;
} ngsNGWTeamInfo;

NGS_EXTERNC ngsNGWTeamInfo **ngsAccountGetTeams();
NGS_EXTERNC int ngsAccountGetTeamsSize();

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
NGS_EXTERNC ngsTrackInfo *ngsTrackGetList(CatalogObjectH tracksTable);
NGS_EXTERNC char ngsTrackAddPoint(CatalogObjectH tracksTable, const char *trackName, double x, double y, double z,
                                  float acc, float speed, float course, long timeStamp, int satCount, char newTrack,
                                  char newSegment);
NGS_EXTERNC char ngsTrackDeletePoints(CatalogObjectH tracksTable, long start, long stop);

/*
 * NGW
 */

typedef struct _ngsNGWServiceLayerInfo {
    const char *keyName;
    const char *displayName;
    int resourceId;
} ngsNGWServiceLayerInfo;

NGS_EXTERNC ngsNGWServiceLayerInfo *ngsNGWServiceList(CatalogObjectH object);
NGS_EXTERNC char ngsNGWServiceDeleteLayer(CatalogObjectH object,
                                          const char *keyName);
NGS_EXTERNC char ngsNGWServiceAddLayer(CatalogObjectH object,
                                       const char *keyName,
                                       const char *displayName,
                                       CatalogObjectH ngwObject);
NGS_EXTERNC char ngsNGWServiceChangeLayer(CatalogObjectH object,
                                          const char *originalKeyName,
                                          const char *newKeyName,
                                          const char *newDisplayName,
                                          CatalogObjectH ngwObject);
// ngsNGWServiceGet/SetLayerOptions(CatalogObjectH object, const char *keyName, char **key-value list);
// or use standard ngsCatalogObjectProperty/ngsCatalogObjectSetProperty

typedef struct _ngsNGWWebmapItemInfo {
    enum ngsWebMapItemType itemType;
    const char *displayName;
} ngsNGWWebmapItemInfo;

typedef struct _ngsNGWWebmapLayerInfo {
    ngsNGWWebmapItemInfo itemInfo;
    const char *adapter;
    char enabled;
    int orderPosition;
    const char *maxScaleDenom;
    const char *minScaleDenom;
    char transparency; // 0 - 100
    CatalogObjectH layer;
} ngsNGWWebmapLayerInfo;

typedef struct _ngsNGWWebmapGroupInfo {
    ngsNGWWebmapItemInfo itemInfo;
    char expanded;
    ngsNGWWebmapItemInfo **children;
} ngsNGWWebmapGroupInfo;

typedef struct _ngsNGWWebmapBasemapInfo {
    int opacity;
    char enabled;
    const char *displayName;
    CatalogObjectH baseMap;
} ngsNGWWebmapBasemapInfo;

NGS_EXTERNC char ngsNGWWebMapDeleteBaseMap(CatalogObjectH object, int index);
NGS_EXTERNC char ngsNGWWebMapAddBaseMap(CatalogObjectH object,
                                        ngsNGWWebmapBasemapInfo baseMap);
NGS_EXTERNC char ngsNGWWebMapInsertBaseMap(CatalogObjectH object,
                                           ngsNGWWebmapBasemapInfo baseMap,
                                           int index);
NGS_EXTERNC ngsNGWWebmapBasemapInfo *ngsNGWWebMapBaseMapList(CatalogObjectH object);

NGS_EXTERNC ngsNGWWebmapGroupInfo *ngsNGWWebMapLayerTree(CatalogObjectH object);
NGS_EXTERNC char ngsNGWWebMapDeleteItem(CatalogObjectH object, long id);
NGS_EXTERNC long ngsNGWWebMapInsertItem(CatalogObjectH object, long pos,
                                        ngsNGWWebmapItemInfo *item);
NGS_EXTERNC void ngsNGWWebmapItemInfoFree(ngsNGWWebmapItemInfo *item);
NGS_EXTERNC void ngsNGWWebmapGroupInfoFree(ngsNGWWebmapGroupInfo *group);

#endif // NGSAPI_H
