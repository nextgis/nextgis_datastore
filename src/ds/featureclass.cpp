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
#include "earcut.hpp"
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
constexpr unsigned short MAX_EDGE_INDEX = 65534;

//------------------------------------------------------------------------------
// VectorTileItem
//------------------------------------------------------------------------------

VectorTileItem::VectorTileItem() :
     m_valid(false),
     m_2d(true)
{

}

void VectorTileItem::removeId(GIntBig id)
{
    auto it = m_ids.find(id);
    if(it != m_ids.end()) {
        m_ids.erase(it);
        if(m_ids.empty()) {
            m_valid = false;
        }
    }
}

void VectorTileItem::addBorderIndex(unsigned short ring, unsigned short index)
{
    if(m_borderIndices.size() <= ring) {
       for(unsigned short i = static_cast<unsigned short>(m_borderIndices.size());
           i < ring + 1; ++i) {
            m_borderIndices.push_back(std::vector<unsigned short>());
        }
    }
    m_borderIndices[ring].push_back(index);
}

void VectorTileItem::save(Buffer* buffer)
{
    buffer->put(static_cast<GByte>(m_2d));

    // vector<SimplePoint> m_points
    buffer->put(static_cast<GUInt32>(m_points.size()));
    for(auto point : m_points) {
        if(m_2d) {
            buffer->put(point.x);
            buffer->put(point.y);
        }
        else {
            // TODO: Add point with z support
        }
    }

    // vector<unsigned short> m_indices
    buffer->put(static_cast<GUInt32>(m_indices.size()));
    for(auto index : m_indices) {
        buffer->put(index);
    }

    //vector<vector<unsigned short>> m_borderIndices
    buffer->put(static_cast<GUInt32>(m_borderIndices.size()));
    for(auto borderIndexArray : m_borderIndices) {
        buffer->put(static_cast<GUInt32>(borderIndexArray.size()));
        for(auto borderIndex : borderIndexArray) {
            buffer->put(borderIndex);
        }
    }

    // vector<SimplePoint> m_centroids
    buffer->put(static_cast<GUInt32>(m_centroids.size()));
    for(auto centroid : m_centroids) {
        if(m_2d) {
            buffer->put(centroid.x);
            buffer->put(centroid.y);
        }
        else {
            // TODO: Add point with z support
        }
    }

    // set<GIntBig> m_ids
    buffer->put(static_cast<GUInt32>(m_ids.size()));
    for(auto id : m_ids) {
        buffer->put(id);
    }
}

bool VectorTileItem::load(Buffer& buffer)
{
    m_2d = buffer.getByte();
    // vector<SimplePoint> m_points
    GUInt32 size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        if(m_2d) {
            float x = buffer.getFloat();
            float y = buffer.getFloat();
            SimplePoint pt = {x, y};
            m_points.push_back(pt);
        }
        else {
            // TODO: Add point with z support
        }
    }

    // vector<unsigned short> m_indices
    size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        m_indices.push_back(buffer.getUShort());
    }

    //vector<vector<unsigned short>> m_borderIndices
    size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        GUInt32 size1 = buffer.getULong();
        std::vector<unsigned short> array;
        for(GUInt32 j = 0; j < size1; ++j) {
            array.push_back(buffer.getUShort());
        }
        if(!array.empty()) {
            m_borderIndices.push_back(array);
        }
    }

    // vector<SimplePoint> m_centroids
    size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        if(m_2d) {
            float x = buffer.getFloat();
            float y = buffer.getFloat();
            SimplePoint pt = {x, y};
            m_centroids.push_back(pt);
        }
        else {
            // TODO: Add point with z support
        }
    }

    // set<GIntBig> m_ids
    size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        m_ids.insert(buffer.getBig());
    }

    m_valid = true;
    return true;
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

bool VectorTileItem::isIdsPresent(const std::set<GIntBig> &other, bool full) const
{
    if(other.empty())
        return false;

    if(full) {
        return std::includes(other.begin(), other.end(),
                             m_ids.begin(), m_ids.end());
    }
    else {
        return std::search(other.begin(), other.end(),
                           m_ids.begin(), m_ids.end()) != other.end();
    }
}
std::set<GIntBig> VectorTileItem::idsIntesect(const std::set<GIntBig> &other) const
{
    std::set<GIntBig> common_data;
    if(other.empty())
        return common_data;
    std::set_intersection(other.begin(), other.end(),
                          m_ids.begin(), m_ids.end(),
                          std::inserter(common_data, common_data.begin()));
    return common_data;
}


