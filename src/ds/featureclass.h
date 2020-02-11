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

constexpr const char* OGR_STYLE_FIELD = "OGR_STYLE";

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
    virtual OGRwkbGeometryType geometryType() const;
    virtual std::vector<OGRwkbGeometryType> geometryTypes() const;
    std::string geometryColumn() const;
    std::vector<std::string> geometryColumns() const;
    bool setIgnoredFields(const std::vector<std::string> &fields =
            std::vector<std::string>());
    void setSpatialFilter(const GeometryPtr &geom = GeometryPtr());
    void setSpatialFilter(double minX, double minY, double maxX, double maxY);

    virtual Envelope extent() const;
    virtual int copyFeatures(const FeatureClassPtr srcFClass,
                             const FieldMapPtr fieldMap,
                             OGRwkbGeometryType filterGeomType,
                             const Progress &progress = Progress(),
                             const Options &options = Options());

    // static
    static std::string geometryTypeName(OGRwkbGeometryType type,
                enum GeometryReportType reportType = GeometryReportType::SIMPLE);
    static OGRwkbGeometryType geometryTypeFromName(const std::string &name);

    // Table interface
protected:
    virtual void onFeatureInserted(FeaturePtr feature) override;
    virtual void onFeatureUpdated(FeaturePtr oldFeature,
                                  FeaturePtr newFeature) override;

protected:
    void emptyFields(bool enable = true) const;
    void init();

protected:
    std::vector<std::string> m_ignoreFields;
    mutable Envelope m_extent;
    bool m_fastSpatialFilter;
};

} // namespace ngs

#endif // NGSFEATUREDATASET_H
