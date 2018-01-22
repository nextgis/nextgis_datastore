/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#include "featureclass.h"

#include <algorithm>

#include "api_priv.h"
#include "coordinatetransformation.h"
#include "dataset.h"
#include "map/maptransform.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"

// For gettext translation
#define POINT_STR _("Point")
#define LINESTRING_STR _("Line String")
#define POLYGON_STR _("Polygon")
#define MPOINT_STR _("Multi Point")
#define MLINESTRING_STR _("Multi Line String")
#define MPOLYGON_STR _("Multi Polygon")

namespace ngs {

constexpr const char* ZOOM_LEVELS_OPTION = "ZOOM_LEVELS";
constexpr unsigned short TILE_SIZE = 256; //240; //512;// 160; // Only use for overviews now in pixelSize
constexpr double WORLD_WIDTH = DEFAULT_BOUNDS_X2.width();

//------------------------------------------------------------------------------
// TilingData
//------------------------------------------------------------------------------

class TilingData : public ThreadData {
public:
    TilingData(FeatureClass* featureClass, FeaturePtr feature, bool own) :
        ThreadData(own), m_feature(feature), m_featureClass(featureClass) {

    }
    FeaturePtr m_feature;
    FeatureClass* m_featureClass;
};

//------------------------------------------------------------------------------
// FeatureClass
//------------------------------------------------------------------------------

FeatureClass::FeatureClass(OGRLayer* layer, ObjectContainer * const parent,
                           const enum ngsCatalogObjectType type,
                           const CPLString &name) :
    Table(layer, parent, type, name),
    m_ovrTable(nullptr),
    m_genTileMutex(CPLCreateMutex()),
    m_creatingOvr(false)
{
    CPLReleaseMutex(m_genTileMutex);

    if(nullptr != m_layer) {
        m_spatialReference = m_layer->GetSpatialRef();

        OGRFeatureDefn* defn = m_layer->GetLayerDefn();
        for(int i = 0; i < defn->GetFieldCount(); ++i) {
            OGRFieldDefn* fld = defn->GetFieldDefn(i);
            m_ignoreFields.push_back(fld->GetNameRef());
        }
        m_ignoreFields.push_back("OGR_STYLE");

        fillZoomLevels();

        OGREnvelope env;
        if(m_layer->GetExtent(&env, 0) == OGRERR_NONE ||
            m_layer->GetExtent(&env, 1) == OGRERR_NONE) {
            m_extent = env;
        }
        else {
            CPLDebug("ngstore", "GetExent failed");
        }

        m_fastSpatialFilter = m_layer->TestCapability(OLCFastSpatialFilter) == 1;
    }

    getTilesTable();
}

FeatureClass::~FeatureClass()
{
    CPLDebug("ngstore", "CPLDestroyMutex(m_genTileMutex)");
    CPLDestroyMutex(m_genTileMutex);
}

OGRwkbGeometryType FeatureClass::geometryType() const
{
    if(nullptr == m_layer)
        return wkbUnknown;
    return m_layer->GetGeomType();
}

const char* FeatureClass::geometryColumn() const
{
    if(nullptr == m_layer)
        return "";
    return m_layer->GetGeometryColumn();
}

std::vector<const char*> FeatureClass::geometryColumns() const
{
    std::vector<const char*> out;
    if(nullptr == m_layer) {
        return out;
    }
    OGRFeatureDefn* defn = m_layer->GetLayerDefn();
    for(int i = 0; i < defn->GetGeomFieldCount(); ++i) {
        OGRGeomFieldDefn* geom = defn->GetGeomFieldDefn(i);
        out.push_back(geom->GetNameRef());
    }
    return out;
}


int FeatureClass::copyFeatures(const FeatureClassPtr srcFClass,
                               const FieldMapPtr fieldMap,
                               OGRwkbGeometryType filterGeomType,
                               const Progress& progress, const Options &options)
{
    if(!srcFClass) {
        return errorMessage(COD_COPY_FAILED, _("Source feature class is invalid"));
    }

    progress.onProgress(COD_IN_PROCESS, 0.0,
                       _("Start copy features from '%s' to '%s'"),
                       srcFClass->name().c_str(), m_name.c_str());

    bool skipEmpty = options.boolOption("SKIP_EMPTY_GEOMETRY", false);
    bool skipInvalid = options.boolOption("SKIP_INVALID_GEOMETRY", false);
    bool toMulti = options.boolOption("FORCE_GEOMETRY_TO_MULTI", false);

    DatasetBatchOperationHolder holder(dynamic_cast<Dataset*>(m_parent));

    OGRSpatialReference* srcSRS = srcFClass->getSpatialReference();
    OGRSpatialReference* dstSRS = getSpatialReference();
    CoordinateTransformation CT(srcSRS, dstSRS);
    GIntBig featureCount = srcFClass->featureCount();
    OGRwkbGeometryType dstGeomType = geometryType();
    double counter = 0;
    srcFClass->reset();
    FeaturePtr feature;
    while((feature = srcFClass->nextFeature())) {
        double complete = counter / featureCount;
        if(!progress.onProgress(COD_IN_PROCESS, complete,
                                _("Copy in process ..."))) {
            return COD_CANCELED;
        }

        OGRGeometry* geom = feature->GetGeometryRef();
        OGRGeometry* newGeom = nullptr;
        if(nullptr == geom) {
            if(skipEmpty) {
                continue;
            }
        }
        else {
            if(skipEmpty && geom->IsEmpty()) {
                continue;
            }
            if(skipInvalid && !geom->IsValid()) {
                continue;
            }

            OGRwkbGeometryType geomType = geom->getGeometryType();
            OGRwkbGeometryType multiGeomType = geomType;
            if (OGR_GT_Flatten(geomType) < wkbPolygon && toMulti) {
                multiGeomType = static_cast<OGRwkbGeometryType>(geomType + 3);
            }
            if (filterGeomType != wkbUnknown && filterGeomType != multiGeomType) {
                continue;
            }

            newGeom = geom->clone();
            if (dstGeomType != geomType) {
                newGeom = OGRGeometryFactory::forceTo(newGeom, dstGeomType);
            }

            CT.transform(newGeom);
        }

        FeaturePtr dstFeature = createFeature();
        if(nullptr != newGeom) {
            dstFeature->SetGeometryDirectly(newGeom);
        }
        dstFeature->SetFieldsFrom(feature, fieldMap.get());

        if(!insertFeature(dstFeature)) {
            if(!progress.onProgress(COD_WARNING, complete,
                               _("Create feature failed. Source feature FID:" CPL_FRMT_GIB),
                               feature->GetFID ())) {
                return COD_CANCELED;
            }
        }
        counter++;
    }
    progress.onProgress(COD_FINISHED, 1.0, _("Done. Copied %d features"),
                       int(counter));

    return COD_SUCCESS;
}

bool FeatureClass::hasOverviews() const
{
    if(nullptr != m_ovrTable) {
        return true;
    }

    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return false;
    }

