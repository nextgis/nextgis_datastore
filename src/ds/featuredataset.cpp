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
        OGRSpatialReference *srcSRS, OGRSpatialReference *dstSRS)
{
    if (nullptr != srcSRS && nullptr != dstSRS && !srcSRS->IsSame(dstSRS)) {
        m_oCT = static_cast<OGRCoordinateTransformation*>(
                              OGRCreateCoordinateTransformation(srcSRS, dstSRS));
    }
    else {
        m_oCT = nullptr;
    }
}

CoordinateTransformationPtr::~CoordinateTransformationPtr()
{
    if(nullptr != m_oCT)
        OCTDestroyCoordinateTransformation(reinterpret_cast<OGRCoordinateTransformationH>(m_oCT));
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

OGRSpatialReference *FeatureDataset::getSpatialReference() const
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
                                 ProgressInfo *progressInfo)
{
    unsigned int flags = 0;
    if(progressInfo) {
        progressInfo->onProgress (0, CPLSPrintf ("Start copy features from '%s' to '%s'",
                                    srcDataset->name ().c_str (), name().c_str ()));
        char** val = CSLFetchNameValueMultiple(progressInfo->options(),
                                               "FEATURES_SKIP");
        for( int i = 0; val != NULL && val[i] != NULL; ++i ) {
            if(EQUAL(val[i], "EMPTY_GEOMETRY"))
                flags |= ngsFeatureLoadSkipType(EmptyGeometry);
            if(EQUAL(val[i], "INVALID_GEOMETRY"))
                flags |= ngsFeatureLoadSkipType(InvalidGeometry);
        }
    }

    OGRSpatialReference *srcSRS = srcDataset->getSpatialReference();
    OGRSpatialReference *dstSRS = getSpatialReference();
    CoordinateTransformationPtr CT (srcSRS, dstSRS);
    GIntBig featureCount = srcDataset->featureCount();
    OGRwkbGeometryType dstGeomType = getGeometryType();
    double counter = 0;
    srcDataset->reset ();
    FeaturePtr feature;
    while((feature = srcDataset->nextFeature ()) != nullptr) {
        if(progressInfo)
            progressInfo->onProgress (counter / featureCount, "copying...");

        OGRGeometry * geom = feature->GetGeometryRef ();
        OGRGeometry *newGeom = nullptr;
        if(nullptr == geom && flags & ngsFeatureLoadSkipType(EmptyGeometry))
            continue;

        if(nullptr != geom) {
            if((flags & ngsFeatureLoadSkipType(EmptyGeometry)) &&
                    geom->IsEmpty ())
                continue;
            if((flags & ngsFeatureLoadSkipType(InvalidGeometry)) &&
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

        int nRes = insertFeature(dstFeature);
        if(nRes != ngsErrorCodes::EC_SUCCESS) {
            CPLString errorMsg;
            errorMsg.Printf ("Create feature failed. "
                             "Source feature FID:%lld", feature->GetFID ());
            reportError (nRes, counter / featureCount, errorMsg, progressInfo);
        }
        counter++;
    }
    if(progressInfo) {
        progressInfo->onProgress (1.0, CPLSPrintf ("Done. Copied %d features",
                                              int(counter)));
        progressInfo->setStatus (ngsErrorCodes::EC_SUCCESS);
    }
    return ngsErrorCodes::EC_SUCCESS;
}

bool FeatureDataset::setIgnoredFields(const char** fields)
{
    return m_layer->SetIgnoredFields (fields) == OGRERR_NONE;
}

std::vector<CPLString> FeatureDataset::getGeometryColumns() const
{
    std::vector<CPLString> out;
    OGRFeatureDefn *defn = m_layer->GetLayerDefn ();
    for(int i = 0; i < defn->GetGeomFieldCount (); ++i) {
        OGRGeomFieldDefn* geom = defn->GetGeomFieldDefn (i);
        out.push_back (geom->GetNameRef ());
    }
    return out;
}

CPLString FeatureDataset::getGeometryColumn() const
{
    return m_layer->GetGeometryColumn ();
}

ResultSetPtr FeatureDataset::executeSQL(const CPLString &statement,
                                        GeometryPtr spatialFilter,
                                        const CPLString &dialect) const
{
    return ResultSetPtr(m_DS->ExecuteSQL(statement, spatialFilter.get (), dialect),
                                     [=](OGRLayer* layer)
    {
        m_DS->ReleaseResultSet(layer);
    });
}

/*
char RenderLayer::getCloseOvr()
{
    float diff = 18;
    float currentDiff;
    char zoom = 18;
    for(auto sampleDist : gSampleDists) {
        currentDiff = sampleDist.second - m_renderZoom;
        if(currentDiff > 0 && currentDiff < diff) {
            diff = currentDiff;
            zoom = sampleDist.second;
        }
    }
    return zoom;
}
*/

ResultSetPtr FeatureDataset::getGeometries(unsigned char /*zoom*/,
                                           GeometryPtr spatialFilter) const
{
    // check geometry column
    CPLString geomFieldName;
    //bool originalGeom = true;
    //if(m_renderZoom > 15) {
        geomFieldName = getGeometryColumn ();
    /*}
    else {
        char zoom = getCloseOvr();
        if(zoom < 18) {
            geomFieldName.Printf ("ngs_geom_%d, %s", zoom,
                                  featureDataset->getGeometryColumn ().c_str ());
            originalGeom = false;
        }
        else {
            geomFieldName = featureDataset->getGeometryColumn ();
        }
    }*/

    CPLString statement;
    statement.Printf ("SELECT %s,%s FROM %s", getFIDColumn().c_str(), geomFieldName.c_str (),
                      name ().c_str ());

    return executeSQL (statement, spatialFilter, "");
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
