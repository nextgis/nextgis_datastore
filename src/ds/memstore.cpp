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
#include "catalog/folder.h"

#include "ngstore/catalog/filter.h"
#include "ngstore/codes.h"
#include "ngstore/util/constants.h"
#include "ngstore/version.h"

#include "util/notify.h"
#include "util/error.h"
#include "util/stringutil.h"

namespace ngs {

constexpr const char* MEMSTORE_EXT = "ngmem"; // NextGIS Memory store
constexpr int MEMSTORE_EXT_LEN = length(MEMSTORE_EXT);
constexpr const char* KEY_TYPE = "type";
constexpr const char* TYPE_VAL = "memory store";
constexpr const char* KEY_LAYERS = "layers";
constexpr const char* KEY_LCO_PREFIX = "LCO.";
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

bool MemoryStore::isNameValid(const char* name) const
{
    if(nullptr == name || EQUAL(name, ""))
        return false;
    if(EQUALN(name, MEMSTORE_EXT, MEMSTORE_EXT_LEN))
        return false;

    return Dataset::isNameValid(name);
}

CPLString MemoryStore::normalizeFieldName(const CPLString& name) const
{
    if(EQUAL("fid", name) || EQUAL("geom", name)) {
        CPLString out = name + "_";
        return out;
    }
    return Dataset::normalizeFieldName(name);
}

void MemoryStore::fillFeatureClasses()
{
}

void MemoryStore::addLayer(const CPLJSONObject& layer)
{
    CPLString name = layer.GetString("name", "New layer");
    enum ngsCatalogObjectType type = static_cast<enum ngsCatalogObjectType>(
                layer.GetInteger("type", CAT_UNKNOWN));
    // Get field definitions
    OGRFeatureDefn fieldDefinition(name);

    CPLJSONArray fields = layer.GetArray("fields");

    for(int i = 0; i < fields.Size(); ++i) {
        CPLJSONObject field = fields[i];

        OGRFieldType fieldType = static_cast<OGRFieldType>(
                    field.GetInteger("type", 0));
        CPLString fieldName = field.GetString("name", "");
        CPLString defaultValue = field.GetString("default", "");

        OGRFieldDefn layerField(fieldName, fieldType);
        if(!defaultValue.empty()) {
            layerField.SetDefault(defaultValue);
        }
        fieldDefinition.AddFieldDefn(&layerField);
    }

    CPLJSONObject options = layer.GetObject("options");
    Options createOptions;
    CPLJSONObject** children = options.GetChildren();
    int counter = 0;
    CPLJSONObject* option = nullptr;
    while((option = children[counter++]) != nullptr) {
        createOptions.addOption(option->GetName(), option->GetString(""));
    }
    CPLJSONObject::DestroyJSONObjectList(children);

    ObjectPtr object;
    if(type == CAT_FC_MEM) {
        OGRwkbGeometryType geomType = static_cast<OGRwkbGeometryType>(
                    layer.GetInteger("geometry_type", wkbUnknown));
        if(wkbUnknown == geomType) {
            errorMessage(_("Unsupported geometry type"));
            return;
        }

        int epsg = layer.GetInteger("epsg", 4326);
        OGRSpatialReference sr;
        sr.importFromEPSG(epsg);

        object = ObjectPtr(createFeatureClass(name, CAT_FC_MEM, &fieldDefinition,
                                              &sr, geomType, createOptions));
    }
    else if(type == CAT_TABLE_MEM) {
        object = ObjectPtr(createTable(name, CAT_TABLE_MEM, &fieldDefinition,
                                       createOptions));
    }

    if(object) {
        m_children.push_back(object);
    }
}

bool MemoryStore::create(const char* path, const Options& options)
{
    CPLJSONDocument memDescriptionFile;
    CPLJSONObject root = memDescriptionFile.GetRoot();
    root.Add(KEY_TYPE, TYPE_VAL);
    root.Add(NGS_VERSION_KEY, NGS_VERSION_NUM);

    CPLJSONArray layers;
    root.Add(KEY_LAYERS, layers);

    CPLJSONObject user;
    for(auto it = options.begin(); it != options.end(); ++it) {
        if(EQUALN(it->first, KEY_USER_PREFIX, KEY_USER_PREFIX_LEN)) {
            user.Add(it->first.c_str() + KEY_USER_PREFIX_LEN, it->second);
        }
    }
    root.Add(KEY_USER, user);

    const char* newPath = CPLResetExtension(path, extension());
    return memDescriptionFile.Save(newPath);
}

const char* MemoryStore::extension()
{
    return MEMSTORE_EXT;
}

bool MemoryStore::open(unsigned int /*openFlags*/, const Options &/*options*/)
{
    if(isOpened()) {
        return true;
    }

    CPLErrorReset();

    CPLJSONDocument memDescriptionFile;
    if(memDescriptionFile.Load(m_path)) {
        CPLJSONObject root = memDescriptionFile.GetRoot();
        if(!EQUAL(root.GetString(KEY_TYPE, ""), TYPE_VAL)) {
            return errorMessage(_("Unsupported memory store type"));
        }
        // Check version if needed
//        if(version < NGS_VERSION_NUM && !upgrade(version)) {
//            return errorMessage(_("Upgrade storage failed"));
//        }

        // Create dataset
        GDALDriver* poDriver = Filter::getGDALDriver(CAT_CONTAINER_MEM);
        if(poDriver == nullptr) {
            return errorMessage(_("Memory driver is not present"));
        }

        m_DS = poDriver->Create(m_path, 0, 0, 0, GDT_Unknown, nullptr);
        if(m_DS == nullptr) {
            return errorMessage(CPLGetLastErrorMsg());
        }
        m_DS->MarkAsShared();

        CPLJSONArray layers = root.GetArray(KEY_LAYERS);
        for(int i = 0; i < layers.Size(); ++i) {
            CPLJSONObject layer = layers[i];
            addLayer(layer);
        }

        m_childrenLoaded = true;
    }
    return false;
}

bool MemoryStore::isReadOnly() const
{
    return access(m_path, W_OK) != 0;
}

bool MemoryStore::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly())
        return false;
    return type == CAT_FC_MEM || type == CAT_TABLE_MEM || type == CAT_RASTER_MEM;
}