    return dataset->getOverviewsTable(name());
}

double FeatureClass::pixelSize(int zoom, bool precize)
{
    int tilesInMapOneDim = 1 << zoom;

    // Tile size. On ower zoom less size
    int tileSize = TILE_SIZE;
    if(precize) {
        tileSize *= 2;
    }
    else {
        tileSize -= (20 - zoom) * 8; // NOTE: Only useful fo points
    }

    long sizeOneDimPixels = tilesInMapOneDim * tileSize;
    return WORLD_WIDTH / sizeOneDimPixels;
}

bool FeatureClass::getTilesTable()
{
    if(m_ovrTable) {
        return true;
    }

    Dataset* parentDS = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDS) {
        return false;
    }

    m_ovrTable = parentDS->getOverviewsTable(name());
//    if(nullptr == m_ovrTable) {
//        m_ovrTable = parentDS->createOverviewsTable(name());
//    }

    return m_ovrTable != nullptr;
}

FeaturePtr FeatureClass::getTileFeature(const Tile& tile)
{
    if(!getTilesTable()) {
        return FeaturePtr();
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));

    m_ovrTable->SetAttributeFilter(CPLSPrintf("%s = %d AND %s = %d AND %s = %d",
                                              OVR_X_KEY, tile.x,
                                              OVR_Y_KEY, tile.y,
                                              OVR_ZOOM_KEY, tile.z));
    FeaturePtr out(m_ovrTable->GetNextFeature());
    m_ovrTable->SetAttributeFilter(nullptr);

    return out;
}

