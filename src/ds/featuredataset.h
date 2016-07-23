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

namespace ngs {

class CoordinateTransformationPtr
{
public:
    CoordinateTransformationPtr(OGRCoordinateTransformation* ct);
    ~CoordinateTransformationPtr();
    static CoordinateTransformationPtr create(const OGRSpatialReference *srcSRS,
                                              const OGRSpatialReference *dstSRS);

    bool transform(OGRGeometry *geom);
protected:
    OGRCoordinateTransformation* m_oCT;
};

class FeatureDataset : public Table, public SpatialDataset
{
public:
    enum SkipType {
        NONE             = 0x0000,
        EMPTY_GEOMETRY   = 0x0001,
        INVALID_GEOMETRY = 0x0002
    };

    enum GeometryReportType {
        FULL,
        OGC,
        SIMPLE
    };

public:
    FeatureDataset(OGRLayer * const layer);
    virtual const OGRSpatialReference * getSpatialReference() const override;
    OGRwkbGeometryType getGeometryType() const;
    int copyFeatures(const FeatureDataset *pSrcDataset, const FieldMapPtr fieldMap,
                     OGRwkbGeometryType filterGeomType,
                     unsigned int skipGeometryFlags, ngsProgressFunc progressFunc,
                     void *progressArguments);
    bool setIgnoredFields(const char **fields);
    // static
    static CPLString getGeometryTypeName(OGRwkbGeometryType type,
                                         enum GeometryReportType reportType);

};

}

#endif // FEATUREDATASET_H
