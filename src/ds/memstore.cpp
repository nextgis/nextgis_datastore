/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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

#include "memstore.h"

// gdal
#include "cpl_conv.h"
#include "cpl_vsi.h"

// stl
#include <iostream>

#include "api_priv.h"
#include "geometry.h"

#include "catalog/catalog.h"
#include "catalog/file.h"
#include "catalog/folder.h"

#include "ngstore/catalog/filter.h"
#include "ngstore/codes.h"
#include "ngstore/util/constants.h"
#include "ngstore/version.h"

#include "util/notify.h"
#include "util/error.h"
#include "util/stringutil.h"

namespace ngs {

constexpr const char *MEMSTORE_EXT = "ngmem"; // NextGIS Memory store
constexpr int MEMSTORE_EXT_LEN = length(MEMSTORE_EXT);
constexpr const char *TYPE_VAL = "memory store";
constexpr const char *KEY_LAYERS = "layers";
constexpr const char *KEY_LCO_PREFIX = "LCO.";
constexpr int KEY_LCO_PREFIX_LEN = length(KEY_LCO_PREFIX);


//------------------------------------------------------------------------------
// MemoryStore
//------------------------------------------------------------------------------

MemoryStore::MemoryStore(ObjectContainer * const parent,
                         const CPLString &name,
                         const CPLString &path) :
    Dataset(parent, CAT_CONTAINER_MEM, name, path)
{
}

MemoryStore::~MemoryStore()
{
}

bool MemoryStore::isNameValid(const std::string &name) const
{
    if(name.empty())
        return false;
    if(comparePart(name, MEMSTORE_EXT, MEMSTORE_EXT_LEN))
        return false;

    return Dataset::isNameValid(name);
}

std::string MemoryStore::normalizeFieldName(const std::string &name) const
{
    if(compare("fid", name) || compare("geom", name)) {
        return name + "_";
    }
    return Dataset::normalizeFieldName(name);
}

void MemoryStore::fillFeatureClasses() const
{
}

ObjectPtr MemoryStore::addLayer(const CPLJSONObject &layer)
{
    std::string name = layer.GetString("name", "New layer");
    enum ngsCatalogObjectType type = static_cast<enum ngsCatalogObjectType>(
                layer.GetInteger("type", CAT_UNKNOWN));
    // Get field definitions
    OGRFeatureDefn fieldDefinition(name.c_str());

    CPLJSONArray fields = layer.GetArray("fields");

    for(int i = 0; i < fields.Size(); ++i) {
        CPLJSONObject field = fields[i];

        OGRFieldType fieldType = static_cast<OGRFieldType>(
                    field.GetInteger("type", 0));
        std::string fieldName = field.GetString("name", "");
        std::string defaultValue = field.GetString("default", "");

        OGRFieldDefn layerField(fieldName.c_str(), fieldType);
        if(!defaultValue.empty()) {
            layerField.SetDefault(defaultValue.c_str());
        }
        fieldDefinition.AddFieldDefn(&layerField);
    }

    CPLJSONObject options = layer.GetObj("options");
    Options createOptions;
    std::vector<CPLJSONObject> children = options.GetChildren();
    for(const CPLJSONObject &child : children) {
        createOptions.add(child.GetName(), child.GetString(""));
    }

    ObjectPtr object;
    if(type == CAT_FC_MEM) {
        OGRwkbGeometryType geomType = static_cast<OGRwkbGeometryType>(
            layer.GetInteger("geometry_type", wkbUnknown));
        if(wkbUnknown == geomType) {
            errorMessage(_("Unsupported geometry type"));
            return ObjectPtr();
        }

        int epsg = layer.GetInteger("epsg", 4326);
        SpatialReferencePtr sr = SpatialReferencePtr::importFromEPSG(epsg);
        object = ObjectPtr(createFeatureClass(name, CAT_FC_MEM, &fieldDefinition,
            sr, geomType, createOptions));
    }
    else if(type == CAT_TABLE_MEM) {
        object = ObjectPtr(createTable(name, CAT_TABLE_MEM, &fieldDefinition,
            createOptions));
    }

    if(object) {
        CPLJSONObject user = layer.GetObj(USER_KEY);
        children = user.GetChildren();
        for(const CPLJSONObject &child : children) {
            object->setProperty(child.GetName(), child.ToString(""), USER_KEY);
        }
        m_children.push_back(object);
    }

    return object;
}

bool MemoryStore::create(const std::string &path, const Options &options)
{
    CPLJSONDocument memDescriptionFile;
    CPLJSONObject root = memDescriptionFile.GetRoot();
    root.Add(KEY_TYPE, TYPE_VAL);
    root.Add(NGS_VERSION_KEY, NGS_VERSION_NUM);

    CPLJSONArray layers;
    root.Add(KEY_LAYERS, layers);

    CPLJSONObject user;
    for(auto it = options.begin(); it != options.end(); ++it) {
        if(comparePart(it->first, USER_PREFIX_KEY, USER_PREFIX_KEY_LEN)) {
            user.Add(it->first.c_str() + USER_PREFIX_KEY_LEN, it->second);
        }
    }
    root.Add(USER_KEY, user);

    std::string newPath = File::resetExtension(path, extension());
    return memDescriptionFile.Save(newPath);
}

std::string MemoryStore::extension()
{
    return MEMSTORE_EXT;
}

bool MemoryStore::open(unsigned int openFlags, const Options &options)
{
    ngsUnused(openFlags);
    ngsUnused(options);

    if(isOpened()) {
        return true;
    }

    CPLErrorReset();

    CPLJSONDocument memDescriptionFile;
    if(memDescriptionFile.Load(m_path)) {
        CPLJSONObject root = memDescriptionFile.GetRoot();
        if(!compare(root.GetString(KEY_TYPE, ""), TYPE_VAL)) {
            return errorMessage(_("Unsupported memory store type"));
        }
        // Check version if needed
//        if(version < NGS_VERSION_NUM && !upgrade(version)) {
//            return errorMessage(_("Upgrade storage failed"));
//        }

        // Create dataset
        GDALDriver *poDriver = Filter::getGDALDriver(CAT_CONTAINER_MEM);
        if(poDriver == nullptr) {
            return errorMessage(_("Memory driver is not present"));
        }

        m_DS = poDriver->Create(m_path.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
        if(m_DS == nullptr) {
            return errorMessage(_("Failed to create memory store. %s"), CPLGetLastErrorMsg());
        }
        m_DS->MarkAsShared();

        CPLJSONArray layers = root.GetArray(KEY_LAYERS);
        for(int i = 0; i < layers.Size(); ++i) {
            CPLJSONObject layer = layers[i];
            addLayer(layer);
        }

        CPLJSONObject user = root.GetObj(USER_KEY);
        std::vector<CPLJSONObject> children = user.GetChildren();
        for(const CPLJSONObject &child : children) {
            m_DS->SetMetadataItem(child.GetName().c_str(),
                                  child.ToString("").c_str(), USER_KEY);
        }

        m_addsDS = m_DS;
        m_addsDS->Reference();

        m_childrenLoaded = true;
    }
    return false;
}

bool MemoryStore::isReadOnly() const
{
    return access(m_path.c_str(), W_OK) != 0;
}

bool MemoryStore::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly()) {
        return false;
    }
    return type == CAT_FC_MEM || type == CAT_TABLE_MEM || type == CAT_RASTER_MEM;
}

