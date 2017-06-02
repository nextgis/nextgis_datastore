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

//------------------------------------------------------------------------------
// VectorTileItem
//------------------------------------------------------------------------------

bool VectorTileItem::isClosed() const
{
    return isEqual(m_points.front().x, m_points.back().x) &&
                    isEqual(m_points.front().y, m_points.back().y);
}
//------------------------------------------------------------------------------
// VectorTile
//------------------------------------------------------------------------------

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
    m_ovrTable(nullptr)
{
    if(m_layer && m_layer->GetSpatialRef())
        m_spatialReference = m_layer->GetSpatialRef();
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

    bool skipEmpty = options.getBoolOption("SKIP_EMPTY_GEOMETRY", false);
    bool skipInvalid = options.getBoolOption("SKIP_INVALID_GEOMETRY", false);
    bool toMulti = options.getBoolOption("FORCE_GEOMETRY_TO_MULTI", false);

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

int FeatureClass::createOverviews(const Progress &progress, const Options &options)
{
    bool force = options.getBoolOption("FORCE", false);
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

    if(options.getBoolOption("CREATE_OVERVIEWS_TABLE", false)) {
        return ngsCode::COD_SUCCESS;
    }

    // Fill overview layer with data
    const CPLString &zoomLevelListStr = options.getStringOption(
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
    FeaturePtr feature;
    GIntBig count = featureCount(false);
    double counter = 0;
    while((feature = nextFeature()) != nullptr) {
        progress.onProgress(ngsCode::COD_IN_PROCESS, counter++ / count,
                            _("Proceeding..."));
        tileGeometry(feature->GetGeometryRef());
    }

    return ngsCode::COD_SUCCESS;
}

VectorTile FeatureClass::getTile(const Tile& tile, const Envelope& tileExtent) const
{
    VectorTile vtile;
    Dataset* const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
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

    // Tile on the fly

    // Calc grid step for zoom
    float step = static_cast<float>(pixelSize(tile.z));

    GeometryPtr spatFilter = tileExtent.toGeometry(getSpatialReference());
    TablePtr features = dataset->executeSQL("", spatFilter);
    FeaturePtr feature;
    Envelope ext = tileExtent;
    ext.resize(1.1); // FIXME: Is it enouth extra size for tile?
    GeometryPtr extGeom = ext.toGeometry(getSpatialReference());

    // TODO: Exclude duplicated points, 2 point lines and 4 point polygons

    while((feature = nextFeature()) != nullptr) {
        VectorTileItem item = tileGeometry(feature->GetGeometryRef(),
                                           extGeom.get(), step);
        if(item.isValid()) {
            vtile.add(feature->GetFID(), item);
        }
    }

    return vtile;
}

VectorTileItem FeatureClass::tileGeometry(OGRGeometry *geometry,
                                          OGRGeometry *extent, float step) const
{
    if(nullptr == geometry) {
        return VectorTileItem();
    }

    GeometryPtr cutGeom(geometry->Intersection(extent));
    if(!cutGeom) {
        return VectorTileItem();
    }

    VectorTileItem vitem;

    switch(OGR_GT_Flatten(geometry->getGeometryType())) {
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
        break;
    }

    vitem.setValid(true);
    return vitem;


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


/*        GByte* geomBlob = (GByte*) VSI_MALLOC_VERBOSE(blobLen);
    if( geomBlob != nullptr && simpleGeom->exportToWkb(
                wkbNDR, geomBlob) == OGRERR_NONE )
        dstFeature->SetField (dstFeature->GetFieldIndex(
            CPLSPrintf ("ngs_geom_%d", sampleDist.second)),
            blobLen, geomBlob);
    CPLFree(geomBlob);*/
}

bool FeatureClass::setIgnoredFields(const std::vector<const char *> fields)
{
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
