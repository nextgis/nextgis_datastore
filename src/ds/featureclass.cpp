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
#include "featureclass.h"

#include <algorithm>

#include "api_priv.h"
#include "dataset.h"
#include "coordinatetransformation.h"
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
constexpr unsigned short TILE_SIZE = 256;
constexpr double WORLD_WIDTH = DEFAULT_BOUNDS_X2.width();
constexpr double TILE_RESIZE = 1.1;  // FIXME: Is it enouth extra size for tile?

//------------------------------------------------------------------------------
// VectorTileItem
//------------------------------------------------------------------------------

VectorTileItem::VectorTileItem() :
     m_valid(false),
     m_2d(true)
{

}

GByte *VectorTileItem::save(int &size)
{
    size = 0;
    return nullptr;
}

bool VectorTileItem::load(GByte *data, int size)
{
     m_valid = false; return false;
}

bool VectorTileItem::isClosed() const
{
    return isEqual(m_points.front().x, m_points.back().x) &&
            isEqual(m_points.front().y, m_points.back().y);
}

void VectorTileItem::loadIds(const VectorTileItem &item)
{
    for(GIntBig id : item.m_ids) {
        m_ids.insert(id);
    }
}

//------------------------------------------------------------------------------
// VectorTile
//------------------------------------------------------------------------------

void VectorTile::add(VectorTileItem item, bool checkDuplicates)
{
    if(checkDuplicates) {
        auto it = std::find(m_items.begin(), m_items.end(), item);
        if(it == m_items.end()) {
            m_items.push_back(item);
        }
        else {
            (*it).loadIds(item);
        }
    }
    else {
        m_items.push_back(item);
    }
}

GByte* VectorTile::save(int &size)
{
    return nullptr;
}

bool VectorTile::load(GByte* data, int size)
{
    m_valid = true;
    return false;
}

// TODO: OGRGeometry::DelaunayTriangulation
// TODO: Check if triangle belong the interior ring - remove it

//------------------------------------------------------------------------------
// FeatureClass
//------------------------------------------------------------------------------

FeatureClass::FeatureClass(OGRLayer *layer,
                               ObjectContainer * const parent,
                               const enum ngsCatalogObjectType type,
                               const CPLString &name) :
    Table(layer, parent, type, name),
    m_ovrTable(nullptr),
    m_fieldsMutex(CPLCreateMutex())
{
    CPLReleaseMutex(m_fieldsMutex);
    if(m_layer) {
        if(m_layer->GetSpatialRef()) {
            m_spatialReference = m_layer->GetSpatialRef();
        }

        OGREnvelope env;
        OGRErr result = m_layer->GetExtent(&env, 1);
        if(result == OGRERR_NONE) {
            m_extent.set(env);
        }

        OGRFeatureDefn* defn = m_layer->GetLayerDefn();
        for(int i = 0; i < defn->GetFieldCount(); ++i) {
            OGRFieldDefn *fld = defn->GetFieldDefn(i);
            m_ignoreFields.push_back(fld->GetNameRef());
        }
        m_ignoreFields.push_back("OGR_STYLE");
    }
}

FeatureClass::~FeatureClass()
{
    CPLDestroyMutex(m_fieldsMutex);
}

OGRwkbGeometryType FeatureClass::geometryType() const
{
    if(nullptr == m_layer)
        return wkbUnknown;
    return m_layer->GetGeomType ();
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
    OGRFeatureDefn *defn = m_layer->GetLayerDefn();
    for(int i = 0; i < defn->GetGeomFieldCount(); ++i) {
        OGRGeomFieldDefn* geom = defn->GetGeomFieldDefn(i);
        out.push_back (geom->GetNameRef());
    }
    return out;
}