bool MemoryStore::create(const enum ngsCatalogObjectType type,
                       const CPLString& name, const Options& options)
{
    CPLString newName = normalizeDatasetName(name);

    CPLJSONObject layer;
    layer.Add("name", newName);
    layer.Add("type", type);

    CPLJSONArray fields;

    // Get field definitions
    int fieldCount = options.intOption("FIELD_COUNT", 0);

    for(int i = 0; i < fieldCount; ++i) {
        CPLString fieldName = options.stringOption(CPLSPrintf("FIELD_%d_NAME", i), "");
        if(fieldName.empty()) {
            return errorMessage(_("Name for field %d is not defined"), i);
        }

        CPLString fieldAlias = options.stringOption(CPLSPrintf("FIELD_%d_ALIAS", i), "");
        if(fieldAlias.empty()) {
            fieldAlias = fieldName;
        }

        CPLJSONObject field;
        field.Add("name", fieldName);
        field.Add("alias", fieldAlias);
        field.Add("type", FeatureClass::fieldTypeFromName(
                      options.stringOption(CPLSPrintf("FIELD_%d_TYPE", i), "")));
        CPLString defaultValue = options.stringOption(
                    CPLSPrintf("FIELD_%d_DEFAULT_VAL", i), "");
        if(!defaultValue.empty()) {
            field.Add("default", defaultValue);
        }

        fields.Add(field);
    }

    layer.Add("fields", fields);

    ObjectPtr object;
    if(type == CAT_FC_MEM) {
        OGRwkbGeometryType geomType = FeatureClass::geometryTypeFromName(
                    options.stringOption("GEOMETRY_TYPE", ""));
        if(wkbUnknown == geomType) {
            return errorMessage(_("Unsupported geometry type"));
        }

        layer.Add("geometry_type", static_cast<int>(geomType));
        layer.Add("epsg", options.intOption("EPSG", 4326));
    }

    CPLJSONObject user, other;
    for(auto it = options.begin(); it != options.end(); ++it) {
        if(EQUALN(it->first, KEY_USER_PREFIX, KEY_USER_PREFIX_LEN)) {
            user.Add(CPLSPrintf("%s",
                                it->first.c_str() + KEY_USER_PREFIX_LEN),
                                it->second);
        }
        else if(EQUALN(it->first, KEY_LCO_PREFIX, KEY_LCO_PREFIX_LEN)) {
            other.Add(CPLSPrintf("%s",
                                 it->first.c_str() + KEY_LCO_PREFIX_LEN),
                                 it->second);
        }
    }

    if(user.IsValid()) {
        layer.Add(KEY_USER, user);
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

    addLayer(layer);

    return memDescriptionFile.Save(m_path);
}

} // namespace ngs