VectorTile FeatureClass::getTileInternal(const Tile& tile)
{
    VectorTile vtile;
    FeaturePtr ovrTile = getTileFeature(tile);
    if(ovrTile) {
        int size = 0;
        GByte* data = ovrTile->GetFieldAsBinary(ovrTile->GetFieldIndex(OVR_TILE_KEY),
                                                &size);
        Buffer buff(data, size, false);
        vtile.load(buff);
    }
    return vtile;
}

bool FeatureClass::setTileFeature(FeaturePtr tile)
{
    if(!getTilesTable()) {
        return false;
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return m_ovrTable->SetFeature(tile) == OGRERR_NONE;
}

bool FeatureClass::createTileFeature(FeaturePtr tile)
{
    if(!getTilesTable()) {
        return false;
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return m_ovrTable->CreateFeature(tile) == OGRERR_NONE;
}

bool FeatureClass::tilingDataJobThreadFunc(ThreadData* threadData)
{
    TilingData* data = static_cast<TilingData*>(threadData);
    // Get tiles for geometry
    OGRGeometry* geom = data->m_feature->GetGeometryRef();
    if( nullptr == geom) {
        return true;
    }

    GEOSGeometryPtr geosGeom(new GEOSGeometryWrap(geom));
    GIntBig fid = data->m_feature->GetFID();

    OGREnvelope env;
    geom->getEnvelope(&env);
    bool precisePixelSize = !(OGR_GT_Flatten(geom->getGeometryType()) == wkbPoint ||
                              OGR_GT_Flatten(geom->getGeometryType()) == wkbMultiPoint);

    auto zoomLevels = data->m_featureClass->zoomLevels();
    for(auto it = zoomLevels.rbegin(); it != zoomLevels.rend(); ++it) {
        unsigned char zoomLevel = *it;
        CPLDebug("ngstore", "tilingDataJobThreadFunc for zoom %d", zoomLevel);
        Envelope extent = extraExtentForZoom(zoomLevel, env);

        std::vector<TileItem> items = MapTransform::getTilesForExtent(
                    extent, zoomLevel, false, true);

        double step = FeatureClass::pixelSize(zoomLevel, precisePixelSize);
        geosGeom->simplify(step);
        for(auto tileItem : items) {
            Envelope ext = tileItem.env;
            ext.resize(TILE_RESIZE);

            auto vItems = data->m_featureClass->tileGeometry(fid, geosGeom, ext);
            data->m_featureClass->addOverviewItem(tileItem.tile, vItems);
        }
    }

    return true;
}

bool FeatureClass::createOverviews(const Progress &progress, const Options &options)
{
    CPLDebug("ngstore", "start create overviews");
    m_genTiles.clear();
    bool force = options.boolOption("FORCE", false);
    if(!force && hasOverviews()) {
        return true;
    }

    Dataset* parentDS = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDS) {
        progress.onProgress(COD_CREATE_FAILED, 0.0,
                            _("Unsupported feature class"));
        return errorMessage(_("Unsupported feature class"));
    }

    if(nullptr == m_ovrTable) {
        m_ovrTable = parentDS->getOverviewsTable(name());
    }

    if(nullptr == m_ovrTable) {
        m_ovrTable = parentDS->createOverviewsTable(name());
    }
    else {
        parentDS->clearOverviewsTable(name());
    }

    // Drop index
    parentDS->dropOverviewsTableIndex(name());

    // Fill overview layer with data
    const CPLString &zoomLevelListStr = options.stringOption(
                ZOOM_LEVELS_OPTION, "");
    fillZoomLevels(zoomLevelListStr);
    if(m_zoomLevels.empty()) {
        return true;
    }

    setProperty("zoom_levels", zoomLevelListStr, NG_ADDITIONS_KEY);

    // Tile and simplify geometry
    progress.onProgress(COD_IN_PROCESS, 0.0,
                        _("Start tiling and simplifying geometry"));

    // Multithreaded thread pool
    CPLDebug("ngstore", "fill pool create overviews");
    ThreadPool threadPool;
    threadPool.init(getNumberThreads(), tilingDataJobThreadFunc);
    emptyFields(true);
    reset();


    FeaturePtr feature;
    while((feature = nextFeature())) {
        threadPool.addThreadData(new TilingData(this, feature, true));
    }

    Progress newProgress(progress);
    newProgress.setTotalSteps(2);
    newProgress.setStep(0);
    threadPool.waitComplete(newProgress);
    threadPool.clearThreadData();

    emptyFields(false);
    reset();

    // Save tiles
    m_creatingOvr = true;
    parentDS->lockExecuteSql(true);
    parentDS->startBatchOperation();

    CPLDebug("ngstore", "finish create overviews");
    double counter = 0.0;
    newProgress.setStep(1);
    for(auto item : m_genTiles) {
        if(!item.second.isValid() || item.second.empty()) {
            continue;
        }
        BufferPtr data = item.second.save();

        FeaturePtr newFeature = OGRFeature::CreateFeature(
                    m_ovrTable->GetLayerDefn() );

        newFeature->SetField(OVR_ZOOM_KEY, item.first.z);
        newFeature->SetField(OVR_X_KEY, item.first.x);
        newFeature->SetField(OVR_Y_KEY, item.first.y);
        newFeature->SetField(newFeature->GetFieldIndex(OVR_TILE_KEY), data->size(),
                          data->data());

//        CPLDebug("ngstore", "Tile size to store %d", data->size());

        if(m_ovrTable->CreateFeature(newFeature) != OGRERR_NONE) {
            errorMessage(COD_INSERT_FAILED, _("Failed to create feature"));
        }

        newProgress.onProgress(COD_IN_PROCESS, counter/m_genTiles.size(),
                               _("Save tiles ..."));
        counter++;
    }

    parentDS->stopBatchOperation();
    m_genTiles.clear();

    // Create index
    parentDS->createOverviewsTableIndex(name());
    parentDS->lockExecuteSql(false);
    m_creatingOvr = false;

    progress.onProgress(COD_FINISHED, 1.0,
                        _("Finish tiling and simplifying geometry"));

    CPLDebug("ngstore", "finish create overviews");
    return true;
}

VectorTile FeatureClass::getTile(const Tile& tile, const Envelope& tileExtent)
{
    VectorTile vtile;
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset || m_creatingOvr) {
        return vtile;
    }

    if(!extent().intersects(tileExtent)) {
        return vtile;
    }

    if(hasOverviews()) {
        if(tile.z <= *m_zoomLevels.rbegin()) {
//
//            // Get closest zoom level
//            unsigned char diff = 127;
//            unsigned char z = 0;
//            for(auto zoomLevel : m_zoomLevels) {
//                unsigned char newDiff = static_cast<unsigned char>(
//                            ABS(zoomLevel - tile.z));
//                if(newDiff < diff) {
//                    z = zoomLevel;
//                    diff = newDiff;
//                    if(diff == 0)
//                        break;
//                }
//            }

            vtile = getTileInternal(tile);
//            if(vtile.isValid()) {
                return vtile;
//            }
        }
    }

    // Tiling on the fly
    CPLDebug("ngstore", "Tiling on the fly in %s", m_name.c_str());

    // Calc grid step for zoom
    bool precisePixelSize = !(OGR_GT_Flatten(geometryType()) == wkbPoint ||
                              OGR_GT_Flatten(geometryType()) == wkbMultiPoint);

    double step = pixelSize(tile.z, precisePixelSize);

    std::vector<FeaturePtr> features;
    OGREnvelope extEnv;
    if(!m_fastSpatialFilter) {
        extEnv = tileExtent.toOgrEnvelope();
    }

    // Lock threads here    
    dataset->lockExecuteSql(true);
    CPLAcquireMutex(m_featureMutex, 10.5);
    emptyFields(true);
    GeometryPtr extGeom = tileExtent.toGeometry(getSpatialReference());
    setSpatialFilter(extGeom);
    //reset();
    FeaturePtr feature;
    while((feature = nextFeature())) {
        if(m_fastSpatialFilter) {
            features.push_back(feature);
        }
        else {
            OGRGeometry* geom = feature->GetGeometryRef();
            if(geom) {
                OGREnvelope env;
                geom->getEnvelope(&env);
                if(env.IsInit() && env.Intersects(extEnv)) {
                    features.push_back(feature);
                }
            }
        }
    }
    emptyFields(false);
    setSpatialFilter();
    dataset->lockExecuteSql(false);
    CPLReleaseMutex(m_featureMutex);

    while(!features.empty()) {
        feature = features.back();

        OGRGeometry* geom = feature->GetGeometryRef();
        if(nullptr != geom) {

            GEOSGeometryPtr geosGeom(new GEOSGeometryWrap(geom));
            GIntBig fid = feature->GetFID();
            geosGeom->simplify(step);

            VectorTileItemArray items = tileGeometry(fid, geosGeom, tileExtent);
            if(!items.empty()) {
                vtile.add(items, false);
            }
        }
        features.pop_back();
    }

//    Debug test
//    TablePtr features = dataset->executeSQL(CPLSPrintf("SELECT * FROM %s",
//                                                       getName().c_str()),
//                                            extGeom);
//    if(features) {
//        FeaturePtr feature;
//        while(true) {
//            feature = features->nextFeature();
//            if(!feature)
//                break;
//            VectorTileItem item = tileGeometry(feature, extGeom.get(), step);
//            if(item.isValid()) {
//                vtile.add(item, false);
//            }
//        }
//    }

    return vtile;
}

