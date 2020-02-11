/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2020 NextGIS, <info@nextgis.com>
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
#include "dataset.h"
#include "ngw.h"
#include "util.h"
#include "util/error.h"
#include "util/settings.h"

#include "ngstore/catalog/filter.h"

namespace ngs {

NGWFeatureClass::NGWFeatureClass(ObjectContainer * const parent,
                                 const enum ngsCatalogObjectType type,
                                 const std::string &name,
                                 const CPLJSONObject &resource,
                                 NGWConnectionBase *connection) :
    FeatureClass(nullptr, parent, type, name),
    NGWResourceBase(resource, connection),
    m_DS(nullptr)
{
    // Fill data from json
    m_geometryType = geometryTypeFromName(
                resource.GetString("vector_layer/geometry_type"));
    m_fastSpatialFilter = true;

    auto fields = resource.GetArray("feature_layer/fields");
    for(int i = 0; i < fields.Size(); ++i) {
        auto field = fields[i];
        m_ignoreFields.emplace_back(field.GetString("keyname"));
    }
    m_ignoreFields.emplace_back("OGR_STYLE");

    // TODO: Create GDALDataset/OGRLayer from json string (resource.Format)
}

NGWFeatureClass::NGWFeatureClass(ObjectContainer * const parent,
                                 const enum ngsCatalogObjectType type,
                                 const std::string &name,
                                 GDALDataset *DS,
                                 OGRLayer *layer,
                                 NGWConnectionBase *connection) :
    FeatureClass(layer, parent, type, name),
    NGWResourceBase(CPLJSONObject(), connection),
    m_DS(DS)
{
    m_resourceId = fromCString(layer->GetMetadataItem("id"));
    m_description = fromCString(layer->GetMetadataItem("description"));
    m_keyName = fromCString(layer->GetMetadataItem("keyname"));
    m_creationDate = fromCString(layer->GetMetadataItem("creation_date"));
}

NGWFeatureClass::~NGWFeatureClass()
{
    close();
}

OGRwkbGeometryType NGWFeatureClass::geometryType() const
{
    return m_geometryType;
}

std::vector<OGRwkbGeometryType> NGWFeatureClass::geometryTypes() const
{
    // NGW layer has only one geometry column
    std::vector<OGRwkbGeometryType> out;
    out.push_back(m_geometryType);
    return out;
}

Envelope NGWFeatureClass::extent() const
{
    if(m_extent.isInit()) {
        return m_extent;
    }

    openDS();

    if(nullptr != m_layer) {
        OGREnvelope env;
        if(m_layer->GetExtent(&env, FALSE) == OGRERR_NONE ||
            m_layer->GetExtent(&env, TRUE) == OGRERR_NONE) {
            m_extent = env;
        }
    }

    return m_extent;
}

SpatialReferencePtr NGWFeatureClass::spatialReference() const
{
    if(m_spatialReference) {
        return m_spatialReference;
    }

    openDS();

    if(nullptr != m_layer) {
        OGRSpatialReference *spaRef = m_layer->GetSpatialRef();
        if(nullptr != spaRef) {
            m_spatialReference = spaRef->Clone();
        }
    }
    return m_spatialReference;
}

bool NGWFeatureClass::destroy()
{
    return NGWResourceBase::remove();
}

bool NGWFeatureClass::canDestroy() const
{
    return true; // Not check user rights here as server will report error if no access.
}

bool NGWFeatureClass::rename(const std::string &newName)
{
    return NGWResourceBase::changeName(newName);
}

bool NGWFeatureClass::canRename() const
{
    return true; // Not check user rights here as server will report error if no access.
}

Properties NGWFeatureClass::properties(const std::string &domain) const
{
    openDS();
    return FeatureClass::properties(domain);
}

std::string NGWFeatureClass::property(const std::string &key,
                                      const std::string &defaultValue,
                                      const std::string &domain) const
{
    openDS();
    return FeatureClass::property(key, defaultValue, domain);
}

bool NGWFeatureClass::setProperty(const std::string &key,
                                  const std::string &value,
                                  const std::string &domain)
{
    openDS();
    return FeatureClass::setProperty(key, value, domain);
}

void NGWFeatureClass::deleteProperties(const std::string &domain)
{
    openDS();
    return FeatureClass::deleteProperties(domain);
}

bool NGWFeatureClass::insertFeature(const FeaturePtr &feature, bool logEdits)
{
    ngsUnused(logEdits);
    openDS();
    return FeatureClass::insertFeature(feature, false);
}

bool NGWFeatureClass::updateFeature(const FeaturePtr &feature, bool logEdits)
{
    ngsUnused(logEdits);
    openDS();
    return FeatureClass::updateFeature(feature, false);
}

bool NGWFeatureClass::deleteFeature(GIntBig id, bool logEdits)
{
    ngsUnused(logEdits);
    openDS();
    return FeatureClass::deleteFeature(id, false);
}

bool NGWFeatureClass::deleteFeatures(bool logEdits)
{
    ngsUnused(logEdits);
    openDS();
    if(nullptr != m_DS && nullptr != m_layer) {
        resetError();
        m_DS->ExecuteSQL(CPLSPrintf("DELETE FROM %s;", m_layer->GetName()),
                         nullptr, nullptr);
        return CPLGetLastErrorType() == 0;
    }
    return false;
}

bool NGWFeatureClass::onRowsCopied(const Progress &progress, const Options &options)
{
    if(nullptr != m_layer) {
        m_layer->ResetReading(); // Produces sync all cached features with NGW.
    }
    return FeatureClass::onRowsCopied(progress, options);
}

void NGWFeatureClass::openDS() const
{
    if(m_layer != nullptr) {
        return;
    }
    std::string userpwd = m_connection->userPwd();
    std::string connectionString = "NGW:" + metadataItem("url", "", "");
    const auto &settings = Settings::instance();
    CPLStringList options;
    if(!userpwd.empty()) {
        options.AddNameValue("USERPWD", userpwd.c_str());
    }
    options.AddNameValue("PAGE_SIZE", settings.getString("NGW_PAGE_SIZE", "50").c_str());
    options.AddNameValue("BATCH_SIZE", settings.getString("NGW_BATCH_SIZE", "100").c_str());
    m_DS = static_cast<GDALDataset*>(
                GDALOpenEx(connectionString.c_str(),
                           DatasetBase::defaultOpenFlags | GDAL_OF_VECTOR,
                           nullptr, options, nullptr));
    if(nullptr != m_DS) {
        m_layer = m_DS->GetLayer(0);
    }
}

void NGWFeatureClass::close()
{
    if(nullptr != m_layer) {
        m_layer->SyncToDisk();
    }
    if(nullptr != m_DS) {
        GDALClose(m_DS);
        m_DS = nullptr;
    }
}

static std::string normalizeFieldName(const std::string &name,
                                      const std::vector<std::string> &nameList,
                                      int counter = 0)
{
    std::string out;
    std::string processedName;
    if(counter == 0) {
        if(compare("id", name)) {
            return normalizeFieldName(name + "_", nameList, counter);
        }
        out = normalize(name, "ru");
        replace_if(out.begin(), out.end(), Dataset::forbiddenChar, '_');

        processedName = out;
    }
    else {
        out = name + "_" + std::to_string(counter);
        processedName = name;
    }

    auto it = std::find(nameList.begin(), nameList.end(), out);
    if(it == nameList.end()) {
        return out;
    }
    return normalizeFieldName(processedName, nameList, counter++);
}


NGWFeatureClass *NGWFeatureClass::createFeatureClass(NGWResourceGroup *resourceGroup,
                                                     const std::string &name,
                                                     OGRFeatureDefn * const definition,
                                                     SpatialReferencePtr spatialRef,
                                                     OGRwkbGeometryType type,
                                                     const Options &options,
                                                     const Progress &progress)
{
    resetError();

    if((type < wkbPoint || type > wkbMultiPolygon) &&
       (type < wkbPoint25D || type > wkbMultiPolygon25D)) {
        errorMessage(_("Unsupported geometry type"));
        return nullptr;
    }

    GDALDriver *driver = Filter::getGDALDriver(CAT_NGW_VECTOR_LAYER);
    if(nullptr == driver) {
        errorMessage(_("Driver not available. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    std::string connectionString = "NGW:" + resourceGroup->property("url", "", "");

    CPLStringList dsOptionsList;
    auto userpwd = options.asString("USERPWD");
    if(!userpwd.empty()) {
        dsOptionsList.AddNameValue("USERPWD", userpwd.c_str());
    }

    const auto &settings = Settings::instance();
    dsOptionsList.AddNameValue("PAGE_SIZE",
                               settings.getString("NGW_PAGE_SIZE", "50").c_str());
    dsOptionsList.AddNameValue("BATCH_SIZE",
                               settings.getString("NGW_BATCH_SIZE", "100").c_str());

    GDALDataset *DS = static_cast<GDALDataset*>(
                GDALOpenEx(connectionString.c_str(),
                           DatasetBase::defaultOpenFlags | GDAL_OF_VECTOR,
                           nullptr, dsOptionsList, nullptr));

    if(nullptr == DS) {
        errorMessage(_("Create of %s file failed. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    CPLStringList lyrOptionsList;
    auto key = options.asString("KEY");
    if(!key.empty()) {
        lyrOptionsList.AddNameValue("KEY", key.c_str());
    }
    auto desc = options.asString(DESCRIPTION_KEY);
    if(!desc.empty()) {
        lyrOptionsList.AddNameValue(DESCRIPTION_KEY, desc.c_str());
    }

    OGRLayer *layer = DS->CreateLayer(name.c_str(), spatialRef, type,
                                      lyrOptionsList);
    if(layer == nullptr) {
        errorMessage(_("Failed to create table %s. %s"), name.c_str(),
                     CPLGetLastErrorMsg());
        return nullptr;
    }

    std::vector<std::string> nameList;
    for (int i = 0; i < definition->GetFieldCount(); ++i) { // Don't check remote id field
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        std::string newFieldName = normalizeFieldName(srcField->GetNameRef(),
                                                      nameList);
        if(!compare(newFieldName, srcField->GetNameRef())) {
            progress.onProgress(COD_WARNING, 0.0,
                _("Field %s of source table was renamed to %s in destination tables"),
                                srcField->GetNameRef(), newFieldName.c_str());
        }

        dstField.SetName(newFieldName.c_str());
        if (layer->CreateField(&dstField) != OGRERR_NONE) {
            errorMessage(_("Failed to create field %s. %s"), newFieldName.c_str(),
                         CPLGetLastErrorMsg());
            return nullptr;
        }

        // Add alias to metadata if exists
        const char *aliasKeyName = CPLSPrintf("FIELD_%d_ALIAS", i);
        auto alias = options.asString(aliasKeyName);
        if(!alias.empty()) {
            layer->SetMetadataItem(aliasKeyName, alias.c_str());
        }

        nameList.push_back(newFieldName);
    }

    // To get resource ID
    layer->SyncToDisk();
    NGWFeatureClass *out = new NGWFeatureClass(resourceGroup, CAT_NGW_VECTOR_LAYER,
                                    name, DS, layer, resourceGroup->connection());
    return out;
}

NGWFeatureClass *NGWFeatureClass::createFeatureClass(NGWResourceGroup *resourceGroup,
                                                     const std::string &name,
                                                     const Options &options,
                                                     const Progress &progress)
{
    // Get field definitions
    CREATE_FEATURE_DEFN_RESULT featureDefnStruct =
            createFeatureDefinition(name, options);
    auto geomTypeStr = options.asString("GEOMETRY_TYPE", "");
    OGRwkbGeometryType geomType = FeatureClass::geometryTypeFromName(geomTypeStr);
    auto spatialRef = resourceGroup->connection()->spatialReference();

    return createFeatureClass(resourceGroup, name, featureDefnStruct.defn.get(),
                              spatialRef, geomType, options, progress);
}


} // namespace ngs