ObjectPtr MemoryStore::create(const enum ngsCatalogObjectType type,
                       const std::string &name, const Options& options)
{
    std::string newName = normalizeDatasetName(name);

    CPLJSONObject layer;
    layer.Add("name", newName);
    layer.Add("type", type);

    CPLJSONArray fields;

    // Get field definitions
    int fieldCount = options.asInt("FIELD_COUNT", 0);

    for(int i = 0; i < fieldCount; ++i) {
        std::string fieldName = options.asString(CPLSPrintf("FIELD_%d_NAME", i), "");
        if(fieldName.empty()) {
            errorMessage(_("Name for field %d is not defined"), i);
            return ObjectPtr();
        }

        std::string fieldAlias = options.asString(CPLSPrintf("FIELD_%d_ALIAS", i), "");
        if(fieldAlias.empty()) {
            fieldAlias = fieldName;
        }

        CPLJSONObject field;
        field.Add("name", fieldName);
        field.Add("alias", fieldAlias);
        field.Add("type", FeatureClass::fieldTypeFromName(
                      options.asString(CPLSPrintf("FIELD_%d_TYPE", i), "")));
        std::string defaultValue = options.asString(
                    CPLSPrintf("FIELD_%d_DEFAULT_VAL", i), "");
        if(!defaultValue.empty()) {
            field.Add("default", defaultValue);
        }

        fields.Add(field);
    }

    layer.Add("fields", fields);

    if(type == CAT_FC_MEM) {
        OGRwkbGeometryType geomType = FeatureClass::geometryTypeFromName(
                    options.asString("GEOMETRY_TYPE", ""));
        if(wkbUnknown == geomType) {
            errorMessage(_("Unsupported geometry type"));
            return ObjectPtr();
        }

        layer.Add("geometry_type", static_cast<int>(geomType));
        layer.Add("epsg", options.asInt("EPSG", 4326));
    }

    CPLJSONObject user, other;
    for(auto it = options.begin(); it != options.end(); ++it) {
        if(comparePart(it->first, USER_PREFIX_KEY, USER_PREFIX_KEY_LEN)) {
            user.Add(it->first.substr(USER_PREFIX_KEY_LEN), it->second);
        }
        else if(comparePart(it->first, KEY_LCO_PREFIX, KEY_LCO_PREFIX_LEN)) {
            other.Add(it->first.substr(KEY_LCO_PREFIX_LEN), it->second);
        }
    }

    if(user.IsValid()) {
        layer.Add(USER_KEY, user);
    }
    if(other.IsValid()) {
        layer.Add("options", other);
    }

    // Save to file
    CPLJSONDocument memDescriptionFile;
    if(memDescriptionFile.Load(m_path)) {
        CPLJSONObject root = memDescriptionFile.GetRoot();
        CPLJSONArray layers = root.GetArray("layers");
        layers.Add(layer);
    }
    memDescriptionFile.Save(m_path);

    return addLayer(layer);
}

bool MemoryStore::deleteFeatures(const std::string &name)
{
    if(nullptr == m_DS) {
        return false;
    }

    OGRLayer *layer = m_DS->GetLayerByName(name.c_str());
    if(nullptr == layer) {
        return false;
    }

    std::set<GIntBig> ids;
    FeaturePtr feature;
    layer->ResetReading();
    while((feature = layer->GetNextFeature())) {
        ids.insert(feature->GetFID());
    }

    for(auto id : ids) {
        if(layer->DeleteFeature(id) != OGRERR_NONE) {
            return  false;
        }
    }

    return true;
}

OGRLayer *MemoryStore::createAttachmentsTable(const std::string &name)
{
    ngsUnused(name);
    return nullptr;
}

bool MemoryStore::destroyAttachmentsTable(const std::string &name)
{
    ngsUnused(name);
    return true;
}

OGRLayer *MemoryStore::getAttachmentsTable(const std::string &name)
{
    ngsUnused(name);
    return nullptr;
}

} // namespace ngs

