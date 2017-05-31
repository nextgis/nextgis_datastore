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

#include "coordinatetransformation.h"
#include "geometry.h"
#include "table.h"
#include "util/options.h"

namespace ngs {

class FeatureClass;
typedef std::shared_ptr<FeatureClass> FeatureClassPtr;

class VectorTile
{
public:
    void add(GIntBig fid, const SimplePoint& pt);
    void addIndex(GIntBig fid, unsigned short index) {
        m_indices[fid].push_back(index);
    }
    void addBorderIndex(GIntBig fid, unsigned short ring, unsigned short index) {
        if(m_borderIndices[fid].empty()) {
            for(unsigned short i = 0; i < ring + 1; ++i) {
                m_borderIndices[fid].push_back(std::vector<unsigned short>());
            }
        }
        m_borderIndices[fid][ring].push_back(index);
    }

    GByte* save();
    bool load(GByte* data);
    std::map<GIntBig, std::vector<SimplePoint>> points() const {
        return m_points;
    }
    const std::vector<unsigned short> indices(GIntBig fid) const {
        return m_indices.find(fid)->second;
    }
    const std::vector<std::vector<unsigned short>> borderIndices(GIntBig fid) const {
        return m_borderIndices.find(fid)->second;
    }

private:
    std::map<GIntBig, std::vector<SimplePoint>> m_points;
    std::map<GIntBig, std::vector<unsigned short>> m_indices;
    std::map<GIntBig, std::vector<std::vector<unsigned short>>> m_borderIndices; // NOTE: first array is exterior ring indices
    std::map<GIntBig, SimplePoint> m_centroids;
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
    virtual ~FeatureClass() = default;

    OGRwkbGeometryType geometryType() const;
    std::vector<OGRwkbGeometryType> geometryTypes();
    const char* geometryColumn() const;
    std::vector<const char*> geometryColumns() const;
    bool setIgnoredFields(const std::vector<const char*> fields =
            std::vector<const char*>());
    virtual int copyFeatures(const FeatureClassPtr srcFClass,
                             const FieldMapPtr fieldMap,
                             OGRwkbGeometryType filterGeomType,
                             const Progress& progress = Progress(),
                             const Options& options = Options());
    bool hasOverviews() const;
    int createOverviews(const Progress& progress = Progress(),
                        const Options& options = Options());
    // TODO: VectorTile getTile(const Tile& tile);

    // static
    static const char *geometryTypeName(OGRwkbGeometryType type,
                enum GeometryReportType reportType = GeometryReportType::SIMPLE);
    static OGRwkbGeometryType geometryTypeFromName(const char* name);

    // Object interface
public:
    virtual bool destroy() override;

protected:
    void tileGeometry(OGRGeometry* geometry);
    void fillZoomLevels(const char* zoomLevels);

protected:
    OGRLayer *m_ovrTable;
    std::vector<unsigned short> m_zoomLevels;
};

}

#endif // NGSFEATUREDATASET_H