int FeatureClass::copyFeatures(const FeatureClassPtr srcFClass,
                               const FieldMapPtr fieldMap,
                               OGRwkbGeometryType filterGeomType,
                               const Progress& progress, const Options &options)
{
    if(!srcFClass) {
        return errorMessage(ngsCode::COD_COPY_FAILED, _("Source feature class is invalid"));
    }

    progress.onProgress(ngsCode::COD_IN_PROCESS, 0.0,
                       _("Start copy features from '%s' to '%s'"),
                       srcFClass->getName().c_str(), m_name.c_str());

    bool skipEmpty = options.boolOption("SKIP_EMPTY_GEOMETRY", false);
    bool skipInvalid = options.boolOption("SKIP_INVALID_GEOMETRY", false);
    bool toMulti = options.boolOption("FORCE_GEOMETRY_TO_MULTI", false);

    OGRSpatialReference *srcSRS = srcFClass->getSpatialReference();
    OGRSpatialReference *dstSRS = getSpatialReference();
    CoordinateTransformation CT(srcSRS, dstSRS);
    GIntBig featureCount = srcFClass->featureCount();
    OGRwkbGeometryType dstGeomType = geometryType();
    double counter = 0;
    srcFClass->reset();
    FeaturePtr feature;
    while((feature = srcFClass->nextFeature())) {
        double complete = counter / featureCount;
        if(!progress.onProgress(ngsCode::COD_IN_PROCESS, complete,
                           _("Copy in process ..."))) {
            return ngsCode::COD_CANCELED;
        }

        OGRGeometry * geom = feature->GetGeometryRef();
        OGRGeometry *newGeom = nullptr;
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
            if(!progress.onProgress(ngsCode::COD_WARNING, complete,
                               _("Create feature failed. Source feature FID:%lld"),
                               feature->GetFID ())) {
                return ngsCode::COD_CANCELED;
            }
        }
        counter++;
    }
    progress.onProgress(ngsCode::COD_FINISHED, 1.0, _("Done. Copied %d features"),
                       int(counter));

    return ngsCode::COD_SUCCESS;
}

bool FeatureClass::hasOverviews() const
{
    if(nullptr != m_ovrTable)
        return true;
    Dataset* const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset)
        return false;

    return dataset->getOverviewsTable(getName());
}

double FeatureClass::pixelSize(int zoom) const
{
    int tilesInMapOneDim = 1 << zoom;
    long sizeOneDimPixels = tilesInMapOneDim * TILE_SIZE;
    return WORLD_WIDTH / sizeOneDimPixels;
}

SimplePoint generalizePoint(OGRPoint* pt, float step)
{
    float x, y;
    if(isEqual(step, 0.0f)) {
        x = static_cast<float>(pt->getX());
        y = static_cast<float>(pt->getY());
    }
    else {
        x = static_cast<long>(pt->getX() / static_cast<double>(step)) * step;
        y = static_cast<long>(pt->getY() / static_cast<double>(step)) * step;
    }

    return {x, y};
}

void FeatureClass::tilePoint(OGRGeometry *geom, OGRGeometry */*extent*/, float step,
                             VectorTileItem *vitem) const
{
    OGRPoint* pt = static_cast<OGRPoint*>(geom);
    vitem->addPoint(generalizePoint(pt, step));
}

