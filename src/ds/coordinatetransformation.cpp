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
#include "coordinatetransformation.h"

namespace ngs {

//------------------------------------------------------------------------------
// SpatialDataset
//------------------------------------------------------------------------------

SpatialDataset::SpatialDataset() : m_spatialReference(nullptr)
{
}

OGRSpatialReference *SpatialDataset::spatialReference() const
{
    return m_spatialReference;
}


//------------------------------------------------------------------------------
// CoordinateTransformation
//------------------------------------------------------------------------------

CoordinateTransformation::CoordinateTransformation(
        OGRSpatialReference* srcSRS, OGRSpatialReference* dstSRS)
{
    if (nullptr != srcSRS && nullptr != dstSRS && !srcSRS->IsSame(dstSRS)) {
        m_oCT = static_cast<OGRCoordinateTransformation*>(
                              OGRCreateCoordinateTransformation(srcSRS, dstSRS));
    }
    else {
        m_oCT = nullptr;
    }
}

CoordinateTransformation::~CoordinateTransformation()
{
    if(nullptr != m_oCT)
        OCTDestroyCoordinateTransformation(
                    reinterpret_cast<OGRCoordinateTransformationH>(m_oCT));
}

bool CoordinateTransformation::transform(OGRGeometry *geom)
{
    if(nullptr == m_oCT)
        return false;
    return geom->transform(m_oCT) == OGRERR_NONE;
}

}
