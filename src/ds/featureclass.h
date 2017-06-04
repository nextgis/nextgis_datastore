/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
#ifndef NGSFEATUREDATASET_H
#define NGSFEATUREDATASET_H

#include <set>

#include "coordinatetransformation.h"
#include "geometry.h"
#include "table.h"
#include "util/options.h"

namespace ngs {

class FeatureClass;
typedef std::shared_ptr<FeatureClass> FeatureClassPtr;

class VectorTileItem
{
public:
    VectorTileItem();
    void addId(GIntBig id) { m_ids.insert(id); }
    void addPoint(const SimplePoint& pt) { m_points.push_back(pt); }
    void addIndex(unsigned short index) { m_indices.push_back(index); }
    void addBorderIndex(unsigned short ring, unsigned short index) {
        for(unsigned short i = 0; i < ring + 1; ++i) {
            m_borderIndices.push_back(std::vector<unsigned short>());
        }
        m_borderIndices[ring].push_back(index);
    }
    void addCentroid(const SimplePoint& pt) { m_centroids.push_back(pt); }

    GByte* save(int &size);
    bool load(GByte* data, int size);
    size_t pointCount() const { return m_points.size(); }
    const SimplePoint& point(size_t index) const { return m_points[index]; }
    bool isClosed() const;
    const std::vector<SimplePoint>& points() const { return m_points; }
    const std::vector<unsigned short>& indices() const { return m_indices; }
    const std::vector<std::vector<unsigned short>>& borderIndices() const {
        return m_borderIndices;
    }
    bool isValid() const { return m_valid; }
    void setValid(bool valid) { m_valid = valid; }
    void loadIds(const VectorTileItem& item);
    bool operator==(const VectorTileItem& other) const {
        return m_points == other.m_points;
    }
    bool isIdsPresent(std::set<GIntBig> other) const { return m_ids == other; }
private:
    std::vector<SimplePoint> m_points;
    std::vector<unsigned short> m_indices;
    std::vector<std::vector<unsigned short>> m_borderIndices; // NOTE: first array is exterior ring indices
    std::vector<SimplePoint> m_centroids;
    std::set<GIntBig> m_ids;
    bool m_valid;
    bool m_2d;
};

class VectorTile
{
public:
    VectorTile() : m_valid(false) {}
    void add(VectorTileItem item, bool checkDuplicates = false);
    GByte* save(int &size);
    bool load(GByte* data, int size);
    std::vector<VectorTileItem> items() const {
        return m_items;
    }

    bool isValid() const { return m_valid; }
private:
    std::vector<VectorTileItem> m_items;
    bool m_valid;
};

/**
 * @brief The FeatureClass class
 */
class FeatureClass : public Table, public SpatialDataset
{
public:
    enum class GeometryReportType {
        FULL,
        OGC,
        SIMPLE
    };

public:
    FeatureClass(OGRLayer * layer,
                 ObjectContainer * const parent = nullptr,
                 const enum ngsCatalogObjectType type = ngsCatalogObjectType::CAT_FC_ANY,
                 const CPLString & name = "");
    virtual ~FeatureClass();

    OGRwkbGeometryType geometryType() const;
    std::vector<OGRwkbGeometryType> geometryTypes();
    const char* geometryColumn() const;
    std::vector<const char*> geometryColumns() const;
    bool setIgnoredFields(const std::vector<const char*> fields =
            std::vector<const char*>());
    void setSpatialFilter(GeometryPtr geom);
    Envelope extent() const;
    virtual int copyFeatures(const FeatureClassPtr srcFClass,
                             const FieldMapPtr fieldMap,
                             OGRwkbGeometryType filterGeomType,
                             const Progress& progress = Progress(),
                             const Options& options = Options());
    bool hasOverviews() const;
    int createOverviews(const Progress& progress = Progress(),
                        const Options& options = Options());
    VectorTile getTile(const Tile& tile, const Envelope& tileExtent = Envelope());

    // static
    static const char *geometryTypeName(OGRwkbGeometryType type,
                enum GeometryReportType reportType = GeometryReportType::SIMPLE);
    static OGRwkbGeometryType geometryTypeFromName(const char* name);

    // Object interface
public:
    virtual bool destroy() override;

protected:
    VectorTileItem tileGeometry(const FeaturePtr &feature, OGRGeometry *extent,
                                float step) const;
    void fillZoomLevels(const char* zoomLevels);
    double pixelSize(int zoom) const;

    void tilePoint(OGRGeometry *geom, OGRGeometry *extent, float step,
                   VectorTileItem *vitem) const;
    void tileLine(OGRGeometry *geom, OGRGeometry *extent, float step,
                   VectorTileItem *vitem)  const;
    void tilePolygon(OGRGeometry *geom, OGRGeometry *extent, float step,
                   VectorTileItem *vitem)  const;
    void tileMultiPoint(OGRGeometry *geom, OGRGeometry *extent, float step,
                   VectorTileItem *vitem)  const;
    void tileMultiLine(OGRGeometry *geom, OGRGeometry *extent, float step,
                   VectorTileItem *vitem)  const;
    void tileMultiPolygon(OGRGeometry *geom, OGRGeometry *extent, float step,
                   VectorTileItem *vitem)  const;

protected:
    OGRLayer *m_ovrTable;
    std::vector<unsigned short> m_zoomLevels;
    CPLMutex* m_fieldsMutex;
    Envelope m_extent;
    std::vector<const char*> m_ignoreFields;
};

}

#endif // NGSFEATUREDATASET_H
