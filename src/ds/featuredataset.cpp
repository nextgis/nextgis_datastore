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
        const OGRSpatialReference *srcSRS, const OGRSpatialReference *dstSRS)
{
    if (nullptr != srcSRS && nullptr != dstSRS && !srcSRS->IsSame(dstSRS)) {
        m_oCT = static_cast<OGRCoordinateTransformation*>(
                                         OGRCreateCoordinateTransformation(
                    const_cast<OGRSpatialReference *>(srcSRS),
                    const_cast<OGRSpatialReference *>(dstSRS)));
    }
    else {
        m_oCT = nullptr;
    }
}

CoordinateTransformationPtr::~CoordinateTransformationPtr()
{
    if(nullptr != m_oCT)
        OCTDestroyCoordinateTransformation(m_oCT);
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
    m_type = ngsDatasetType(Featureset);
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

int FeatureDataset::copyFeatures(const FeatureDataset *srcDataset,
                                 const FieldMapPtr fieldMap,
                                 OGRwkbGeometryType filterGeomType,
                                 unsigned int skipGeometryFlags,
                                 ngsProgressFunc progressFunc,
                                 void *progressArguments)
{
    if(progressFunc)
        progressFunc(0, CPLSPrintf ("Start copy features from '%s' to '%s'",
                                    srcDataset->name ().c_str (), name().c_str ()),
                     progressArguments);

    const OGRSpatialReference *srcSRS = srcDataset->getSpatialReference();
    const OGRSpatialReference *dstSRS = getSpatialReference();
    CoordinateTransformationPtr CT (srcSRS, dstSRS);
    GIntBig featureCount = srcDataset->featureCount();
    OGRwkbGeometryType dstGeomType = getGeometryType();
    double counter = 0;
    srcDataset->reset ();
    FeaturePtr feature;
    while((feature = srcDataset->nextFeature ()) != nullptr) {
        if(progressFunc)
            progressFunc(counter / featureCount, "copying...", progressArguments);

        OGRGeometry * geom = feature->GetGeometryRef ();
        OGRGeometry *newGeom = nullptr;
        if(nullptr == geom && skipGeometryFlags & ngsFeatureLoadSkipType(EmptyGeometry))
            continue;

        if(nullptr != geom) {
            if((skipGeometryFlags & ngsFeatureLoadSkipType(EmptyGeometry)) &&
                    geom->IsEmpty ())
                continue;
            if((skipGeometryFlags & ngsFeatureLoadSkipType(InvalidGeometry)) &&
                    !geom->IsValid())
                continue;


            OGRwkbGeometryType geomType = geom->getGeometryType ();
            OGRwkbGeometryType nonMultiGeomType = geomType;
            if (OGR_GT_Flatten(geomType) > wkbPolygon &&
                    OGR_GT_Flatten(geomType) < wkbGeometryCollection) {
                nonMultiGeomType = static_cast<OGRwkbGeometryType>(geomType - 3);
            }
            if (filterGeomType != wkbUnknown && filterGeomType != nonMultiGeomType) {
                continue;
            }

            if (dstGeomType != geomType) {
                newGeom = OGRGeometryFactory::forceTo(geom, dstGeomType);
            }
            else {
                newGeom = geom->clone ();
            }

            CT.transform(newGeom);
        }

        FeaturePtr dstFeature = createFeature ();
        if(nullptr != newGeom)
            dstFeature->SetGeometryDirectly (newGeom);
        dstFeature->SetFieldsFrom (feature, fieldMap.get());

        if(insertFeature(dstFeature) != ngsErrorCodes::SUCCESS) {
            CPLError(CE_Warning, CPLE_AppDefined, "Create feature failed. "
                     "Source feature FID:%lld", feature->GetFID ());
        }
        counter++;
    }
    if(progressFunc)
        progressFunc(1.0, CPLSPrintf ("Done. Copied %d features", int(counter)),
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
    if(reportType == GeometryReportType::Full)
        return OGRGeometryTypeToName(type);
    if(reportType == GeometryReportType::Ogc)
        return OGRToOGCGeomType(type);

    switch (OGR_GT_Flatten(type)) {
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
