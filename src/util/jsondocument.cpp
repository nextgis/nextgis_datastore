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
#include "jsondocument.h"
#include "ngstore/api.h"

#include "cpl_vsi.h"
#include "cpl_error.h"

namespace ngs{

#define JSON_PATH_DELIMITER "/"

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

int JSONDocument::save(const char *path)
{
    VSILFILE* fp = VSIFOpenL(path, "wt");
    if( nullptr == fp )
        return ngsErrorCodes::EC_SAVE_FAILED;

    const char* data = json_object_to_json_string_ext(m_rootJsonObject,
                                                      JSON_C_TO_STRING_PRETTY);
    VSIFWriteL(data, 1, strlen(data), fp);

    VSIFCloseL(fp);

    return ngsErrorCodes::EC_SUCCESS;
}

JSONObject JSONDocument::getRoot()
{
    if(nullptr == m_rootJsonObject)
        m_rootJsonObject = json_object_new_object();
    return JSONObject(m_rootJsonObject);
}

int JSONDocument::load(const char *path)
{
    VSILFILE* fp = VSIFOpenL(path, "rt");
    if( fp == nullptr )
        return ngsErrorCodes::EC_OPEN_FAILED;

    GByte* pabyOut = nullptr;
    if( !VSIIngestFile( fp, path, &pabyOut, nullptr, -1) ) {
        return ngsErrorCodes::EC_OPEN_FAILED;
    }

    VSIFCloseL(fp);

    // load from ngs.ngmd file
    json_tokener* jstok = json_tokener_new();
    m_rootJsonObject = json_tokener_parse_ex(jstok, (char*)pabyOut, -1);
    json_tokener_free(jstok);
    if( jstok->err != json_tokener_success) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "JSON parsing error: %s (at offset %d)",
                  json_tokener_error_desc(jstok->err), jstok->char_offset);
        return ngsErrorCodes::EC_OPEN_FAILED;
    }
    return ngsErrorCodes::EC_SUCCESS;
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
    json_object *poVal = json_object_new_string (val);
    json_object_object_add(m_jsonObject, name, poVal);
}

void JSONObject::add(const char *name, double val)
{
    json_object *poVal = json_object_new_double (val);
    json_object_object_add(m_jsonObject, name, poVal);
}

void JSONObject::add(const char *name, int val)
{
    json_object *poVal = json_object_new_int (val);
    json_object_object_add(m_jsonObject, name, poVal);
}

void JSONObject::add(const char *name, long val)
{
    json_object *poVal = json_object_new_int64 (val);
    json_object_object_add(m_jsonObject, name, poVal);
}

void JSONObject::add(const char *name, const JSONArray &val)
{
    json_object_object_add(m_jsonObject, name, val.m_jsonObject);
}

void JSONObject::add(const char *name, bool val)
{
    if(nullptr == name)
        return;
    char objectName[255];
    JSONObject object = getObjectByPath(name, &objectName[0]);
    if(object.isValid()) {
        json_object *poVal = json_object_new_boolean (val);
        json_object_object_add(object.m_jsonObject, objectName, poVal);
   }
}

CPLString JSONObject::getString(const char *name, const char* defaultVal) const
{
    json_object* poVal = nullptr;
    if( json_object_object_get_ex(m_jsonObject, name, &poVal)) {
    if( poVal && json_object_get_type(poVal) == json_type_string )
        return json_object_get_string (poVal);
    }
    return defaultVal;
}

double JSONObject::getDouble(const char *name, double defaultVal) const
{
    json_object* poVal = nullptr;
    if( json_object_object_get_ex(m_jsonObject, name, &poVal)) {
    if( poVal && json_object_get_type(poVal) == json_type_double )
        return json_object_get_double (poVal);
    }
    return defaultVal;
}