void FeatureClass::tileLine(OGRGeometry *geom, OGRGeometry *extent, float step,
                            VectorTileItem *vitem) const
{
/*

//    GeometryPtr cutGeom(geometry->Intersection(extent));
//    if(!cutGeom) {
//        return VectorTileItem();
//    }

    OGREnvelope extent;
    geometry->getEnvelope(&extent);
    Envelope geometryExtent(extent);

    // TODO: Tile and simplify geometry



    //    vtile.add(0, { static_cast<float>(ext.getMinX()), static_cast<float>(ext.getMinY()) });
    //    vtile.add(0, { static_cast<float>(ext.getMinX()), static_cast<float>(ext.getMaxY()) });
    //    vtile.add(0, { static_cast<float>(ext.getMaxX()), static_cast<float>(ext.getMaxY()) });
    //    vtile.add(0, { static_cast<float>(ext.getMaxX()), static_cast<float>(ext.getMinY()) });
    //    vtile.add(0, { static_cast<float>(ext.getMinX()), static_cast<float>(ext.getMinY()) });
    //    OGRRawPoint center = ext.getCenter();
    //        vtile.add(0, { static_cast<float>( ext.getMinX() + (center.x - ext.getMinX()) / 2), static_cast<float>(center.y) });
    //    vtile.add(0, { static_cast<float>(ext.getMaxX()), static_cast<float>(ext.getMaxY()) });


        // Polygon
        OGRRawPoint center = ext.getCenter();
        vtile.add(0, { static_cast<float>( ext.getMinX() + (center.x - ext.getMinX()) / 2), static_cast<float>(center.y) });
        vtile.add(0, { static_cast<float>(center.x), static_cast<float>(ext.getMaxY()) });
        vtile.add(0, { static_cast<float>(center.x), static_cast<float>(ext.getMinY()) });
        vtile.addIndex(0, 0);
        vtile.addIndex(0, 1);
        vtile.addIndex(0, 2);

        vtile.add(0, { static_cast<float>(ext.getMaxX()), static_cast<float>(ext.getMinY()) });
        vtile.addIndex(0, 1);
        vtile.addIndex(0, 2);
        vtile.addIndex(0, 3);

        vtile.add(0, { static_cast<float>(ext.getMaxX()), static_cast<float>(ext.getMaxY()) });
        vtile.addIndex(0, 1);
        vtile.addIndex(0, 3);
        vtile.addIndex(0, 4);

        vtile.addBorderIndex(0, 0, 0);
        vtile.addBorderIndex(0, 0, 1);
        vtile.addBorderIndex(0, 0, 4);
        vtile.addBorderIndex(0, 0, 3);
        vtile.addBorderIndex(0, 0, 2);
        vtile.addBorderIndex(0, 0, 0);
*/

/*        GByte* geomBlob = (GByte*) VSI_MALLOC_VERBOSE(blobLen);
    if( geomBlob != nullptr && simpleGeom->exportToWkb(
                wkbNDR, geomBlob) == OGRERR_NONE )
        dstFeature->SetField (dstFeature->GetFieldIndex(
            CPLSPrintf ("ngs_geom_%d", sampleDist.second)),
            blobLen, geomBlob);
    CPLFree(geomBlob);*/
}

void FeatureClass::tilePolygon(OGRGeometry *geom, OGRGeometry *extent, float step,
                               VectorTileItem *vitem) const
{

}

void FeatureClass::tileMultiPoint(OGRGeometry *geom, OGRGeometry *extent,
                                  float step, VectorTileItem *vitem) const
{
    OGRMultiPoint* mpoint = static_cast<OGRMultiPoint*>(geom);
    for(int i = 0; i < mpoint->getNumGeometries(); ++i) {
        tilePoint(mpoint->getGeometryRef(i), extent, step, vitem);
    }
}

void FeatureClass::tileMultiLine(OGRGeometry *geom, OGRGeometry *extent,
                                 float step, VectorTileItem *vitem) const
{
    OGRMultiLineString* mline = static_cast<OGRMultiLineString*>(geom);
    for(int i = 0; i < mline->getNumGeometries(); ++i) {
        tileLine(mline->getGeometryRef(i), extent, step, vitem);
    }
}

void FeatureClass::tileMultiPolygon(OGRGeometry *geom, OGRGeometry *extent,
                                    float step, VectorTileItem *vitem) const
{
    OGRMultiPolygon* mpoly = static_cast<OGRMultiPolygon*>(geom);
    for(int i = 0; i < mpoly->getNumGeometries(); ++i) {
        tileLine(mpoly->getGeometryRef(i), extent, step, vitem);
    }
}

int FeatureClass::createOverviews(const Progress &progress, const Options &options)
{
    bool force = options.boolOption("FORCE", false);
    if(!force && hasOverviews()) {
        return ngsCode::COD_SUCCESS;
    }

    Dataset* parentDS = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDS) {
        progress.onProgress(ngsCode::COD_CREATE_FAILED, 0.0,
                            _("Unsupported feature class"));
        return errorMessage(ngsCode::COD_CREATE_FAILED,
                            _("Unsupported feature class"));
    }

    m_ovrTable = parentDS->getOverviewsTable(getName());
    if(nullptr == m_ovrTable) {
        m_ovrTable = parentDS->createOverviewsTable(getName());
    }
    else {
        parentDS->clearOverviewsTable(getName());
    }

    if(options.boolOption("CREATE_OVERVIEWS_TABLE", false)) {
        return ngsCode::COD_SUCCESS;
    }

    // Fill overview layer with data
    const CPLString &zoomLevelListStr = options.stringOption(
                ZOOM_LEVELS_OPTION, "");
    fillZoomLevels(zoomLevelListStr);
    if(m_zoomLevels.empty()) {
        return ngsCode::COD_SUCCESS;
    }

    CPLString key(getName());
    key += ".zoom_levels";

    parentDS->setMetadata(key, zoomLevelListStr);

    // Tile and simplify geometry
    progress.onProgress(ngsCode::COD_IN_PROCESS, 0.0,
                        _("Start tiling and simplifieng geometry"));

    // TODO: multithreaded thread pool
