/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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

#include "api_priv.h"
#include "coordinatetransformation.h"
#include "dataset.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"

// For gettext translation
#define POINT_STR _("Point")
#define LINESTRING_STR _("Line String")
#define POLYGON_STR _("Polygon")
#define MPOINT_STR _("Multi Point")
#define MLINESTRING_STR _("Multi Line String")
#define MPOLYGON_STR _("Multi Polygon")

namespace ngs {

//------------------------------------------------------------------------------
// FeatureClass
//------------------------------------------------------------------------------

FeatureClass::FeatureClass(OGRLayer *layer, ObjectContainer * const parent,
                           const enum ngsCatalogObjectType type,
                           const std::string &name) :
    Table(layer, parent, type, name)
{
    init();
}

OGRwkbGeometryType FeatureClass::geometryType() const
{
    if(nullptr == m_layer) {
        return wkbUnknown;
    }
    return m_layer->GetGeomType();
}

std::string FeatureClass::geometryColumn() const
{
    if(nullptr == m_layer) {
        return "";
    }
    return m_layer->GetGeometryColumn();
}

std::vector<std::string> FeatureClass::geometryColumns() const
{
    std::vector<std::string> out;
    if(nullptr == m_layer) {
        return out;
    }
    OGRFeatureDefn *defn = m_layer->GetLayerDefn();
    for(int i = 0; i < defn->GetGeomFieldCount(); ++i) {
        OGRGeomFieldDefn *geom = defn->GetGeomFieldDefn(i);
        out.emplace_back(geom->GetNameRef());
    }
    return out;
}


int FeatureClass::copyFeatures(const FeatureClassPtr srcFClass,
                               const FieldMapPtr fieldMap,
                               OGRwkbGeometryType filterGeomType,
                               const Progress &progress, const Options &options)
{
    if(!srcFClass) {
        return outMessage(COD_COPY_FAILED, _("Source feature class is invalid"));
    }

    progress.onProgress(COD_IN_PROCESS, 0.0,
                       _("Start copy features from '%s' to '%s'"),
                       srcFClass->name().c_str(), name().c_str());

    bool skipEmpty = options.asBool("SKIP_EMPTY_GEOMETRY", false);
    bool skipInvalid = options.asBool("SKIP_INVALID_GEOMETRY", false);
    bool toMulti = options.asBool("FORCE_GEOMETRY_TO_MULTI", false);

    DatasetBatchOperationHolder holder(dynamic_cast<Dataset*>(m_parent));

    SpatialReferencePtr srcSRS = srcFClass->spatialReference();
    SpatialReferencePtr dstSRS = spatialReference();
    CoordinateTransformation CT(srcSRS, dstSRS);
    GIntBig featureCount = srcFClass->featureCount();
    OGRwkbGeometryType dstGeomType = geometryType();
    double counter = 0;
    srcFClass->reset();
    FeaturePtr feature;
    bool ogrStyleToField = options.asBool("OGR_STYLE_STRING_TO_FIELD", false);
    bool ogrStyleFieldToStyle = options.asBool("OGR_STYLE_FIELD_TO_STRING", false);
    while((feature = srcFClass->nextFeature())) {
        double complete = counter / featureCount;
        if(!progress.onProgress(COD_IN_PROCESS, complete,
                                _("Copy in process ..."))) {
            return COD_CANCELED;
        }

        OGRGeometry *geom = feature->GetGeometryRef();
        OGRGeometry *newGeom = nullptr;
        if(nullptr == geom) {
            if(skipEmpty) {
                continue;
            }
        }
        else {
            if(skipEmpty && geom->IsEmpty()) {
                continue;
            }
            if(skipInvalid && !geom->IsValid()) {
                continue;
            }

            OGRwkbGeometryType geomType = geom->getGeometryType();
            OGRwkbGeometryType multiGeomType = geomType;
            if(OGR_GT_Flatten(geomType) < wkbPolygon && toMulti) {
                multiGeomType = static_cast<OGRwkbGeometryType>(geomType + 3);
            }
            if(filterGeomType != wkbUnknown && filterGeomType != multiGeomType) {
                continue;
            }

            newGeom = geom->clone();
            if (dstGeomType != geomType) {
                newGeom = OGRGeometryFactory::forceTo(newGeom, dstGeomType);
            }

            CT.transform(newGeom);
        }

        FeaturePtr dstFeature = createFeature();
        if(nullptr != newGeom) {
            dstFeature->SetGeometryDirectly(newGeom);
        }
        if(dstFeature->SetFieldsFrom(feature, fieldMap.get()) != OGRERR_NONE) {
            if(!progress.onProgress(COD_WARNING, complete,
                               _("Set feature fields failed. Source feature FID:" CPL_FRMT_GIB ". Error: %s"),
                               feature->GetFID (), CPLGetLastErrorMsg())) {
                return COD_CANCELED;
            }
        }

        if(ogrStyleToField) {
            dstFeature->SetField(OGR_STYLE_FIELD, feature->GetStyleString());
        }
        if(ogrStyleFieldToStyle) {
            dstFeature->SetStyleString(feature->GetFieldAsString(OGR_STYLE_FIELD));
        }

        if(!insertFeature(dstFeature)) {
            if(!progress.onProgress(COD_WARNING, complete,
                               _("Create feature failed. Source feature FID:" CPL_FRMT_GIB),
                               feature->GetFID ())) {
                return COD_CANCELED;
            }
        }
        counter++;
    }
    progress.onProgress(COD_FINISHED, 1.0, _("Done. Copied %d features"),
                        static_cast<int>(counter));

    return COD_SUCCESS;
}

bool FeatureClass::setIgnoredFields(const std::vector<std::string> &fields)
{
    if(nullptr == m_layer) {
        return false;
    }
    if(fields.empty()) {
        return m_layer->SetIgnoredFields(nullptr) == OGRERR_NONE;
    }

    char** ignoreFields = nullptr;
    for(const std::string &fieldName : fields) {
        ignoreFields = CSLAddString(ignoreFields, fieldName.c_str());
    }
    bool result = m_layer->SetIgnoredFields(
                const_cast<const char**>(ignoreFields)) == OGRERR_NONE;
    CSLDestroy(ignoreFields);
    return result;
}

void FeatureClass::setSpatialFilter(const GeometryPtr &geom)
{
    if(nullptr != m_layer) {
        if(geom) {
            m_layer->SetSpatialFilter(geom.get());
        }
        else {
            m_layer->SetSpatialFilter(nullptr);
        }
    }
}

void FeatureClass::setSpatialFilter(double minX, double minY,
                                    double maxX, double maxY)
{
    if(nullptr != m_layer) {
        m_layer->SetSpatialFilterRect(minX, minY, maxX, maxY);
    }
}

Envelope FeatureClass::extent() const
{
    return m_extent;
}

std::string FeatureClass::geometryTypeName(OGRwkbGeometryType type,
                                           enum GeometryReportType reportType)
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

OGRwkbGeometryType FeatureClass::geometryTypeFromName(const std::string &name)
{
    if(name.empty())
        return wkbUnknown;
    return OGRFromOGCGeomType(name.c_str());
}

std::vector<OGRwkbGeometryType> FeatureClass::geometryTypes() const
{
    std::vector<OGRwkbGeometryType> out;

    OGRwkbGeometryType geomType = geometryType();
    if (OGR_GT_Flatten(geomType) == wkbUnknown ||
            OGR_GT_Flatten(geomType) == wkbGeometryCollection) {

        emptyFields(true);
        reset();
        std::map<OGRwkbGeometryType, int> counts;
        FeaturePtr feature;
        while((feature = nextFeature())) {
            OGRGeometry * const geom = feature->GetGeometryRef();
            if (nullptr != geom) {
                geomType = geom->getGeometryType();
                counts[OGR_GT_Flatten(geomType)] += 1;
            }
        }
        emptyFields(false);

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

void FeatureClass::emptyFields(bool enable) const
{
    if(nullptr == m_layer) {
        return;
    }
    if(!enable) {
        m_layer->SetIgnoredFields(nullptr);
        return;
    }

    char **ignoreFields = nullptr;
    for(const std::string &fieldName : m_ignoreFields) {
        ignoreFields = CSLAddString(ignoreFields, fieldName.c_str());
    }
    m_layer->SetIgnoredFields(const_cast<const char**>(ignoreFields));
    CSLDestroy(ignoreFields);
    return;
}

void FeatureClass::init()
{
    if(nullptr != m_layer) {
        OGRSpatialReference *spaRef = m_layer->GetSpatialRef();
        if(nullptr != spaRef) {
            m_spatialReference = spaRef->Clone();
        }

        OGRFeatureDefn *defn = m_layer->GetLayerDefn();
        for(int i = 0; i < defn->GetFieldCount(); ++i) {
            OGRFieldDefn *fld = defn->GetFieldDefn(i);
            m_ignoreFields.emplace_back(fld->GetNameRef());
        }
        m_ignoreFields.emplace_back(OGR_STYLE_FIELD);

        OGREnvelope env;
        if(m_layer->GetExtent(&env, FALSE) == OGRERR_NONE ||
            m_layer->GetExtent(&env, TRUE) == OGRERR_NONE) {
            m_extent = env;
        }
        else {
            CPLDebug("ngstore", "GetExent failed");
        }

        m_fastSpatialFilter = m_layer->TestCapability(OLCFastSpatialFilter) == TRUE;
    }
}

void FeatureClass::onFeatureInserted(FeaturePtr feature)
{
    Table::onFeatureInserted(feature);
    // Update envelope
    OGRGeometry *geom = feature->GetGeometryRef();
    if(nullptr == geom) {
        return;
    }
    OGREnvelope env;
    geom->getEnvelope(&env);
    Envelope extentBase = env;
    extentBase.fix();
    m_extent.merge(extentBase);
}

void FeatureClass::onFeatureUpdated(FeaturePtr oldFeature, FeaturePtr newFeature)
{
    Table::onFeatureUpdated(oldFeature, newFeature);
    // 1. original feature has no geometry but feature has
    //       just add new geometries to tiles
    // 2. original feature has geometry and feature has
    //       remove old geometries and add new ones
    // 3. original feature has geometry and feature has not
    //       add new geometries

    OGRGeometry *originalGeom = oldFeature->GetGeometryRef();
    OGRGeometry *newGeom = newFeature->GetGeometryRef();
    Envelope extentBase;

    if(nullptr != originalGeom) {
        OGREnvelope env;
        originalGeom->getEnvelope(&env);
        extentBase = env;
    }

    if(nullptr != newGeom) {
        OGREnvelope env;
        newGeom->getEnvelope(&env);
        extentBase.merge(env);
    }

    extentBase.fix();
    m_extent.merge(extentBase);

}

} // namespace ngs
