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
#ifndef FEATUREDATASET_H
#define FEATUREDATASET_H

#include "table.h"
#include "spatialdataset.h"

#include "ogr_spatialref.h"

#define ngsFeatureLoadSkipType(x) static_cast<unsigned int>( ngs::FeatureDataset::SkipType::x )

namespace ngs {

class CoordinateTransformationPtr
{
public:
    CoordinateTransformationPtr(
            const OGRSpatialReference *srcSRS, const OGRSpatialReference *dstSRS);
    ~CoordinateTransformationPtr();
    bool transform(OGRGeometry *geom);
protected:
    // no copy constructor
    CoordinateTransformationPtr(const CoordinateTransformationPtr& /*other*/) {}
protected:
    OGRCoordinateTransformation* m_oCT;
};

class FeatureDataset : public Table, public SpatialDataset
{
public:
    enum class SkipType : unsigned int {
        None            = 0x0000,
        EmptyGeometry   = 0x0001,
        InvalidGeometry = 0x0002
    };

    enum class GeometryReportType {
        Full,
        Ogc,
        Simple
    };

public:
    FeatureDataset(OGRLayer * const layer);
    virtual const OGRSpatialReference * getSpatialReference() const override;
    OGRwkbGeometryType getGeometryType() const;
    virtual int copyFeatures(const FeatureDataset *srcDataset,
                             const FieldMapPtr fieldMap,
                             OGRwkbGeometryType filterGeomType,
                             unsigned int skipGeometryFlags,
                             ngsProgressFunc progressFunc,
                             void *progressArguments);
    bool setIgnoredFields(const char **fields);
    // static
    static CPLString getGeometryTypeName(OGRwkbGeometryType type,
                                         enum GeometryReportType reportType);

};

}

#endif // FEATUREDATASET_H
