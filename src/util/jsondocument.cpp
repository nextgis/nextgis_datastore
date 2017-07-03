/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
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
#include "jsondocument.h"

#include "cpl_vsi.h"
#include "cpl_error.h"

#include "ngstore/api.h"
#include "util/error.h"

namespace ngs{

constexpr const char * JSON_PATH_DELIMITER = "/";
constexpr unsigned short JSON_NAME_MAX_SIZE = 255;

//------------------------------------------------------------------------------
// JSONDocument
//------------------------------------------------------------------------------

JSONDocument::JSONDocument() : m_rootJsonObject(nullptr)
{

}

JSONDocument::~JSONDocument()
{
    if(m_rootJsonObject)
        json_object_put(m_rootJsonObject);
}

bool JSONDocument::save(const char *path)
{
    VSILFILE* fp = VSIFOpenL(path, "wt");
    if( nullptr == fp )
        return errorMessage(_("Open file %s to write failed"), path);

    const char* data = json_object_to_json_string_ext(m_rootJsonObject,
                                                      JSON_C_TO_STRING_PRETTY);
    VSIFWriteL(data, 1, strlen(data), fp);

    VSIFCloseL(fp);

    return true;
}

JSONObject JSONDocument::getRoot()
{
    if(nullptr == m_rootJsonObject)
        m_rootJsonObject = json_object_new_object();
    return JSONObject(m_rootJsonObject);
}

bool JSONDocument::load(const char *path)
{
    VSILFILE* fp = VSIFOpenL(path, "rt");
    if( fp == nullptr )
        return errorMessage(_("Open file %s failed"), path);

    GByte* pabyOut = nullptr;
    if( !VSIIngestFile( fp, path, &pabyOut, nullptr, -1) ) {
        return errorMessage(_("Read file %s failed"), path);
    }

    VSIFCloseL(fp);

    // load from ngs.ngmd file
    json_tokener* jstok = json_tokener_new();
    m_rootJsonObject = json_tokener_parse_ex(jstok,
                                    reinterpret_cast<const char*>(pabyOut), -1);
    bool parsed = jstok->err == json_tokener_success;
    if(!parsed) {
        errorMessage(_("JSON parsing error: %s (at offset %d)"),
                            json_tokener_error_desc(jstok->err),
                            jstok->char_offset);
    }
    json_tokener_free(jstok);
    return parsed;
}

bool JSONDocument::load(GByte* data, int len)
{
    if(nullptr == data) {
        return false;
    }
    json_tokener* jstok = json_tokener_new();
    m_rootJsonObject = json_tokener_parse_ex(jstok,
                                    reinterpret_cast<const char*>(data), len);
    bool parsed = jstok->err == json_tokener_success;
    if(!parsed) {
        errorMessage(_("JSON parsing error: %s (at offset %d)"),
                            json_tokener_error_desc(jstok->err),
                            jstok->char_offset);
    }
    json_tokener_free(jstok);
    return parsed;
}

//------------------------------------------------------------------------------
// JSONObject
//------------------------------------------------------------------------------

JSONObject::JSONObject()
{
    m_jsonObject = json_object_new_object();
}

JSONObject::JSONObject(const char *name, const JSONObject &parent)
{
    m_jsonObject = json_object_new_object();
    json_object_object_add(parent.m_jsonObject, name, m_jsonObject);
}

JSONObject::JSONObject(json_object *jsonObject) : m_jsonObject(jsonObject)
{

}

void JSONObject::add(const char *name, const char *val)
{
    if(nullptr == name)
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object *poVal = json_object_new_string(val);
        json_object_object_add(object.m_jsonObject, objectName, poVal);
    }
}

void JSONObject::add(const char *name, double val)
{
    if(nullptr == name)
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object *poVal = json_object_new_double(val);
        json_object_object_add(object.m_jsonObject, objectName, poVal);
    }
}

void JSONObject::add(const char *name, int val)
{
    if(nullptr == name)
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object *poVal = json_object_new_int(val);
        json_object_object_add(object.m_jsonObject, objectName, poVal);
    }
}

void JSONObject::add(const char *name, long val)
{
    if(nullptr == name)
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object *poVal = json_object_new_int64(val);
        json_object_object_add(object.m_jsonObject, objectName, poVal);
    }
}

void JSONObject::add(const char *name, const JSONArray &val)
{
    if(nullptr == name)
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object_object_add(object.m_jsonObject, objectName, val.m_jsonObject);
    }
}

void JSONObject::add(const char *name, const JSONObject &val)
{
    if(nullptr == name)
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object_object_add(object.m_jsonObject, objectName, val.m_jsonObject);
    }
}

void JSONObject::add(const char *name, bool val)
{
    if(nullptr == name)
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object *poVal = json_object_new_boolean(val);
        json_object_object_add(object.m_jsonObject, objectName, poVal);
    }
}

void JSONObject::set(const char *name, const char *val)
{
    destroy(name);
    add(name, val);
}

void JSONObject::set(const char *name, double val)
{
    destroy(name);
    add(name, val);
}

void JSONObject::set(const char *name, int val)
{
    destroy(name);
    add(name, val);
}

void JSONObject::set(const char *name, long val)
{
    destroy(name);
    add(name, val);
}

void JSONObject::set(const char *name, bool val)
{
    destroy(name);
    add(name, val);
}

JSONArray JSONObject::getArray(const char *name) const
{
    if(nullptr == name)
        return JSONArray(nullptr);
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object* poVal = nullptr;
        if( json_object_object_get_ex(object.m_jsonObject, objectName, &poVal)) {
            if( poVal && json_object_get_type(poVal) == json_type_array )
                return JSONArray (poVal);
        }
    }
    return JSONArray(nullptr);
}

