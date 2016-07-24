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
#ifndef JSONDOCUMENT_H
#define JSONDOCUMENT_H

#include "json.h"
#include "cpl_string.h"

namespace ngs {

/**
 * @brief The JSONObject class JSON object from JSONDocument
 */
class JSONObject
{
public:
    enum class Type {
        Null,
        Object,
        Array,
        Boolean,
        String,
        Integer,
        Double
    };

public:
    JSONObject(json_object *jsonObject);
    void add(const char* name, const char* val);
    void add(const char* name, double val);
    void add(const char* name, int val);
    CPLString getString(const char* name, const CPLString& defaultVal);
    double getDouble(const char* name, double defaultVal);
    int getInteger(const char* name, int defaultVal);
    enum Type getType() const;
protected:
    json_object *m_jsonObject;
};

/**
 * @brief The JSONDocument class Wrapper class around json-c library
 */
class JSONDocument
{
public:
    JSONDocument();
    ~JSONDocument();
    int save(const char* path);
    JSONObject getRoot();
    int load(const char* path);

    // TODO: add JSONObject reader(stream);
    // JsonReader reader = new JsonReader(new InputStreamReader(in, "UTF-8"));
    // reader.beginObject()
    // while (reader.hasNext())
    // String name = reader.nextName()
    // reader.beginArray();
protected:
    json_object *m_rootJsonObject;
};

}

#endif // JSONDOCUMENT_H
