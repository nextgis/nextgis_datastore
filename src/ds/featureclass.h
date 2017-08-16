/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
#ifndef NGSFEATUREDATASET_H
#define NGSFEATUREDATASET_H

#include <algorithm>
#include <set>

#include "coordinatetransformation.h"
#include "geometry.h"
#include "ngstore/codes.h"
#include "table.h"
#include "util/buffer.h"
#include "util/options.h"
#include "util/threadpool.h"

namespace ngs {

class FeatureClass;
typedef std::shared_ptr<FeatureClass> FeatureClassPtr;

class VectorTileItem
{
    friend class VectorTile;
public:
    VectorTileItem();
    void addId(GIntBig id) { m_ids.insert(id); }
    void removeId(GIntBig id);
    void addPoint(const SimplePoint& pt) { m_points.push_back(pt); }
    void addIndex(unsigned short index) { m_indices.push_back(index); }
    void addBorderIndex(unsigned short ring, unsigned short index) {
        for(unsigned short i = 0; i < ring + 1; ++i) {
            m_borderIndices.push_back(std::vector<unsigned short>());
        }
        m_borderIndices[ring].push_back(index);
    }
    void addCentroid(const SimplePoint& pt) { m_centroids.push_back(pt); }

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
    bool operator==(const VectorTileItem& other) const {
        return m_points == other.m_points;
    }
    bool isIdsPresent(std::set<GIntBig> other) const {
        return std::includes(other.begin(), other.end(), m_ids.begin(), m_ids.end());
    }
protected:
    void loadIds(const VectorTileItem& item);
    void save(Buffer* buffer);
    bool load(Buffer& buffer);
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
    void add(const VectorTileItem &item, bool checkDuplicates = false);
    void remove(GIntBig id);
    BufferPtr save();
    bool load(Buffer& buffer);
    std::vector<VectorTileItem> items() const {
        return m_items;
    }
    bool empty() { return m_items.empty(); }

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
    explicit FeatureClass(OGRLayer* layer,
                 ObjectContainer* const parent = nullptr,
                 const enum ngsCatalogObjectType type = CAT_FC_ANY,
                 const CPLString & name = "");
    virtual ~FeatureClass();

    OGRwkbGeometryType geometryType() const;
    std::vector<OGRwkbGeometryType> geometryTypes();
    const char* geometryColumn() const;
    std::vector<const char*> geometryColumns() const;
    bool setIgnoredFields(const std::vector<const char*> fields =
            std::vector<const char*>());
    void setSpatialFilter(GeometryPtr geom);
    void setSpatialFilter(double minX, double minY, double maxX, double maxY);

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
    std::set<unsigned char> zoomLevels() const { return m_zoomLevels; }
    void addOverviewItem(const Tile& tile, const VectorTileItem& item) {
        CPLMutexHolder holder(m_genTileMutex, 150.0);
        m_genTiles[tile].add(item, true);
    }

    // static
    static const char* geometryTypeName(OGRwkbGeometryType type,
                enum GeometryReportType reportType = GeometryReportType::SIMPLE);
    static OGRwkbGeometryType geometryTypeFromName(const char* name);
    static OGRFieldType fieldTypeFromName(const char* name);
    static double pixelSize(int zoom);

    // Object interface
public:
    virtual bool destroy() override;

    // Table interface
public:
    virtual bool insertFeature(const FeaturePtr& feature) override;
    virtual bool updateFeature(const FeaturePtr& feature) override;
    virtual bool deleteFeature(GIntBig id) override;
    virtual bool deleteFeatures() override;

protected:
    VectorTileItem tileGeometry(const FeaturePtr &feature, OGRGeometry* extent,
                                float step) const;
    void fillZoomLevels(const char* zoomLevels = nullptr);

    void tilePoint(OGRGeometry* geom, OGRGeometry* extent, float step,
                   VectorTileItem* vitem) const;
    void tileLine(OGRGeometry* geom, OGRGeometry* extent, float step,
                   VectorTileItem* vitem)  const;
    void tilePolygon(OGRGeometry* geom, OGRGeometry* extent, float step,
                   VectorTileItem* vitem)  const;
    void tileMultiPoint(OGRGeometry* geom, OGRGeometry* extent, float step,
                   VectorTileItem* vitem)  const;
    void tileMultiLine(OGRGeometry* geom, OGRGeometry* extent, float step,
                   VectorTileItem* vitem)  const;
    void tileMultiPolygon(OGRGeometry* geom, OGRGeometry* extent, float step,
                   VectorTileItem* vitem)  const;
    bool getTilesTable();
    FeaturePtr getTileFeature(const Tile& tile);
    VectorTile getTileInternal(const Tile& tile);
    bool setTileFeature(FeaturePtr tile);
    bool createTileFeature(FeaturePtr tile);

    // static
protected:
    static bool tilingDataJobThreadFunc(ThreadData *threadData);

protected:
    OGRLayer* m_ovrTable;
    std::set<unsigned char> m_zoomLevels;
    CPLMutex* m_fieldsMutex;
    CPLMutex* m_genTileMutex;
    CPLMutex* m_layersMutex;
    std::vector<const char*> m_ignoreFields;
    Envelope m_extent;

private:
    class TilingData : public ThreadData {
    public:
        TilingData(FeatureClass* featureClass, FeaturePtr feature, bool own) :
            ThreadData(own), m_feature(feature), m_featureClass(featureClass) {}
        virtual ~TilingData() = default;
        FeaturePtr m_feature;
        FeatureClass* m_featureClass;
    };
    std::map<Tile, VectorTile> m_genTiles;
};

}

#endif // NGSFEATUREDATASET_H