VectorTileItemArray FeatureClass::tileGeometry(GIntBig fid, GEOSGeometryPtr geom,
                                               const Envelope& env) const
{
    VectorTileItemArray out;
    if(!geom->isValid()) {
        return out;
    }

    GEOSGeometryPtr clipGeom = geom->clip(env);
    clipGeom->fillTile(fid, out);

    return out;
}

bool FeatureClass::setIgnoredFields(const std::vector<const char*>& fields)
{
    if(nullptr == m_layer) {
        return false;
    }
    if(fields.empty()) {
        return m_layer->SetIgnoredFields(nullptr) == OGRERR_NONE;
    }

    char** ignoreFields = nullptr;
    for(const char* fieldName : fields) {
        ignoreFields = CSLAddString(ignoreFields, fieldName);
    }
    bool result = m_layer->SetIgnoredFields(const_cast<const char**>(
                                                ignoreFields)) == OGRERR_NONE;
    CSLDestroy(ignoreFields);
    return result;
}

void FeatureClass::setSpatialFilter(const GeometryPtr& geom)
{
    if(nullptr != m_layer) {
        if(geom) {
            m_layer->SetSpatialFilter(geom.get());
        }
        else {
            m_layer->SetSpatialFilter(nullptr);
        }
    }
}

void FeatureClass::setSpatialFilter(double minX, double minY,
                                    double maxX, double maxY)
{
    if(nullptr != m_layer) {
        m_layer->SetSpatialFilterRect(minX, minY, maxX, maxY);
    }
}

