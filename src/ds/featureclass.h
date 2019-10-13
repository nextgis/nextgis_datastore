/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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

#include "coordinatetransformation.h"
#include "geometry.h"
#include "ngstore/codes.h"
#include "table.h"
#include "util/options.h"
#include "util/threadpool.h"

namespace ngs {

constexpr double TILE_RESIZE = 1.1;

class FeatureClass;
using FeatureClassPtr = std::shared_ptr<FeatureClass>;

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
    explicit FeatureClass(OGRLayer *layer,
                          ObjectContainer * const parent = nullptr,
                          const enum ngsCatalogObjectType type = CAT_FC_ANY,
                          const std::string &name = "");
    virtual ~FeatureClass() = default;

    OGRwkbGeometryType geometryType() const;
    std::vector<OGRwkbGeometryType> geometryTypes() const;
    std::string geometryColumn() const;
    std::vector<std::string> geometryColumns() const;
    bool setIgnoredFields(const std::vector<std::string> &fields =
            std::vector<std::string>());
    void setSpatialFilter(const GeometryPtr &geom = GeometryPtr());
    void setSpatialFilter(double minX, double minY, double maxX, double maxY);

    Envelope extent() const;
    virtual int copyFeatures(const FeatureClassPtr srcFClass,
                             const FieldMapPtr fieldMap,
                             OGRwkbGeometryType filterGeomType,
                             const Progress &progress = Progress(),
                             const Options &options = Options());
    bool hasOverviews() const;
    bool createOverviews(const Progress &progress = Progress(),
                         const Options &options = Options());
    VectorTile getTile(const Tile &tile, const Envelope &tileExtent = Envelope());
    std::set<unsigned char> zoomLevels() const { return m_zoomLevels; }
    void addOverviewItem(const Tile &tile, const VectorTileItemArray &items);

    // static
    static std::string geometryTypeName(OGRwkbGeometryType type,
                enum class GeometryReportType reportType = GeometryReportType::SIMPLE);
    static OGRwkbGeometryType geometryTypeFromName(const std::string &name);
    static OGRFieldType fieldTypeFromName(const std::string &name);
    static double pixelSize(int zoom, bool precize = false);
    static Envelope extraExtentForZoom(unsigned char zoom, const Envelope &env);

    // Object interface
public:
    virtual bool destroy() override;

    // Table interface
public:
    virtual bool insertFeature(const FeaturePtr &feature, bool logEdits = true) override;
    virtual bool updateFeature(const FeaturePtr &feature, bool logEdits = true) override;
    virtual bool deleteFeature(GIntBig id, bool logEdits = true) override;
    virtual bool deleteFeatures(bool logEdits = true) override;

protected:
    VectorTileItemArray tileGeometry(GIntBig fid, GEOSGeometryPtr geom,
                                     const Envelope &env) const;
    void fillZoomLevels(const std::string &zoomLevels = "");
    void emptyFields(bool enable = true) const;
/*
    void tileLine(GIntBig fid, OGRGeometry* geom, OGRGeometry* extent,
                  float step, VectorTileItemArray& vitemArray)  const;
    void tilePolygon(GIntBig fid, OGRGeometry* geom, OGRGeometry* extent,
                     float step, VectorTileItemArray& vitemArray)  const;
    void tileMultiLine(GIntBig fid, OGRGeometry* geom, OGRGeometry* extent,
                       float step, VectorTileItemArray& vitemArray)  const;
    void tileMultiPolygon(GIntBig fid, OGRGeometry* geom, OGRGeometry* extent,
                          float step, VectorTileItemArray& vitemArray)  const;
                          */

    bool hasTilesTable();
    FeaturePtr getTileFeature(const Tile &tile);
    VectorTile getTileInternal(const Tile &tile);
    bool setTileFeature(FeaturePtr tile);
    bool createTileFeature(FeaturePtr tile);

    // static
protected:
    static bool tilingDataJobThreadFunc(ThreadData *threadData);

protected:
    OGRLayer *m_ovrTable;
    std::set<unsigned char> m_zoomLevels;
    Mutex m_genTileMutex;
    std::vector<std::string> m_ignoreFields;
    Envelope m_extent;
    bool m_fastSpatialFilter;
    bool m_creatingOvr;

private:
    std::map<Tile, VectorTile> m_genTiles;
};

}

#endif // NGSFEATUREDATASET_H
