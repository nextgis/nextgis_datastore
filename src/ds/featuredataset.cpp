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
#include "featuredataset.h"

#include "ogr_spatialref.h"

using namespace ngs;

//------------------------------------------------------------------------------
// CoordinateTransformationPtr
//------------------------------------------------------------------------------

CoordinateTransformationPtr::CoordinateTransformationPtr(
        OGRCoordinateTransformation *ct) : m_oCT(ct)
{

}

CoordinateTransformationPtr::~CoordinateTransformationPtr()
{
    OCTDestroyCoordinateTransformation(m_oCT);
}

CoordinateTransformationPtr CoordinateTransformationPtr::create(
        const OGRSpatialReference *srcSRS, const OGRSpatialReference *dstSRS)
{
    if (nullptr != srcSRS && nullptr != dstSRS && !srcSRS->IsSame(dstSRS)) {
        return CoordinateTransformationPtr(static_cast<OGRCoordinateTransformation*>(
                                         OGRCreateCoordinateTransformation(
                    const_cast<OGRSpatialReference *>(srcSRS),
                    const_cast<OGRSpatialReference *>(dstSRS))));
    }
    return CoordinateTransformationPtr(nullptr);
}

bool CoordinateTransformationPtr::transform(OGRGeometry* geom) {
    if(nullptr == m_oCT)
        return false;
    return geom->transform (m_oCT) == OGRERR_NONE;
}

//------------------------------------------------------------------------------
// FeatureDataset
//------------------------------------------------------------------------------

FeatureDataset::FeatureDataset(OGRLayer * const layer) : Table(layer),
    SpatialDataset()
{

}

const OGRSpatialReference *FeatureDataset::getSpatialReference() const
{
    if(nullptr != m_layer)
        return m_layer->GetSpatialRef ();
    return nullptr;
}

OGRwkbGeometryType FeatureDataset::getGeometryType() const
{
    if(nullptr != m_layer)
        return m_layer->GetGeomType ();
    return wkbUnknown;
}

int FeatureDataset::copyFeatures(const FeatureDataset *pSrcDataset,
                                 const FieldMapPtr fieldMap,
                                 OGRwkbGeometryType filterGeomType,
                                 unsigned int skipGeometryFlags,
                                 ngsProgressFunc progressFunc,
                                 void *progressArguments)
{
    if(progressFunc)
        progressFunc(0, CPLSPrintf ("Start copy features from '%s' to '%s'",
                                    pSrcDataset->name ().c_str (), name().c_str ()),
                     progressArguments);

    const OGRSpatialReference *srcSRS = pSrcDataset->getSpatialReference();
    const OGRSpatialReference *dstSRS = getSpatialReference();
    CoordinateTransformationPtr CT = CoordinateTransformationPtr::create (srcSRS,
                                                                          dstSRS);
    GIntBig featureCount = pSrcDataset->featureCount();
    OGRwkbGeometryType dstGeomType = getGeometryType();
    double counter = 0;
    pSrcDataset->reset ();
    FeaturePtr feature;
    while((feature = pSrcDataset->nextFeature ()) != nullptr) {
        if(progressFunc)
            progressFunc(counter / featureCount, "copying...", progressArguments);

        const OGRGeometry * geom = feature->GetGeometryRef ();
        if((skipGeometryFlags & EMPTY_GEOMETRY) && !geom->IsEmpty ())
            continue;
        if((skipGeometryFlags & INVALID_GEOMETRY) && !geom->IsValid())
            continue;


        OGRGeometry *newGeom = nullptr;
        OGRwkbGeometryType geomType = geom->getGeometryType ();
        OGRwkbGeometryType nonMultiGeomType = geomType;
        if (wkbFlatten(geomType) > wkbPolygon &&
                wkbFlatten(geomType) < wkbGeometryCollection) {
            nonMultiGeomType = static_cast<OGRwkbGeometryType>(geomType - 3);
        }
        if (filterGeomType != wkbUnknown && filterGeomType != nonMultiGeomType) {
            continue;
        }

        if (dstGeomType != geomType) {
            newGeom = OGRGeometryFactory::forceTo((OGRGeometry*)geom, dstGeomType);
        }
        else {
            newGeom = geom->clone ();
        }

        CT.transform(newGeom);

        FeaturePtr dstFeature = createFeature ();
        dstFeature->SetGeometryDirectly (newGeom);
        dstFeature->SetFieldsFrom (feature, fieldMap.get());

        if(insertFeature(dstFeature) != ngsErrorCodes::SUCCESS) {
            CPLError(CE_Warning, CPLE_AppDefined, "Create feature failed. "
                     "Source feature FID:%lld", feature->GetFID ());
        }
        counter++;
    }
    if(progressFunc)
        progressFunc(1, CPLSPrintf ("Copied %d features", int(counter)),
                     progressArguments);

    return ngsErrorCodes::SUCCESS;
}

bool FeatureDataset::setIgnoredFields(const char** fields)
{
    return m_layer->SetIgnoredFields (fields) == OGRERR_NONE;
}

CPLString FeatureDataset::getGeometryTypeName(OGRwkbGeometryType type,
                                                GeometryReportType reportType)
{
    if(reportType == FULL)
        return OGRGeometryTypeToName(type);
    if(reportType == OGC)
        return OGRToOGCGeomType(type);

    switch (wkbFlatten(type)) {
    case wkbUnknown:
        return "unk";
    case wkbPoint:
        return "pt";
    case wkbLineString:
        return "ln";
    case wkbPolygon:
        return "plg";
    case wkbMultiPoint:
        return "mptr";
    case wkbMultiLineString:
        return "mln";
    case wkbMultiPolygon:
        return "mplg";
    case wkbGeometryCollection:
        return "gt";
    case wkbCircularString:
        return "cir";
    case wkbCompoundCurve:
        return "ccrv";
    case wkbCurvePolygon:
        return "crvplg";
    case wkbMultiCurve:
        return "mcrv";
    case wkbMultiSurface:
        return "msurf";
    case wkbCurve:
        return "crv";
    case wkbSurface:
        return "surf";
    default:
        return "any";
    }
}
