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
#include "api.h"

#include "cpl_vsi.h"
#include "cpl_error.h"

using namespace ngs;

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
        return ngsErrorCodes::SAVE_FAILED;

    const char* data = json_object_to_json_string_ext(m_rootJsonObject,
                                                      JSON_C_TO_STRING_PRETTY);
    VSIFWriteL(data, 1, strlen(data), fp);

    VSIFCloseL(fp);

    return ngsErrorCodes::SUCCESS;
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
        return ngsErrorCodes::OPEN_FAILED;

    GByte* pabyOut = nullptr;
    if( !VSIIngestFile( fp, path, &pabyOut, nullptr, -1) ) {
        return ngsErrorCodes::OPEN_FAILED;
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
        return ngsErrorCodes::OPEN_FAILED;
    }
    return ngsErrorCodes::SUCCESS;
}

//------------------------------------------------------------------------------
// JSONObject
//------------------------------------------------------------------------------

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

CPLString JSONObject::getString(const char *name, const CPLString& defaultVal)
{
    json_object* poVal = nullptr;
    if( json_object_object_get_ex(m_jsonObject, name, &poVal)) {
    if( poVal && json_object_get_type(poVal) == json_type_string )
        return json_object_get_string (poVal);
    }
    return defaultVal;
}

double JSONObject::getDouble(const char *name, double defaultVal)
{
    json_object* poVal = nullptr;
    if( json_object_object_get_ex(m_jsonObject, name, &poVal)) {
    if( poVal && json_object_get_type(poVal) == json_type_double )
        return json_object_get_double (poVal);
    }
    return defaultVal;
}

int JSONObject::getInteger(const char *name, int defaultVal)
{
    json_object* poVal = nullptr;
    if( json_object_object_get_ex(m_jsonObject, name, &poVal)) {
    if( poVal && json_object_get_type(poVal) == json_type_int )
        return json_object_get_int (poVal);
    }
    return defaultVal;
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