int JSONObject::getInteger(const char *name, int defaultVal) const
{
    json_object* poVal = nullptr;
    if( json_object_object_get_ex(m_jsonObject, name, &poVal)) {
    if( poVal && json_object_get_type(poVal) == json_type_int )
        return json_object_get_int (poVal);
    }
    return defaultVal;
}

long JSONObject::getLong(const char *name, long defaultVal) const
{
    json_object* poVal = nullptr;
    if( json_object_object_get_ex(m_jsonObject, name, &poVal)) {
    if( poVal && json_object_get_type(poVal) == json_type_int ) // FIXME: How to differ long and int if json_type_int64 or json_type_long not exist?
        return json_object_get_int64 (poVal);
    }
    return defaultVal;
}

bool JSONObject::getBool(const char *name, bool defaultVal) const
{
    if(nullptr == name)
        return defaultVal;
    JSONObject object = getObjectByPath(name);
    if(object.isValid())
        return object.getBool(defaultVal);
    return defaultVal;
}

JSONObject JSONObject::getObjectByPath(const char* path) const
{
    json_object* poVal = nullptr;
    CPLStringList pathPortions(CSLTokenizeString2( path, JSON_PATH_DELIMITER, 0 ));
    int portionsCount = pathPortions.size();
    if(0 == portionsCount)
        return JSONObject(nullptr);
    JSONObject object = *this;
    for(int i = 0; i < portionsCount; ++i) {
        // TODO: check array index in path - i.e. settings/catalog/root/id:1/name
        // if EQUALN(pathPortions[i+1], "id:", 3) -> getArray
        if(json_object_object_get_ex(object.m_jsonObject, pathPortions[i], &poVal)) {
            object = JSONObject(poVal);
        }
        else {
            if(i == portionsCount - 1)
                return JSONObject(nullptr);
            else
                object = JSONObject(pathPortions[i], object.m_jsonObject);
        }
    }
    return object;
}

JSONObject JSONObject::getObjectByPath(const char *path, char *name)
{
    json_object* poVal = nullptr;
    CPLStringList pathPortions(CSLTokenizeString2( path, JSON_PATH_DELIMITER, 0 ));
    int portionsCount = pathPortions.size();
    if(0 == portionsCount)
        return JSONObject(nullptr);
    JSONObject object = *this;
    for(int i = 0; i < portionsCount -1 ; ++i) {
        // TODO: check array index in path - i.e. settings/catalog/root/id:1/name
        // if EQUALN(pathPortions[i+1], "id:", 3) -> getArray
        if(json_object_object_get_ex(object.m_jsonObject, pathPortions[i], &poVal)) {
            object = JSONObject(poVal);
        }
        else {
            object = JSONObject(pathPortions[i], object.m_jsonObject);
        }
    }

    // Check if such object already  exists
    if(json_object_object_get_ex(object.m_jsonObject,
                                 pathPortions[portionsCount - 1], &poVal))
        return JSONObject(nullptr);
    strcpy (name, pathPortions[portionsCount - 1]);
    return object;
}

bool JSONObject::getBool(bool defaultVal) const
{
    if( m_jsonObject && json_object_get_type(m_jsonObject) == json_type_boolean )
        return json_object_get_boolean (m_jsonObject);
    return defaultVal;
}

JSONArray JSONObject::getArray(const char *name) const
{
    json_object* poVal = nullptr;
    if( json_object_object_get_ex(m_jsonObject, name, &poVal)) {
    if( poVal && json_object_get_type(poVal) == json_type_array )
        return JSONArray (poVal);
    }
    return JSONArray(nullptr);

}

JSONObject JSONObject::getObject(const char *name) const
{
    json_object* poVal = nullptr;
    if(json_object_object_get_ex(m_jsonObject, name, &poVal)) {
        return JSONObject(poVal);
    }
    return JSONObject(nullptr);
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
    return JSONObject (json_object_array_get_idx(m_jsonObject, key));
}

const JSONObject JSONArray::operator[](int key) const
{
    return JSONObject (json_object_array_get_idx(m_jsonObject, key));
}

}