/*    FeaturePtr feature;
    GIntBig count = featureCount(false);
    double counter = 0;
    float step = static_cast<float>(pixelSize(tile.z));
    Envelope ext = tileExtent;
    ext.resize(TILE_RESIZE);
    GeometryPtr extGeom = ext.toGeometry(getSpatialReference());

    while((feature = nextFeature()) != nullptr) {
        progress.onProgress(ngsCode::COD_IN_PROCESS, counter++ / count,
                            _("Proceeding..."));
        tileGeometry(feature, extGeom, step);
    }
*/
    return ngsCode::COD_SUCCESS;
}

VectorTile FeatureClass::getTile(const Tile& tile, const Envelope& tileExtent)
{
    VectorTile vtile;
    Dataset* const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return vtile;
    }

    if(!m_extent.intersects(tileExtent)) {
        return vtile;
    }

    if(hasOverviews() && tile.z <= m_zoomLevels.back()) {
        // Get closet zoom level
        int diff = 32000;
        unsigned short z = m_zoomLevels.front();
        for(auto zoomLevel : m_zoomLevels) {
            int newDiff = abs(zoomLevel - tile.z);
            if(newDiff < diff) {
                z = zoomLevel;
                diff = newDiff;
            }
        }

        vtile = dataset->getTile(getName(), tile.x, tile.y, z);
        if(vtile.isValid()) {
            return vtile;
        }
    }

    // Tiling on the fly

    // Calc grid step for zoom
    float step = static_cast<float>(pixelSize(tile.z)); // 0.0f;//

    Envelope ext = tileExtent;
    ext.resize(TILE_RESIZE);
    std::vector<FeaturePtr> features;
    OGREnvelope extEnv = ext.toOgrEnvelope();

    // Lock threads here
    CPLAcquireMutex(m_fieldsMutex, 50.0);
//    setSpatialFilter(extGeom);
    setIgnoredFields(m_ignoreFields);
    reset();
    FeaturePtr feature;
    while((feature = nextFeature())) {
        OGRGeometry* geom = feature->GetGeometryRef();
        if(geom) {
            OGREnvelope env;
            geom->getEnvelope(&env);
            if(env.IsInit() && env.Intersects(extEnv)) {
                features.push_back(feature);
            }
        }
    }
//    setSpatialFilter(GeometryPtr());
    setIgnoredFields(std::vector<const char*>());
    CPLReleaseMutex(m_fieldsMutex);

    if(!features.empty()) {
    GeometryPtr extGeom = ext.toGeometry(getSpatialReference());
    while(!features.empty()) {
        auto feature = features.back();
        VectorTileItem item = tileGeometry(feature, extGeom.get(), step);
        if(item.isValid()) {
            vtile.add(item, false);
        }
        features.pop_back();
    }
    }

//    TablePtr features = dataset->executeSQL(CPLSPrintf("SELECT * FROM %s",
//                                                       getName().c_str()),
//                                            extGeom);
//    if(features) {
//        FeaturePtr feature;
//        while(true) {
//            CPLAcquireMutex(m_fieldsMutex, 1000.0);
//            feature = features->nextFeature();
//            CPLReleaseMutex(m_fieldsMutex);
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

VectorTileItem FeatureClass::tileGeometry(const FeaturePtr &feature,
                                          OGRGeometry *extent, float step) const
{
    OGRGeometry *geometry = feature->GetGeometryRef();
    if(nullptr == geometry) {
        return VectorTileItem();
    }

    VectorTileItem vitem;
    vitem.addId(feature->GetFID());
    switch(OGR_GT_Flatten(geometry->getGeometryType())) {
    case wkbPoint:
        tilePoint(geometry, extent, step, &vitem);
        break;
    case wkbLineString:
        tileLine(geometry, extent, step, &vitem);
        break;
    case wkbPolygon:
        tilePolygon(geometry, extent, step, &vitem);
        break;
    case wkbMultiPoint:
        tileMultiPoint(geometry, extent, step, &vitem);
        break;
    case wkbMultiLineString:
        tileMultiLine(geometry, extent, step, &vitem);
        break;
    case wkbMultiPolygon:
        tileMultiPolygon(geometry, extent, step, &vitem);
        break;
    case wkbGeometryCollection:
    case wkbCircularString:
    case wkbCompoundCurve:
    case wkbCurvePolygon:
    case wkbMultiCurve:
    case wkbMultiSurface:
    case wkbCurve:
    case wkbSurface:
    case wkbPolyhedralSurface:
    case wkbTIN:
    case wkbTriangle:
    default:
        break; // Not supported yet
    }

    if(vitem.pointCount() > 0) {
        vitem.setValid(true);
    }
    return vitem;
}

