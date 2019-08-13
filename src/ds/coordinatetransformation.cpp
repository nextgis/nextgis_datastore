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
// SpatialReferencePtr
//------------------------------------------------------------------------------
void OSRRelease( OGRSpatialReference *srs ) {
    if(nullptr != srs) {
        srs->Release();
    }
}

SpatialReferencePtr::SpatialReferencePtr(OGRSpatialReference *srs) : shared_ptr(srs, OSRRelease)
{
}

SpatialReferencePtr::SpatialReferencePtr() : shared_ptr(nullptr, OSRRelease)
{
}

SpatialReferencePtr &SpatialReferencePtr::operator=(OGRSpatialReference *srs)
{
    reset(srs);
    return *this;
}

bool SpatialReferencePtr::setFromUserInput(const std::string &input)
{
    if(!input.empty()) {
        OGRSpatialReference *srs = new OGRSpatialReference();
        if(srs->SetFromUserInput(input.c_str()) == OGRERR_NONE) {
            reset(srs);
            return true;
        }
    }
    return false;
}

SpatialReferencePtr SpatialReferencePtr::importFromEPSG(int EPSG)
{
    SpatialReferencePtr sr(new OGRSpatialReference());
    if(sr->importFromEPSG(EPSG) == OGRERR_NONE) {
        sr->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        return sr;        
    }
    return nullptr;
}

//------------------------------------------------------------------------------
// SpatialDataset
//------------------------------------------------------------------------------

SpatialDataset::SpatialDataset()
{
}

SpatialReferencePtr SpatialDataset::spatialReference() const
{
    return m_spatialReference;
}

//------------------------------------------------------------------------------
// CoordinateTransformation
//------------------------------------------------------------------------------

CoordinateTransformation::CoordinateTransformation(
        SpatialReferencePtr srcSRS, SpatialReferencePtr dstSRS)
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