Envelope FeatureClass::extent() const
{
    return m_extent;
}

const char* FeatureClass::geometryTypeName(OGRwkbGeometryType type,
                                                GeometryReportType reportType)
{
    if(reportType == GeometryReportType::FULL)
        return OGRGeometryTypeToName(type);
    if(reportType == GeometryReportType::OGC)
        return OGRToOGCGeomType(type);

    switch(OGR_GT_Flatten(type)) {
    case wkbUnknown:
        return "unk";
    case wkbPoint:
        return "pt";
    case wkbLineString:
        return "ln";
    case wkbPolygon:
        return "plg";
    case wkbMultiPoint:
        return "mptr";
    case wkbMultiLineString:
        return "mln";
    case wkbMultiPolygon:
        return "mplg";
    case wkbGeometryCollection:
        return "gt";
    case wkbCircularString:
        return "cir";
    case wkbCompoundCurve:
        return "ccrv";
    case wkbCurvePolygon:
        return "crvplg";
    case wkbMultiCurve:
        return "mcrv";
    case wkbMultiSurface:
        return "msurf";
    case wkbCurve:
        return "crv";
    case wkbSurface:
        return "surf";
    case wkbPolyhedralSurface:
        return "psurf";
    case wkbTIN:
        return "tin";
    case wkbTriangle:
        return "triangle";
    default:
        return "any";
    }
}

OGRwkbGeometryType FeatureClass::geometryTypeFromName(const char* name)
{
    if(nullptr == name || EQUAL(name, ""))
        return wkbUnknown;
    if(EQUAL(name, "POINT"))
        return wkbPoint;
    if(EQUAL(name, "LINESTRING"))
        return wkbLineString;
    if(EQUAL(name, "POLYGON"))
        return wkbPolygon;
    if(EQUAL(name, "MULTIPOINT"))
        return wkbMultiPoint;
    if(EQUAL(name, "MULTILINESTRING"))
        return wkbMultiLineString;
    if(EQUAL(name, "MULTIPOLYGON"))
        return wkbMultiPolygon;
    return wkbUnknown;
}

