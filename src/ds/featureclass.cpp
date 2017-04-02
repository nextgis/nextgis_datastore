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
#include "featureclass.h"

#include "coordinatetransformation.h"

// For gettext translation
#define POINT_STR _("Point")
#define LINESTRING_STR _("Line String")
#define POLYGON_STR _("Polygon")
#define MPOINT_STR _("Multi Point")
#define MLINESTRING_STR _("Multi Line String")
#define MPOLYGON_STR _("Multi Polygon")

namespace ngs {

#define ngsFeatureLoadSkipType(x) static_cast<unsigned int>( FeatureClass::SkipType::x )

FeatureClass::FeatureClass(OGRLayer *layer,
                               ObjectContainer * const parent,
                               const ngsCatalogObjectType type,
                               const CPLString &name,
                               const CPLString &path) :
    Table(layer, parent, type, name, path)
{
}

OGRSpatialReference *FeatureClass::getSpatialReference() const
{
    if(nullptr == m_layer)
        return nullptr;
    return m_layer->GetSpatialRef();
}

OGRwkbGeometryType FeatureClass::getGeometryType() const
{
    if(nullptr == m_layer)
        return wkbUnknown;
    return m_layer->GetGeomType ();
}

const char* FeatureClass::getGeometryColumn() const
{
    if(nullptr == m_layer)
        return "";
    return m_layer->GetGeometryColumn();
}

std::vector<const char*> FeatureClass::getGeometryColumns() const
{
    std::vector<const char*> out;
    OGRFeatureDefn *defn = m_layer->GetLayerDefn();
    for(int i = 0; i < defn->GetGeomFieldCount(); ++i) {
        OGRGeomFieldDefn* geom = defn->GetGeomFieldDefn(i);
        out.push_back (geom->GetNameRef());
    }
    return out;
}


bool FeatureClass::copyFeatures(const FeatureClassPtr srcFClass,
                               const FieldMapPtr fieldMap,
                               OGRwkbGeometryType filterGeomType,
                               const Progress& process, const Options &options)
{
    if(!srcFClass) {
        process.onProgress(ngsErrorCodes::EC_COPY_FAILED, 0.0,
                           _("Source feature class is invalid"));
        return false;
    }

    process.onProgress(ngsErrorCodes::EC_IN_PROCESS, 0.0,
                       _("Start copy features from '%s' to '%s'"),
                       srcFClass->getName().c_str(), m_name.c_str());


    unsigned int flags = 0;
    if(options.getBoolOption("SKIP_EMPTY_GEOMETRY"))
        flags |= ngsFeatureLoadSkipType(EMPTYGEOMETRY);
    if(options.getBoolOption("SKIP_INVALID_GEOMETRY"))
        flags |= ngsFeatureLoadSkipType(INVALIDGEOMETRY);

    OGRSpatialReference *srcSRS = srcFClass->getSpatialReference();
    OGRSpatialReference *dstSRS = getSpatialReference();
    CoordinateTransformation CT(srcSRS, dstSRS);
    GIntBig featureCount = srcFClass->featureCount();
    OGRwkbGeometryType dstGeomType = getGeometryType();
    double counter = 0;
    srcFClass->reset();
    FeaturePtr feature;
    while((feature = srcFClass->nextFeature()) != nullptr) {
        double complete = counter / featureCount;
        process.onProgress(ngsErrorCodes::EC_IN_PROCESS, complete,
                           _("Copy in process ..."));

        OGRGeometry * geom = feature->GetGeometryRef();
        OGRGeometry *newGeom = nullptr;
        if(nullptr == geom) {
            if(flags & ngsFeatureLoadSkipType(EMPTYGEOMETRY))
                continue;
        }
        else {
            if((flags & ngsFeatureLoadSkipType(EMPTYGEOMETRY)) &&
                    geom->IsEmpty())
                continue;
            if((flags & ngsFeatureLoadSkipType(INVALIDGEOMETRY)) &&
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
                newGeom = geom->clone();
            }

            CT.transform(newGeom);
        }

        FeaturePtr dstFeature = createFeature();
        if(nullptr != newGeom)
            dstFeature->SetGeometryDirectly(newGeom);
        dstFeature->SetFieldsFrom(feature, fieldMap.get());

        if(!insertFeature(dstFeature)) {
            process.onProgress(ngsErrorCodes::EC_WARNING, complete,
                               _("Create feature failed. Source feature FID:%lld"),
                               feature->GetFID ());
        }
        counter++;
    }
    process.onProgress(ngsErrorCodes::EC_FINISHED, 1.0, _("Done. Copied %d features"),
                       int(counter));

    return true;
}

bool FeatureClass::setIgnoredFields(const std::vector<const char *> fields)
{
    if(fields.empty()) {
        return m_layer->SetIgnoredFields(nullptr) == OGRERR_NONE;
    }

    char** ignoreFields = nullptr;
    for(const char* fieldName : fields) {
        ignoreFields = CSLAddString(ignoreFields, fieldName);
    }
    bool result = m_layer->SetIgnoredFields(
                const_cast<const char**>(ignoreFields)) == OGRERR_NONE;
    CSLDestroy(ignoreFields);
    return result;
}

const char* FeatureClass::getGeometryTypeName(OGRwkbGeometryType type,
                                                GeometryReportType reportType)
{
    if(reportType == GeometryReportType::FULL)
        return OGRGeometryTypeToName(type);
    if(reportType == GeometryReportType::OGC)
        return OGRToOGCGeomType(type);

    switch(OGR_GT_Flatten(type)) {
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
    case wkbPolyhedralSurface:
        return "psurf";
    case wkbTIN:
        return "tin";
    case wkbTriangle:
        return "triangle";
    default:
        return "any";
    }
}

std::vector<OGRwkbGeometryType> FeatureClass::getGeometryTypes()
{
    std::vector<OGRwkbGeometryType> out;

    OGRwkbGeometryType geomType = getGeometryType();
    if (OGR_GT_Flatten(geomType) == wkbUnknown ||
            OGR_GT_Flatten(geomType) == wkbGeometryCollection) {

        std::vector<const char*> ignoreFields;
        OGRFeatureDefn* defn = getDefinition();
        for(int i = 0; i < defn->GetFieldCount(); ++i) {
            OGRFieldDefn *fld = defn->GetFieldDefn(i);
            ignoreFields.push_back(fld->GetNameRef());
        }
        ignoreFields.push_back("OGR_STYLE");
        setIgnoredFields(ignoreFields);
        reset();
        std::map<OGRwkbGeometryType, int> counts;
        FeaturePtr feature;
        while((feature = nextFeature()) != nullptr) {
            OGRGeometry * const geom = feature->GetGeometryRef();
            if (nullptr != geom) {
                OGRwkbGeometryType geomType = geom->getGeometryType();
                counts[OGR_GT_Flatten(geomType)] += 1;
            }
        }
        ignoreFields.clear();
        setIgnoredFields(ignoreFields);

        if(counts[wkbPoint] > 0) {
            if(counts[wkbMultiPoint] > 0) {
                out.push_back(wkbMultiPoint);
            }
            else {
                out.push_back(wkbPoint);
            }
        }
        else if(counts[wkbLineString] > 0) {
            if(counts[wkbMultiLineString] > 0) {
                out.push_back(wkbMultiLineString);
            }
            else {
                out.push_back(wkbLineString);
            }
        }
        else if(counts[wkbPolygon] > 0) {
            if(counts[wkbMultiPolygon] > 0) {
                out.push_back(wkbMultiPolygon);
            }
            else {
                out.push_back(wkbPolygon);
            }
        }
    }
    else {
        out.push_back (geomType);
    }
    return out;
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

ResultSetPtr FeatureClass::getGeometries(unsigned char zoom,
                                           GeometryPtr spatialFilter) const
{
    // check geometry column
    CPLString geomFieldName;
    //bool originalGeom = true;
    //if(m_renderZoom > 15) {
        geomFieldName = getGeometryColumn ();
    }
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
    }

    CPLString statement;
    statement.Printf ("SELECT %s,%s FROM %s", getFIDColumn().c_str(), geomFieldName.c_str (),
                      name ().c_str ());

    return executeSQL (statement, spatialFilter, "");
}

*/

}


