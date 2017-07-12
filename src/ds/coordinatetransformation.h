/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#ifndef NGSCOORDINATETRANSFORMATION_H
#define NGSCOORDINATETRANSFORMATION_H

#include <memory>

// gdal
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

namespace ngs {

typedef std::shared_ptr<OGRSpatialReference> SpatialReferencePtr;

/**
 * @brief The SpatialDataset interface class for datasets with spatial reference.
 */
class SpatialDataset {
public:
    SpatialDataset() : m_spatialReference(nullptr) {}
    virtual ~SpatialDataset() = default;
    virtual OGRSpatialReference *getSpatialReference() const {
        return m_spatialReference; }
protected:
    OGRSpatialReference* m_spatialReference;
};

class CoordinateTransformation
{
public:
    explicit CoordinateTransformation(OGRSpatialReference *srcSRS,
                             OGRSpatialReference *dstSRS);
    ~CoordinateTransformation();
    bool transform(OGRGeometry *geom);
protected:
    // no copy constructor
    CoordinateTransformation(const CoordinateTransformation& /*other*/) = default;
protected:
    OGRCoordinateTransformation* m_oCT;
};

}

#endif // NGSCOORDINATETRANSFORMATION_H