JSONObject JSONObject::getObject(const char *name) const
{
    if(nullptr == name)
        return JSONArray(nullptr);
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object* poVal = nullptr;
        if(json_object_object_get_ex(object.m_jsonObject, objectName, &poVal)) {
            return JSONObject(poVal);
        }
    }
    return JSONObject(nullptr);
}

void JSONObject::destroy(const char *name)
{
    if(nullptr == name)
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object_object_del(object.m_jsonObject, objectName);
    }
}

const char* JSONObject::getString(const char *name, const char* defaultVal) const
{
    if(nullptr == name)
        return defaultVal;
    JSONObject object = getObject(name);
    return object.getString(defaultVal);
}

const char* JSONObject::getString(const char* defaultVal) const
{
    if( m_jsonObject && json_object_get_type(m_jsonObject) == json_type_string )
        return json_object_get_string (m_jsonObject);
    return defaultVal;
}

double JSONObject::getDouble(const char *name, double defaultVal) const
{
    if(nullptr == name)
        return defaultVal;
    JSONObject object = getObject(name);
    return object.getDouble(defaultVal);
}

double JSONObject::getDouble(double defaultVal) const
{
    if( m_jsonObject && json_object_get_type(m_jsonObject) == json_type_double )
        return json_object_get_double (m_jsonObject);
    return defaultVal;
}

int JSONObject::getInteger(const char *name, int defaultVal) const
{
    if(nullptr == name)
        return defaultVal;
    JSONObject object = getObject(name);
    return object.getInteger(defaultVal);
}

int JSONObject::getInteger(int defaultVal) const
{
    if( m_jsonObject && json_object_get_type(m_jsonObject) == json_type_int )
        return json_object_get_int (m_jsonObject);
    return defaultVal;
}

long JSONObject::getLong(const char *name, long defaultVal) const
{
    if(nullptr == name)
        return defaultVal;
    JSONObject object = getObject(name);
    return object.getLong(defaultVal);
}

long JSONObject::getLong(long defaultVal) const
{
    if( m_jsonObject && json_object_get_type(m_jsonObject) == json_type_int ) // FIXME: How to differ long and int if json_type_int64 or json_type_long not exist?
        return json_object_get_int64 (m_jsonObject);
    return defaultVal;
}

bool JSONObject::getBool(const char *name, bool defaultVal) const
{
    if(nullptr == name)
        return defaultVal;
    JSONObject object = getObject(name);
    return object.getBool(defaultVal);
}

bool JSONObject::getBool(bool defaultVal) const
{
    if( m_jsonObject && json_object_get_type(m_jsonObject) == json_type_boolean )
        return json_object_get_boolean (m_jsonObject);
    return defaultVal;
}

JSONObject JSONObject::getObjectByPath(const char *path, char *name) const
{
    json_object* poVal = nullptr;
    CPLStringList pathPortions(CSLTokenizeString2( path, JSON_PATH_DELIMITER, 0 ));
    int portionsCount = pathPortions.size();
    if(0 == portionsCount)
        return JSONObject(nullptr);
    JSONObject object = *this;
    for(int i = 0; i < portionsCount - 1; ++i) {
        // TODO: check array index in path - i.e. settings/catalog/root/id:1/name
        // if EQUALN(pathPortions[i+1], "id:", 3) -> getArray
        if(json_object_object_get_ex(object.m_jsonObject, pathPortions[i], &poVal)) {
            object = JSONObject(poVal);
        }
        else {
            object = JSONObject(pathPortions[i], object.m_jsonObject);
        }
    }

//    // Check if such object already  exists
//    if(json_object_object_get_ex(object.m_jsonObject,
//                                 pathPortions[portionsCount - 1], &poVal))
//        return JSONObject(nullptr);

    CPLStrlcpy(name, pathPortions[portionsCount - 1], JSON_NAME_MAX_SIZE);
    return object;
}

JSONObject::Type JSONObject::getType() const
{
    if(nullptr == m_jsonObject)
        return Type::Null;
    switch (json_object_get_type(m_jsonObject)) {
    case  json_type_null:
        return Type::Null;
    case json_type_boolean:
        return Type::Boolean;
    case json_type_double:
        return Type::Double;
    case json_type_int:
        return Type::Integer;
    case json_type_object:
        return Type::Object;
    case json_type_array:
        return Type::Array;
    case json_type_string:
        return Type::String;
    }
    return Type::Null;
}

bool JSONObject::isValid() const
{
    return nullptr != m_jsonObject;
}

//------------------------------------------------------------------------------
// JSONArray
//------------------------------------------------------------------------------

JSONArray::JSONArray() : JSONObject( json_object_new_array() )
{

}

JSONArray::JSONArray(json_object *jsonObject): JSONObject(jsonObject)
{

}

int JSONArray::size() const
{
    if(nullptr == m_jsonObject)
        return 0;
    return json_object_array_length(m_jsonObject);
}

void JSONArray::add(const JSONObject &val)
{
    if(val.m_jsonObject)
        json_object_array_add(m_jsonObject, val.m_jsonObject);
}

JSONObject JSONArray::operator[](int key)
{
    return JSONObject(json_object_array_get_idx(m_jsonObject, key));
}

const JSONObject JSONArray::operator[](int key) const
{
    return JSONObject(json_object_array_get_idx(m_jsonObject, key));
}

}