//------------------------------------------------------------------------------
// VectorTile
//------------------------------------------------------------------------------

void VectorTile::add(const VectorTileItem &item, bool checkDuplicates)
{
    if(!item.isValid()) {
        return;
    }
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

    if(!m_valid) {
        m_valid = !m_items.empty();
    }
}

void VectorTile::add(const VectorTileItemArray& items,
                     bool checkDuplicates)
{
    for(auto item : items) {
        add(item, checkDuplicates);
    }
}

void VectorTile::remove(GIntBig id)
{
    auto it = m_items.begin();
    while(it != m_items.end()) {
        (*it).removeId(id);
        if((*it).isValid() == false) {
            it = m_items.erase(it);
        }
        else {
            ++it;
        }
    }
}

BufferPtr VectorTile::save()
{
    BufferPtr buff(new Buffer);
    buff->put(static_cast<GUInt32>(m_items.size()));
    for(auto item : m_items) {
        item.save(buff.get());
    }
    return buff;
}

bool VectorTile::load(Buffer& buffer)
{
    GUInt32 size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        VectorTileItem item;
        item.load(buffer);
        m_items.push_back(item);
    }
    m_valid = true;
    return true;
}

//------------------------------------------------------------------------------
// FeatureClass
//------------------------------------------------------------------------------

FeatureClass::FeatureClass(OGRLayer* layer,
                           ObjectContainer * const parent,
                           const enum ngsCatalogObjectType type,
                           const CPLString &name) :
    Table(layer, parent, type, name),
    m_ovrTable(nullptr),
    m_genTileMutex(CPLCreateMutex()),
    m_layersMutex(CPLCreateMutex())
{
    CPLReleaseMutex(m_genTileMutex);
    CPLReleaseMutex(m_layersMutex);

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
    }

    getTilesTable();
}