OGRFieldType FeatureClass::fieldTypeFromName(const char* name)
{
    if(nullptr == name || EQUAL(name, ""))
        return OFTMaxType;
    if(EQUAL(name, "INTEGER"))
        return OFTInteger;
    if(EQUAL(name, "INTEGER_LIST"))
        return OFTIntegerList;
    if(EQUAL(name, "REAL"))
        return OFTReal;
    if(EQUAL(name, "REAL_LIST"))
        return OFTRealList;
    if(EQUAL(name, "STRING"))
        return OFTString;
    if(EQUAL(name, "STRING_LIST"))
        return OFTStringList;
    if(EQUAL(name, "BINARY"))
        return OFTBinary;
    if(EQUAL(name, "DATE"))
        return OFTDate;
    if(EQUAL(name, "TIME"))
        return OFTTime;
    if(EQUAL(name, "DATE_TIME"))
        return OFTDateTime;
    if(EQUAL(name, "INTEGER64"))
        return OFTInteger64;
    if(EQUAL(name, "INTEGER64_LIST"))
        return OFTInteger64List;
    return OFTMaxType;
}

std::vector<OGRwkbGeometryType> FeatureClass::geometryTypes() const
{
    std::vector<OGRwkbGeometryType> out;

    OGRwkbGeometryType geomType = geometryType();
    if (OGR_GT_Flatten(geomType) == wkbUnknown ||
            OGR_GT_Flatten(geomType) == wkbGeometryCollection) {

        emptyFields(true);
        reset();
        std::map<OGRwkbGeometryType, int> counts;
        FeaturePtr feature;
        while((feature = nextFeature())) {
            OGRGeometry * const geom = feature->GetGeometryRef();
            if (nullptr != geom) {
                geomType = geom->getGeometryType();
                counts[OGR_GT_Flatten(geomType)] += 1;
            }
        }
        emptyFields(false);

        if(counts[wkbPoint] > 0) {
            if(counts[wkbMultiPoint] > 0) {
                out.push_back(wkbMultiPoint);
            }
            else {
                out.push_back(wkbPoint);
            }
        }
        else if(counts[wkbLineString] > 0) {
            if(counts[wkbMultiLineString] > 0) {
                out.push_back(wkbMultiLineString);
            }
            else {
                out.push_back(wkbLineString);
            }
        }
        else if(counts[wkbPolygon] > 0) {
            if(counts[wkbMultiPolygon] > 0) {
                out.push_back(wkbMultiPolygon);
            }
            else {
                out.push_back(wkbPolygon);
            }
        }
    }
    else {
        out.push_back (geomType);
    }
    return out;
}

bool FeatureClass::destroy()
{
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return false;
    }

    if(m_ovrTable) {
        m_ovrTable->ResetReading();
    }
    m_layer->SetSpatialFilter(nullptr);
    CPLString name = m_name;
    if(!Table::destroy()) {
        return false;
    }

    dataset->destroyOverviewsTable(name); // Overviews table maybe not exists

    return true;
}

void FeatureClass::fillZoomLevels(const char* zoomLevels)
{
    CPLString _zoomLevels;
    if(zoomLevels) {
        _zoomLevels = zoomLevels;
    }
    else {
        _zoomLevels = property("zoom_levels", "", NG_ADDITIONS_KEY);
    }
    char** zoomLevelArray = CSLTokenizeString2(_zoomLevels, ",", 0);
    if(nullptr != zoomLevelArray) {
        int i = 0;
        const char* zoomLevel;
        m_zoomLevels.clear();
        while((zoomLevel = zoomLevelArray[i++]) != nullptr) {
            m_zoomLevels.insert(static_cast<unsigned char>(atoi(zoomLevel)));
        }
        CSLDestroy(zoomLevelArray);
    }
}

void FeatureClass::emptyFields(bool enable) const
{
    if(nullptr == m_layer) {
        return;
    }
    if(!enable) {
        m_layer->SetIgnoredFields(nullptr);
        return;
    }

    char** ignoreFields = nullptr;
    for(const char* fieldName : m_ignoreFields) {
        ignoreFields = CSLAddString(ignoreFields, fieldName);
    }
    m_layer->SetIgnoredFields(const_cast<const char**>(ignoreFields));
    CSLDestroy(ignoreFields);
    return;
}