bool FeatureClass::setIgnoredFields(const std::vector<const char *> fields)
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
    bool result = m_layer->SetIgnoredFields(
                const_cast<const char**>(ignoreFields)) == OGRERR_NONE;
    CSLDestroy(ignoreFields);
    return result;
}

void FeatureClass::setSpatialFilter(GeometryPtr geom)
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

Envelope FeatureClass::extent() const
{
    if(nullptr == m_layer) {
        return Envelope();
    }

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

std::vector<OGRwkbGeometryType> FeatureClass::geometryTypes()
{
    std::vector<OGRwkbGeometryType> out;

    OGRwkbGeometryType geomType = geometryType();
    if (OGR_GT_Flatten(geomType) == wkbUnknown ||
            OGR_GT_Flatten(geomType) == wkbGeometryCollection) {

        std::vector<const char*> ignoreFields;
        OGRFeatureDefn* defn = definition();
        for(int i = 0; i < defn->GetFieldCount(); ++i) {
            OGRFieldDefn *fld = defn->GetFieldDefn(i);
            ignoreFields.push_back(fld->GetNameRef());
        }
        ignoreFields.push_back("OGR_STYLE");
        setIgnoredFields(ignoreFields);
        reset();
        std::map<OGRwkbGeometryType, int> counts;
        FeaturePtr feature;
        while((feature = nextFeature())) {
            OGRGeometry * const geom = feature->GetGeometryRef();
            if (nullptr != geom) {
                OGRwkbGeometryType geomType = geom->getGeometryType();
                counts[OGR_GT_Flatten(geomType)] += 1;
            }
        }
        ignoreFields.clear();
        setIgnoredFields(ignoreFields);

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
    Dataset* const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset)
        return false;

    if(!Table::destroy()) {
        return false;
    }

    dataset->destroyOverviewsTable(getName()); // Overviews table maybe not exists

    return true;
}

void FeatureClass::fillZoomLevels(const char *zoomLevels)
{
    char** zoomLevelArray = CSLTokenizeString2(zoomLevels, ",", 0);
    int i = 0;
    const char* zoomLevel;
    while((zoomLevel = zoomLevelArray[i++]) != nullptr) {
        m_zoomLevels.push_back(static_cast<unsigned short>(atoi(zoomLevel)));
    }
    CSLDestroy(zoomLevelArray);
}

/*
char RenderLayer::getCloseOvr()
{
    float diff = 18;
    float currentDiff;
    char zoom = 18;
    for(auto sampleDist : gSampleDists) {
        currentDiff = sampleDist.second - m_renderZoom;
        if(currentDiff > 0 && currentDiff < diff) {
            diff = currentDiff;
            zoom = sampleDist.second;
        }
    }
    return zoom;
}

ResultSetPtr FeatureClass::getGeometries(unsigned char zoom,
                                           GeometryPtr spatialFilter) const
{
    // check geometry column
    CPLString geomFieldName;
    //bool originalGeom = true;
    //if(m_renderZoom > 15) {
        geomFieldName = getGeometryColumn ();
    }
    else {
        char zoom = getCloseOvr();
        if(zoom < 18) {
            geomFieldName.Printf ("ngs_geom_%d, %s", zoom,
                                  featureDataset->getGeometryColumn ().c_str ());
            originalGeom = false;
        }
        else {
            geomFieldName = featureDataset->getGeometryColumn ();
        }
    }

    CPLString statement;
    statement.Printf ("SELECT %s,%s FROM %s", getFIDColumn().c_str(), geomFieldName.c_str (),
                      name ().c_str ());

    return executeSQL (statement, spatialFilter, "");
}

*/

} // namespace ngs
