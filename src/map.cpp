/******************************************************************************
*  Project: NextGIS ...
*  Purpose: Application for ...
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*******************************************************************************
*  Copyright (C) 2012-2016 NextGIS, info@nextgis.ru
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#include "map.h"
#include "mapstore.h"
#include "constants.h"
#include "api.h"
#include "table.h"

#include "json.h"

using namespace ngs;

//------------------------------------------------------------------------------
// Layer
//------------------------------------------------------------------------------

Layer::Layer()
{

}

short Layer::id() const
{
    return NOT_FOUND;
}


//------------------------------------------------------------------------------
// Map
//------------------------------------------------------------------------------

Map::Map() : m_name(DEFAULT_MAP_NAME),
    m_epsg(DEFAULT_EPSG), m_minX(DEFAULT_MIN_X), m_minY(DEFAULT_MIN_Y),
    m_maxX(DEFAULT_MAX_X), m_maxY(DEFAULT_MAX_Y), m_deleted(false),
    m_bkChanged(true)
{
    m_bkColor = {210, 245, 255, 255};
}

Map::Map(const CPLString& name, const CPLString& description, unsigned short epsg,
         double minX, double minY, double maxX, double maxY) :
    m_name(name), m_description(description), m_epsg(epsg),
    m_minX(minX), m_minY(minY), m_maxX(maxX), m_maxY(maxY), m_deleted(false),
    m_bkChanged(true)
{
    m_bkColor = {210, 245, 255, 255};
}

Map::~Map()
{

}

CPLString Map::name() const
{
    return m_name;
}

void Map::setName(const CPLString &name)
{
    m_name = name;
}

CPLString Map::description() const
{
    return m_description;
}

void Map::setDescription(const CPLString &description)
{
    m_description = description;
}

unsigned short Map::epsg() const
{
    return m_epsg;
}

void Map::setEpsg(unsigned short epsg)
{
    m_epsg = epsg;
}

double Map::minX() const
{
    return m_minX;
}

void Map::setMinX(double minX)
{
    m_minX = minX;
}

double Map::minY() const
{
    return m_minY;
}

void Map::setMinY(double minY)
{
    m_minY = minY;
}

double Map::maxX() const
{
    return m_maxX;
}

void Map::setMaxX(double maxX)
{
    m_maxX = maxX;
}

double Map::maxY() const
{
    return m_maxY;
}

void Map::setMaxY(double maxY)
{
    m_maxY = maxY;
}

int Map::load(const char *path)
{
    // TODO: need to switch to SAX json reader/writer, need abstract json class
    // for both json-c and new json support.
    m_path = path;

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
    json_object * poObj = json_tokener_parse_ex(jstok, (char*)pabyOut, -1);
    if( jstok->err != json_tokener_success) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "JSON parsing error: %s (at offset %d)",
                  json_tokener_error_desc(jstok->err), jstok->char_offset);
        json_tokener_free(jstok);
        return ngsErrorCodes::OPEN_FAILED;
    }

    json_object* poRootInstance = poObj;
    if( poRootInstance && json_object_get_type(poRootInstance) == json_type_object )
    {
        json_object* poName = json_object_object_get(poRootInstance, MAP_NAME);
        if( poName && json_object_get_type(poName) == json_type_string )
            m_name = json_object_get_string(poName);
        json_object* poDescription = json_object_object_get(poRootInstance, MAP_DESCRIPTION);
        if( poDescription && json_object_get_type(poDescription) == json_type_string )
            m_description = json_object_get_string(poDescription);
        json_object* poEPSG = json_object_object_get(poRootInstance, MAP_EPSG);
        if( poEPSG && json_object_get_type(poEPSG) == json_type_int )
            m_epsg = json_object_get_int(poEPSG);
        json_object* poMinX = json_object_object_get(poRootInstance, MAP_MIN_X);
        if( poMinX && json_object_get_type(poMinX) == json_type_double )
            m_minX = json_object_get_int(poMinX);
        json_object* poMinY = json_object_object_get(poRootInstance, MAP_MIN_Y);
        if( poMinY && json_object_get_type(poMinY) == json_type_double )
            m_minY = json_object_get_int(poMinY);
        json_object* poMaxX = json_object_object_get(poRootInstance, MAP_MAX_X);
        if( poMaxX && json_object_get_type(poMaxX) == json_type_double )
            m_maxX = json_object_get_int(poMaxX);
        json_object* poMaxY = json_object_object_get(poRootInstance, MAP_MAX_Y);
        if( poMaxY && json_object_get_type(poMaxY) == json_type_double )
            m_maxY = json_object_get_int(poMaxY);

    }
    json_tokener_free(jstok);
    return ngsErrorCodes::SUCCESS;
}

int Map::save(const char *path)
{
    if(m_deleted)
        return ngsErrorCodes::SAVE_FAILED;

    VSILFILE* fp = VSIFOpenL(path, "wt");
    if( fp == NULL )
        return ngsErrorCodes::SAVE_FAILED;

    // save to *.ngmd file
    json_object *poJsonObject = json_object_new_object();
    json_object *poName = json_object_new_string(m_name);
    json_object_object_add(poJsonObject, MAP_NAME, poName);
    json_object *poDescription = json_object_new_string(m_description);
    json_object_object_add(poJsonObject, MAP_DESCRIPTION, poDescription);
    json_object *poEPSG = json_object_new_int (m_epsg);
    json_object_object_add(poJsonObject, MAP_EPSG, poEPSG);
    json_object *poMinX = json_object_new_double (m_minX);
    json_object_object_add(poJsonObject, MAP_MIN_X, poMinX);
    json_object *poMinY = json_object_new_double (m_minY);
    json_object_object_add(poJsonObject, MAP_MIN_Y, poMinY);
    json_object *poMaxX = json_object_new_double (m_maxX);
    json_object_object_add(poJsonObject, MAP_MAX_X, poMaxX);
    json_object *poMaxY = json_object_new_double (m_maxY);
    json_object_object_add(poJsonObject, MAP_MAX_Y, poMaxY);


    const char* data = json_object_to_json_string_ext(poJsonObject,
                                                      JSON_C_TO_STRING_PRETTY);
    VSIFWriteL(data, 1, strlen(data), fp);
    json_object_put(poJsonObject);

    VSIFCloseL(fp);

    return ngsErrorCodes::SUCCESS;
}

int Map::destroy()
{
    if(m_deleted) {
        return ngsErrorCodes::DELETE_FAILED;
    }

    if(m_path.empty ()) {
        m_deleted = true;
        return ngsErrorCodes::SUCCESS;
    }

    int nRet = CPLUnlinkTree (m_path) == 0 ? ngsErrorCodes::SUCCESS :
                                             ngsErrorCodes::DELETE_FAILED;
    if(nRet == ngsErrorCodes::SUCCESS)
        m_deleted = true;
    return nRet;
}

bool Map::isDeleted() const
{
    return m_deleted;
}

ngsRGBA Map::getBackgroundColor() const
{
    return m_bkColor;
}

int Map::setBackgroundColor(const ngsRGBA &color)
{
    m_bkColor = color;
    m_bkChanged = true;
    return ngsErrorCodes::SUCCESS;
}

bool Map::isBackgroundChanged() const
{
    return m_bkChanged;
}

void Map::setBackgroundChanged(bool bkChanged)
{
    m_bkChanged = bkChanged;
}
