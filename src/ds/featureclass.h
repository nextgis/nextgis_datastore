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
#include "table.h"
#include "util/options.h"

namespace ngs {

class FeatureClass;
typedef std::shared_ptr<FeatureClass> FeatureClassPtr;

class FeatureClass : public Table, public ISpatialDataset
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

    OGRwkbGeometryType getGeometryType() const;
    std::vector<OGRwkbGeometryType> getGeometryTypes();
    const char* getGeometryColumn() const;
    std::vector<const char*> getGeometryColumns() const;
    bool setIgnoredFields(const std::vector<const char*> fields =
            std::vector<const char*>());
    virtual int copyFeatures(const FeatureClassPtr srcFClass,
                             const FieldMapPtr fieldMap,
                             OGRwkbGeometryType filterGeomType,
                             const Progress& progress = Progress(),
                             const Options& options = Options());
    // static
    static const char *getGeometryTypeName(OGRwkbGeometryType type,
                enum GeometryReportType reportType = GeometryReportType::SIMPLE);
    static OGRwkbGeometryType getGeometryTypeFromName(const char* name);


    // ISpatialDataset interface
public:
    virtual OGRSpatialReference *getSpatialReference() const override;
};

}

#endif // NGSFEATUREDATASET_H