FeatureClass::~FeatureClass()
{
    CPLDebug("ngstore", "CPLDestroyMutex(m_genTileMutex)");
    CPLDestroyMutex(m_genTileMutex);
    CPLDebug("ngstore", "CPLDestroyMutex(m_layersMutex)");
    CPLDestroyMutex(m_layersMutex);
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
                               _("Create feature failed. Source feature FID:%lld"),
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

double FeatureClass::pixelSize(int zoom)
{
    int tilesInMapOneDim = 1 << zoom;

    // Tile size. On ower zoom less size
    int tileSize = TILE_SIZE - (20 - zoom) * 8;

    long sizeOneDimPixels = tilesInMapOneDim * tileSize;
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

void FeatureClass::tilePoint(GIntBig fid, OGRGeometry* geom,
                             OGRGeometry */*extent*/, float step,
                             VectorTileItemArray& vitemArray) const
{
    VectorTileItem vitem;
    vitem.addId(fid);
    OGRPoint* pt = static_cast<OGRPoint*>(geom);
    vitem.addPoint(generalizePoint(pt, step));
    if(vitem.pointCount() > 0) {
        vitem.setValid(true);
        vitemArray.push_back(vitem);
    }
}

void FeatureClass::tileLine(GIntBig fid, OGRGeometry* geom, OGRGeometry* extent,
                            float step, VectorTileItemArray& vitemArray) const
{
    GeometryPtr cutGeom(geom->Intersection(extent));
    if(!cutGeom) {
        return;
    }

    if(OGR_GT_Flatten(cutGeom->getGeometryType()) == wkbMultiLineString) {
        return tileMultiLine(fid, cutGeom.get(), extent, step, vitemArray);
    }

    if(OGR_GT_Flatten(cutGeom->getGeometryType()) == wkbGeometryCollection) {
        OGRGeometryCollection* coll = ngsStaticCast(OGRGeometryCollection, cutGeom);
        for(int i = 0; i < coll->getNumGeometries(); ++i) {
            OGRGeometry* collGeom = coll->getGeometryRef(i);
            if(OGR_GT_Flatten(collGeom->getGeometryType()) == wkbMultiLineString) {
                tileMultiLine(fid, collGeom, extent, step, vitemArray);
            }
            else if(OGR_GT_Flatten(collGeom->getGeometryType()) == wkbLineString) {
                tileLine(fid, collGeom, extent, step, vitemArray);
            }
            else {
                CPLDebug("ngstore", "Tile not line");
            }
        }
        return;
    }

    if(OGR_GT_Flatten(cutGeom->getGeometryType()) != wkbLineString) {
        CPLDebug("ngstore", "Tile not line");
        return;
    }

    VectorTileItem vitem;
    vitem.addId(fid);
    OGRPoint pt;
    SimplePoint prevPt;
    OGRLineString* line = ngsStaticCast(OGRLineString, cutGeom);
    for(int i = 0; i < line->getNumPoints(); ++i) {
        line->getPoint(i, &pt);
        SimplePoint spt = generalizePoint(&pt, step);
        if(spt == prevPt) {
            continue;
        }
        prevPt = spt;
        vitem.addPoint(prevPt);
    }

    if(vitem.pointCount() > 0) {
        if(vitem.pointCount() == 1) {
            prevPt.x += 1.5f;
            prevPt.y += 1.5f;
            vitem.addPoint(prevPt);
        }
        vitem.setValid(true);
        vitemArray.push_back(vitem);
    }
}

typedef struct _edgePt {
    OGRRawPoint pr;
    unsigned short index;
} EDGE_PT;

typedef std::map<int, std::vector<EDGE_PT>> EDGES;

void setEdgeIndex(unsigned short index, double x, double y, EDGES& edges)
{
    for(auto& edge : edges) {
        for(EDGE_PT& edgeitem : edge.second) {
            if(isEqual(edgeitem.pr.x, x) && isEqual(edgeitem.pr.y, y)) {
                edgeitem.index = index;
                return;
            }
        }
    }
}

// The number type to use for tessellation
typedef double Coord;

// The index type. Defaults to uint32_t, but you can also pass uint16_t if you know that your
// data won't have more than 65536 vertices.
typedef unsigned short N;

// Create array
typedef std::array<Coord, 2> MBPoint;
typedef std::vector<MBPoint> MBVertices;
typedef std::vector<MBVertices> MBPolygon;

OGRPoint* findPointByIndex(N index, const MBPolygon& poly)
{
    N currentIndex = index;
    for(const auto& vertices : poly) {
        if(currentIndex >= vertices.size()) {
            currentIndex -= vertices.size();
            continue;
        }

        MBPoint pt = vertices[currentIndex];
        return new OGRPoint(pt[0], pt[1]);
    }
    return nullptr;
}

void addCenterPolygon(GIntBig fid, const SimplePoint& simpleCenter,
                   VectorTileItemArray& vitemArray) {
    VectorTileItem vitem;
    vitem.addId(fid);

    unsigned short index = 0;
    SimplePoint pt1 = {simpleCenter.x - 0.5f, simpleCenter.y - 0.5f};
    vitem.addPoint(pt1);
    unsigned short firstIndex = index;
    vitem.addBorderIndex(0, index);
    vitem.addIndex(index++);

    SimplePoint pt2 = {simpleCenter.x - 0.5f, simpleCenter.y + 0.5f};
    vitem.addPoint(pt2);
    vitem.addBorderIndex(0, index);
    vitem.addIndex(index++);

    SimplePoint pt3 = {simpleCenter.x + 0.5f, simpleCenter.y + 0.5f};
    vitem.addPoint(pt3);
    vitem.addBorderIndex(0, index);
    vitem.addIndex(index++);

    vitem.addBorderIndex(0, firstIndex); // Close ring

    vitem.setValid(true);
    vitemArray.push_back(vitem);
}

void FeatureClass::tilePolygon(GIntBig fid, OGRGeometry* geom,
                               OGRGeometry* extent, float step,
                               VectorTileItemArray& vitemArray) const
{
    GeometryPtr cutGeom(geom->Intersection(extent));
    if(!cutGeom) {
        return;
    }

    if(OGR_GT_Flatten(cutGeom->getGeometryType()) == wkbMultiPolygon) {
        return tileMultiPolygon(fid, cutGeom.get(), extent, step, vitemArray);
    }

    if(OGR_GT_Flatten(cutGeom->getGeometryType()) == wkbGeometryCollection) {
        OGRGeometryCollection* coll = ngsStaticCast(OGRGeometryCollection, cutGeom);
        for(int i = 0; i < coll->getNumGeometries(); ++i) {
            OGRGeometry* collGeom = coll->getGeometryRef(i);
            if(OGR_GT_Flatten(collGeom->getGeometryType()) == wkbMultiPolygon) {
                tileMultiPolygon(fid, collGeom, extent, step, vitemArray);
            }
            else if(OGR_GT_Flatten(collGeom->getGeometryType()) == wkbPolygon) {
                tilePolygon(fid, collGeom, extent, step, vitemArray);
            }
            else {
                CPLDebug("ngstore", "Tile not line");
            }
        }
        return;
    }

    if(OGR_GT_Flatten(cutGeom->getGeometryType()) != wkbPolygon) {
        CPLDebug("ngstore", "Tile not line");
        return;
    }

    OGREnvelope cutGeomExt;
    OGRPoint center;
    cutGeom->Centroid(&center);
    SimplePoint simpleCenter = generalizePoint(&center, step);

    // Check if polygon very small and occupy only one screen pixel
    cutGeom->getEnvelope(&cutGeomExt);
    Envelope cutGeomExtEnv = cutGeomExt;
    if(cutGeomExtEnv.width() < static_cast<double>(step) ||
            cutGeomExtEnv.height() < static_cast<double>(step)) {
        return addCenterPolygon(fid, simpleCenter, vitemArray);
    }

    // Prepare data
    OGRPolygon* poly = ngsStaticCast(OGRPolygon, cutGeom);
    EDGES edges;
    OGRPoint pt;

    // The number type to use for tessellation
    using Coord = double;

    // The index type. Defaults to uint32_t, but you can also pass uint16_t if you know that your
    // data won't have more than 65536 vertices.
    using N = unsigned short;

    // Create array
    using MBPoint = std::array<Coord, 2>;
    std::vector<std::vector<MBPoint>> polygon;

    OGRLinearRing* ring = poly->getExteriorRing();
    std::vector<MBPoint> exteriorRing;
    for(int i = 0; i < ring->getNumPoints(); ++i) { // Without last point
        ring->getPoint(i, &pt);
        double x = pt.getX();
        double y = pt.getY();
        edges[0].push_back({OGRRawPoint(x, y), MAX_EDGE_INDEX});

        MBPoint pt{ { x, y } };
        exteriorRing.emplace_back(pt);
    }

    polygon.emplace_back(exteriorRing);

    for(int i = 0; i < poly->getNumInteriorRings(); ++i) {
        ring = poly->getInteriorRing(i);
        std::vector<MBPoint> interiorRing;
        for(int j = 0; j < ring->getNumPoints(); ++j) {
            ring->getPoint(j, &pt);
            double x = pt.getX();
            double y = pt.getY();
            edges[i + 1].push_back({OGRRawPoint(x, y), MAX_EDGE_INDEX});

            MBPoint pt{ { x, y } };
            interiorRing.emplace_back(pt);
        }
        polygon.emplace_back(interiorRing);
    }

    VectorTileItem vitem;
    vitem.addId(fid);

    // Run tessellation
    // Returns array of indices that refer to the vertices of the input polygon.
    // Three subsequent indices form a triangle.
    std::vector<N> indices = mapbox::earcut<N>(polygon);
    if(indices.empty()) {
        return addCenterPolygon(fid, simpleCenter, vitemArray);
    }

    // Test without holes
    struct simpleAndOriginPt {
        SimplePoint pt;
        double x, y;
    };

    unsigned char tinIndex = 0;
    struct simpleAndOriginPt tin[3];
    unsigned short vertexIndex = 0;

    for(auto index : indices) {
        OGRPoint* pt = findPointByIndex(index, polygon);
        if(pt != nullptr) {
            tin[tinIndex] = {generalizePoint(pt, step), pt->getX(), pt->getY()};
            delete pt;
        }
        else {
            tin[tinIndex] = {{BIG_VALUE_F, BIG_VALUE_F}, BIG_VALUE, BIG_VALUE};
        }
        tinIndex++;

        if(tinIndex == 3) {
            tinIndex = 0;

            // Add tin and indexes
            if(tin[0].pt == tin[1].pt || tin[1].pt == tin[2].pt ||
                    tin[2].pt == tin[0].pt) {
                continue;
            }

            for(unsigned char j = 0; j < 3; ++j) {
                vitem.addPoint(tin[j].pt);
                // Check each vertex belongs to exterior or interior ring
                setEdgeIndex(vertexIndex, tin[j].x, tin[j].y, edges);

                vitem.addIndex(vertexIndex++);
            }
        }
    }
/*
    for(auto& tin: resultPolys) {
        for(unsigned char j = 0; j < 3; ++j) {
            SimplePoint pts = {tin[j].x, tin[j].y};
            vitem.addPoint(pts);
            // Check each vertex belongs to exterior or interior ring
            setEdgeIndex(index, tin[j].x, tin[j].y, edges);

            vitem.addIndex(index++);
        }
    }

    for(unsigned short i = 0; i < poly->getNumInteriorRings() + 1; ++i) {
        unsigned short firstIndex = MAX_EDGE_INDEX;
        for(auto& edge : edges[i]) {
            if(edge.index != MAX_EDGE_INDEX) {
                if(firstIndex == MAX_EDGE_INDEX) {
                    firstIndex = edge.index;
                }
                vitem.addBorderIndex(i, edge.index);
            }
        }
        vitem.addBorderIndex(i, firstIndex);
    }

    vitem.setValid(true);
    vitemArray.push_back(vitem);
*/
/*

    // Test without holes
    struct simpleAndOriginPt {
        SimplePoint pt;
        double x, y;
    };

    OGRGeometryCollection* tins = static_cast<OGRGeometryCollection*>(
                cutGeom->DelaunayTriangulation(0.0, 0));
    CPLDebug("ngstore", "DelaunayTriangulation returns %d tins", tins->getNumGeometries());
    for(int i = 0; i < tins->getNumGeometries(); ++i) {
        OGRPolygon* tin = static_cast<OGRPolygon*>(tins->getGeometryRef(i));
        if(tin->Intersects(poly)) {

            OGRPolygon* cutTin = static_cast<OGRPolygon*>(tin->Intersection(poly));

            if(cutTin->Within(poly)) { // Remove tins not overlaped poly

                char* wkt = nullptr;
                cutTin->exportToWkt(&wkt);
                CPLDebug("ngstore", "The tin %s is in polygon", wkt);
                CPLFree(wkt);

                ring = cutTin->getExteriorRing();
                struct simpleAndOriginPt pts[3];
                for(unsigned char j = 0; j < 3; ++j) {
                    ring->getPoint(j, &pt);
                    // Simplify tin
                    pts[j] = {generalizePoint(&pt, step), pt.getX(), pt.getY()};
                }

                // Add tin and indexes
                if(pts[0].pt == pts[1].pt || pts[1].pt == pts[2].pt ||
                        pts[2].pt == pts[0].pt) {
                    continue;
                }

                for(unsigned char j = 0; j < 3; ++j) {
                    vitem.addPoint(pts[j].pt);
                    // Check each vertex belongs to exterior or interior ring
                    setEdgeIndex(index, pts[j].x, pts[j].y, edges);

                    vitem.addIndex(index++);
                }
            }
        }
        else {
            char* wkt = nullptr;
            tin->exportToWkt(&wkt);
            CPLDebug("ngstore", "The tin %s is not in polygon", wkt);
            CPLFree(wkt);
        }
    }
*/
    // If all tins are filtered out, create triangle at the center of poly
    if(vitem.pointCount() == 0) {
        SimplePoint pt1 = {simpleCenter.x - 0.5f, simpleCenter.y - 0.5f};
        vitem.addPoint(pt1);
        unsigned short firstIndex = vertexIndex;
        vitem.addBorderIndex(0, vertexIndex);
        vitem.addIndex(vertexIndex++);

        SimplePoint pt2 = {simpleCenter.x - 0.5f, simpleCenter.y + 0.5f};
        vitem.addPoint(pt2);
        vitem.addBorderIndex(0, vertexIndex);
        vitem.addIndex(vertexIndex++);

        SimplePoint pt3 = {simpleCenter.x + 0.5f, simpleCenter.y + 0.5f};
        vitem.addPoint(pt3);
        vitem.addBorderIndex(0, vertexIndex);
        vitem.addIndex(vertexIndex++);

        vitem.addBorderIndex(0, firstIndex); // Close ring
    }
    else {
        for(unsigned short i = 0; i < poly->getNumInteriorRings() + 1; ++i) {
            unsigned short firstIndex = MAX_EDGE_INDEX;
            for(auto& edge : edges[i]) {
                if(edge.index != MAX_EDGE_INDEX) {
                    if(firstIndex == MAX_EDGE_INDEX) {
                        firstIndex = edge.index;
                    }
                    vitem.addBorderIndex(i, edge.index);
                }
            }
            vitem.addBorderIndex(i, firstIndex);
        }
    }

    vitem.setValid(true);
    vitemArray.push_back(vitem);
}

void FeatureClass::tileMultiPoint(GIntBig fid, OGRGeometry* geom,
                                  OGRGeometry* extent, float step,
                                  VectorTileItemArray& vitemArray) const
{
    OGRMultiPoint* mpoint = static_cast<OGRMultiPoint*>(geom);
    for(int i = 0; i < mpoint->getNumGeometries(); ++i) {
        tilePoint(fid, mpoint->getGeometryRef(i), extent, step, vitemArray);
    }
}

void FeatureClass::tileMultiLine(GIntBig fid, OGRGeometry* geom,
                                 OGRGeometry* extent, float step,
                                 VectorTileItemArray& vitemArray) const
{
    OGRMultiLineString* mline = static_cast<OGRMultiLineString*>(geom);
    for(int i = 0; i < mline->getNumGeometries(); ++i) {
        tileLine(fid, mline->getGeometryRef(i), extent, step, vitemArray);
    }
}

void FeatureClass::tileMultiPolygon(GIntBig fid, OGRGeometry* geom,
                                    OGRGeometry* extent, float step,
                                    VectorTileItemArray& vitemArray) const
{
    OGRMultiPolygon* mpoly = static_cast<OGRMultiPolygon*>(geom);
    for(int i = 0; i < mpoly->getNumGeometries(); ++i) {
        tilePolygon(fid, mpoly->getGeometryRef(i), extent, step, vitemArray);
    }
}

bool FeatureClass::getTilesTable()
{
    if(m_ovrTable) {
        return true;
    }

    CPLMutexHolder holder(m_layersMutex);
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

    CPLMutexHolder holder(m_genTileMutex);
    CPLAcquireMutex(m_featureMutex, 10.5);
    m_ovrTable->SetAttributeFilter(CPLSPrintf("%s = %d AND %s = %d AND %s = %d",
                                              OVR_X_KEY, tile.x,
                                              OVR_Y_KEY, tile.y,
                                              OVR_ZOOM_KEY, tile.z));
    FeaturePtr out(m_ovrTable->GetNextFeature());
    m_ovrTable->SetAttributeFilter(nullptr);
    CPLReleaseMutex(m_featureMutex);
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

    return m_ovrTable->SetFeature(tile) == OGRERR_NONE;
}

bool FeatureClass::createTileFeature(FeaturePtr tile)
{
    if(!getTilesTable()) {
        return false;
    }

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

    OGREnvelope env;
    geom->getEnvelope(&env);

    for(auto zoomLevel : data->m_featureClass->zoomLevels()) {
        Envelope extent = extraExtentForZoom(zoomLevel, env);

        std::vector<TileItem> items = MapTransform::getTilesForExtent(
                    extent, zoomLevel, false, true);
        for(auto tileItem : items) {
            float step = static_cast<float>(FeatureClass::pixelSize(
                                                tileItem.tile.z));
            Envelope ext = tileItem.env;
            ext.resize(TILE_RESIZE);
            auto vItems = data->m_featureClass->tileGeometry(
                        data->m_feature, ext.toGeometry(nullptr).get(), step);
            data->m_featureClass->addOverviewItem(tileItem.tile, vItems);
        }
    }

    return true;
}

int FeatureClass::createOverviews(const Progress &progress, const Options &options)
{
    m_genTiles.clear();
    bool force = options.boolOption("FORCE", false);
    if(!force && hasOverviews()) {
        return COD_SUCCESS;
    }

    Dataset* parentDS = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDS) {
        progress.onProgress(COD_CREATE_FAILED, 0.0,
                            _("Unsupported feature class"));
        return errorMessage(COD_CREATE_FAILED,
                            _("Unsupported feature class"));
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
        return COD_SUCCESS;
    }

    setProperty("zoom_levels", zoomLevelListStr, KEY_NG_ADDITIONS);

    // Tile and simplify geometry
    progress.onProgress(COD_IN_PROCESS, 0.0,
                        _("Start tiling and simplifying geometry"));

    // Multithreaded thread pool
    ThreadPool threadPool;
    threadPool.init(getNumberThreads(), tilingDataJobThreadFunc);

    FeaturePtr feature;
    setIgnoredFields(m_ignoreFields);
    reset();

    while((feature = nextFeature()) != nullptr) {
        threadPool.addThreadData(new TilingData(this, feature, true));
    }

    Progress newProgress(progress);
    newProgress.setTotalSteps(2);
    threadPool.waitComplete(newProgress);
    threadPool.clearThreadData();

    setIgnoredFields(std::vector<const char*>());
    reset();

    // Save tiles
    parentDS->startBatchOperation();

    double counter = 0.0;
    newProgress.setStep(1);
    for(auto item : m_genTiles) {
        if(!item.second.isValid()) {
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

    progress.onProgress(COD_FINISHED, 1.0,
                        _("Finish tiling and simplifying geometry"));
    return COD_SUCCESS;
}

VectorTile FeatureClass::getTile(const Tile& tile, const Envelope& tileExtent)
{
    VectorTile vtile;
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return vtile;
    }

    if(!extent().intersects(tileExtent)) {
        return vtile;
    }

    if(hasOverviews()) {
        if(tile.z <= *m_zoomLevels.rbegin()) {

            // Get closest zoom level
            unsigned char diff = 127;
            unsigned char z = 0;
            for(auto zoomLevel : m_zoomLevels) {
                unsigned char newDiff = static_cast<unsigned char>(
                            ABS(zoomLevel - tile.z));
                if(newDiff < diff) {
                    z = zoomLevel;
                    diff = newDiff;
                    if(diff == 0)
                        break;
                }
            }

            vtile = getTileInternal(tile);
//            if(vtile.isValid()) {
                return vtile;
//            }
        }
    }

    // Tiling on the fly
    CPLDebug("ngstore", "Tiling on the fly");

    // Calc grid step for zoom
    float step = static_cast<float>(pixelSize(tile.z));

    std::vector<FeaturePtr> features;
    OGREnvelope extEnv = tileExtent.toOgrEnvelope();

    // Lock threads here
    CPLAcquireMutex(m_featureMutex, 10.5);
    setIgnoredFields(m_ignoreFields);
    GeometryPtr extGeom = tileExtent.toGeometry(getSpatialReference());
    setSpatialFilter(extGeom);
    //reset();
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
    setIgnoredFields();
    setSpatialFilter();
    CPLReleaseMutex(m_featureMutex);

    while(!features.empty()) {
        feature = features.back();
        VectorTileItemArray items = tileGeometry(feature, extGeom.get(), step);
        if(!items.empty()) {
            vtile.add(items, false);
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

VectorTileItemArray FeatureClass::tileGeometry(const FeaturePtr &feature,
                                          OGRGeometry* extent, float step) const
{
    VectorTileItemArray out;
    OGRGeometry* geometry = feature->GetGeometryRef();
    if(nullptr == geometry) {
        return out;
    }

    switch(OGR_GT_Flatten(geometry->getGeometryType())) {
    case wkbPoint:
        tilePoint(feature->GetFID(), geometry, extent, step, out);
        break;
    case wkbLineString:
        tileLine(feature->GetFID(), geometry, extent, step, out);
        break;
    case wkbPolygon:
        tilePolygon(feature->GetFID(), geometry, extent, step, out);
        break;
    case wkbMultiPoint:
        tileMultiPoint(feature->GetFID(), geometry, extent, step, out);
        break;
    case wkbMultiLineString:
        tileMultiLine(feature->GetFID(), geometry, extent, step, out);
        break;
    case wkbMultiPolygon:
        tileMultiPolygon(feature->GetFID(), geometry, extent, step, out);
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

std::vector<OGRwkbGeometryType> FeatureClass::geometryTypes()
{
    std::vector<OGRwkbGeometryType> out;

    OGRwkbGeometryType geomType = geometryType();
    if (OGR_GT_Flatten(geomType) == wkbUnknown ||
            OGR_GT_Flatten(geomType) == wkbGeometryCollection) {

        std::vector<const char*> ignoreFields;
        OGRFeatureDefn* defn = definition();
        for(int i = 0; i < defn->GetFieldCount(); ++i) {
            OGRFieldDefn* fld = defn->GetFieldDefn(i);
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
                geomType = geom->getGeometryType();
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
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return false;
    }

    if(m_ovrTable) {
        m_ovrTable->ResetReading();
    }
    m_layer->SetSpatialFilter(nullptr);
    if(!Table::destroy()) {
        return false;
    }

    dataset->destroyOverviewsTable(name()); // Overviews table maybe not exists

    return true;
}

void FeatureClass::fillZoomLevels(const char* zoomLevels)
{
    CPLString _zoomLevels;
    if(zoomLevels) {
        _zoomLevels = zoomLevels;
    }
    else {
        _zoomLevels = getProperty("zoom_levels", "", KEY_NG_ADDITIONS);
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


bool FeatureClass::insertFeature(const FeaturePtr& feature)
{
    bool result =  Table::insertFeature(feature);
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

    for(auto zoomLevel : zoomLevels()) {
        Envelope extent = extraExtentForZoom(zoomLevel, extentBase);
        std::vector<TileItem> items =
                MapTransform::getTilesForExtent(extent, zoomLevel, false, true);
        for(auto tileItem : items) {
            float step = static_cast<float>(FeatureClass::pixelSize(
                                                tileItem.tile.z));
            Envelope ext = tileItem.env;
            ext.resize(TILE_RESIZE);
            auto vItem = tileGeometry(feature, ext.toGeometry(nullptr).get(),
                                      step);

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

bool FeatureClass::updateFeature(const FeaturePtr& feature)
{
    // Get previous geometry extent to get modified
    GIntBig id = feature->GetFID();
    FeaturePtr updateFeature = getFeature(id);
    if(!updateFeature) {
        return Table::updateFeature(feature);
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


    bool result =  Table::updateFeature(feature);

    m_extent.merge(extentBase);

    if(!result) {
        return result;
    }    

    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset || dataset->isBatchOperation()) {
        return result;
    }

    for(auto zoomLevel : zoomLevels()) {
        Envelope extent = extraExtentForZoom(zoomLevel, extentBase);

        std::vector<TileItem> items =
                MapTransform::getTilesForExtent(extent, zoomLevel, false, true);
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

            float step = static_cast<float>(FeatureClass::pixelSize(
                                                tileItem.tile.z));
            Envelope env = tileItem.env;
            env.resize(TILE_RESIZE);
            auto vItem = tileGeometry(feature, env.toGeometry(nullptr).get(),
                                      step);
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

bool FeatureClass::deleteFeature(GIntBig id)
{
    // Get previous geometry extent to get modified tiles
    FeaturePtr deleteFeature = getFeature(id);
    if(!deleteFeature) {
        return Table::deleteFeature(id);
    }

    OGRGeometry* geom = deleteFeature->GetGeometryRef();
    OGREnvelope env;
    geom->getEnvelope(&env);

    bool result = Table::deleteFeature(id);

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
    result = deleteAttachments(id);
    return result;
}


bool FeatureClass::deleteFeatures()
{
    if(Table::deleteFeatures()) {
        Dataset* dataset = dynamic_cast<Dataset*>(m_parent);
        if(nullptr != dataset) {
            return dataset->clearOverviewsTable(name());
        }
    }
    return false;
}

} // namespace ngs