bool FeatureClass::insertFeature(const FeaturePtr& feature, bool logEdits)
{
    bool result =  Table::insertFeature(feature, logEdits);
    if(!result) {
        return result;
    }

    OGRGeometry* geom = feature->GetGeometryRef();
    if(nullptr == geom) {
        return result;
    }
    OGREnvelope env;
    geom->getEnvelope(&env);
    Envelope extentBase = env;
    extentBase.fix();
    m_extent.merge(extentBase);

    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset || dataset->isBatchOperation()) {
        return result;
    }

    bool precisePixelSize = !(OGR_GT_Flatten(geom->getGeometryType()) == wkbPoint ||
                              OGR_GT_Flatten(geom->getGeometryType()) == wkbMultiPoint);

    GEOSGeometryPtr geosGeom(new GEOSGeometryWrap(geom));
    GIntBig fid = feature->GetFID();

    auto zoomLevelsList = zoomLevels();
    for(auto it = zoomLevelsList.rbegin(); it != zoomLevelsList.rend(); ++it) {
        unsigned char zoomLevel = *it;
        Envelope extent = extraExtentForZoom(zoomLevel, extentBase);
        std::vector<TileItem> items =
                MapTransform::getTilesForExtent(extent, zoomLevel, false, true);

        double step = FeatureClass::pixelSize(zoomLevel, precisePixelSize);
        geosGeom->simplify(step);

        for(auto tileItem : items) {

            Envelope ext = tileItem.env;
            ext.resize(TILE_RESIZE);

            auto vItem = tileGeometry(fid, geosGeom, ext);

            FeaturePtr tile = getTileFeature(tileItem.tile);
            VectorTile  vtile;
            bool create = true;
            if(tile) {
                int size = 0;
                GByte* data = tile->GetFieldAsBinary(tile->GetFieldIndex(
                                                         OVR_TILE_KEY), &size);
                Buffer buff(data, size, false);
                vtile.load(buff);
                create = false;
            }
            else {
                tile = OGRFeature::CreateFeature(m_ovrTable->GetLayerDefn());

                tile->SetField(OVR_ZOOM_KEY, tileItem.tile.z);
                tile->SetField(OVR_X_KEY, tileItem.tile.x);
                tile->SetField(OVR_Y_KEY, tileItem.tile.y);
            }
            vtile.add(vItem, true);

            // Add tile back
            if(vtile.isValid()) {
                BufferPtr data = vtile.save();
                tile->SetField(tile->GetFieldIndex(OVR_TILE_KEY), data->size(),
                                                     data->data());

                if(create) {
                    createTileFeature(tile);
                }
                else {
                    setTileFeature(tile);
                }
            }
        }
    }

    return result;
}

Envelope FeatureClass::extraExtentForZoom(unsigned char zoom, const Envelope& env)
{
    Envelope extent = env;
    int tilesInMapOneDim = 1 << zoom;
    double halfTilesInMapOneDim = tilesInMapOneDim * 0.5;
    double tilesSizeOneDim = DEFAULT_BOUNDS.maxX() / halfTilesInMapOneDim;
    double extraSize = tilesSizeOneDim * TILE_RESIZE - tilesSizeOneDim;
    extent.setMinX(extent.minX() - extraSize);
    extent.setMinY(extent.minY() - extraSize);
    extent.setMaxX(extent.maxX() + extraSize);
    extent.setMaxY(extent.maxY() + extraSize);
    return extent;
}

bool FeatureClass::updateFeature(const FeaturePtr& feature, bool logEdits)
{
    // Get previous geometry extent to get modified
    GIntBig id = feature->GetFID();
    FeaturePtr updateFeature = getFeature(id);
    if(!updateFeature) {
        return Table::updateFeature(feature, logEdits);
    }

    // 1. original feature has no geometry but feature has
    //       just add new geometries to tiles
    // 2. original feature has geometry and feature has
    //       remove old geometries and add new ones
    // 3. original feature has geometry and feature has not
    //       add new geometries

    OGRGeometry* originalGeom = updateFeature->GetGeometryRef();
    OGRGeometry* newGeom = feature->GetGeometryRef();
    Envelope extentBase;

    if(nullptr != originalGeom) {
        OGREnvelope env;
        originalGeom->getEnvelope(&env);
        extentBase = env;
    }

    if(nullptr != newGeom) {
        OGREnvelope env;
        newGeom->getEnvelope(&env);
        extentBase.merge(env);
    }

    extentBase.fix();


    bool result = Table::updateFeature(feature, logEdits);

    m_extent.merge(extentBase);

    if(!result) {
        return result;
    }    

    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset || dataset->isBatchOperation()) {
        return result;
    }

    bool precisePixelSize = !(OGR_GT_Flatten(geometryType()) == wkbPoint ||
                              OGR_GT_Flatten(geometryType()) == wkbMultiPoint);

    GEOSGeometryPtr geosGeom(new GEOSGeometryWrap(newGeom));
    GIntBig fid = feature->GetFID();

    auto zoomLevelsList = zoomLevels();
    for(auto it = zoomLevelsList.rbegin(); it != zoomLevelsList.rend(); ++it) {
        unsigned char zoomLevel = *it;
        Envelope extent = extraExtentForZoom(zoomLevel, extentBase);

        std::vector<TileItem> items =
                MapTransform::getTilesForExtent(extent, zoomLevel, false, true);

        double step = FeatureClass::pixelSize(zoomLevel, precisePixelSize);
        geosGeom->simplify(step);

        for(auto tileItem : items) {
            FeaturePtr tile = getTileFeature(tileItem.tile);
            VectorTile  vtile;
            bool create = true;
            if(tile) {
                int size = 0;
                GByte* data = tile->GetFieldAsBinary(tile->GetFieldIndex(OVR_TILE_KEY), &size);
                Buffer buff(data, size, false);
                vtile.load(buff);
                create = false;
            }
            else {
                tile = OGRFeature::CreateFeature(m_ovrTable->GetLayerDefn());

                tile->SetField(OVR_ZOOM_KEY, tileItem.tile.z);
                tile->SetField(OVR_X_KEY, tileItem.tile.x);
                tile->SetField(OVR_Y_KEY, tileItem.tile.y);
            }

            vtile.remove(id);

            Envelope env = tileItem.env;
            env.resize(TILE_RESIZE);
            auto vItem = tileGeometry(fid, geosGeom, env);
            vtile.add(vItem, true);

            // Add tile back
            if(vtile.isValid()) {
                BufferPtr data = vtile.save();
                tile->SetField(tile->GetFieldIndex(OVR_TILE_KEY), data->size(),
                                                     data->data());

                if(create) {
                    createTileFeature(tile);
                }
                else {
                    setTileFeature(tile);
                }
            }
        }
    }

    return result;
}

bool FeatureClass::deleteFeature(GIntBig id, bool logEdits)
{
    // Get previous geometry extent to get modified tiles
    FeaturePtr deleteFeature = getFeature(id);
    if(!deleteFeature) {
        return Table::deleteFeature(id, logEdits);
    }

    OGRGeometry* geom = deleteFeature->GetGeometryRef();
    OGREnvelope env;
    geom->getEnvelope(&env);

    bool result = Table::deleteFeature(id, logEdits);

    if(!result) {
        return result;
    }

    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset || dataset->isBatchOperation()) {
        return result;
    }

    for(auto zoomLevel : zoomLevels()) {
        Envelope extent = extraExtentForZoom(zoomLevel, env);
        std::vector<TileItem> items =
                MapTransform::getTilesForExtent(extent, zoomLevel, false, true);
        for(auto tileItem : items) {
            FeaturePtr tile = getTileFeature(tileItem.tile);

            VectorTile  vtile;
            if(tile) {
                int size = 0;
                GByte* data = tile->GetFieldAsBinary(tile->GetFieldIndex(OVR_TILE_KEY), &size);
                Buffer buff(data, size, false);
                if(vtile.load(buff)) {
                    vtile.remove(id);
                }
            }

            // Add tile back
            if(vtile.isValid()) {
                BufferPtr data = vtile.save();
                tile->SetField(tile->GetFieldIndex(OVR_TILE_KEY), data->size(),
                                                     data->data());

                setTileFeature(tile);
            }
            else if(tile) {
                result = m_ovrTable->DeleteFeature(tile->GetFID());
            }
        }
    }
    // Delete attachments
    result = deleteAttachments(id, logEdits);
    return result;
}


bool FeatureClass::deleteFeatures(bool logEdits)
{
    if(Table::deleteFeatures(logEdits)) {
        Dataset* dataset = dynamic_cast<Dataset*>(m_parent);
        if(nullptr != dataset) {
            return dataset->clearOverviewsTable(name());
        }
    }
    return false;
}

} // namespace ngs
