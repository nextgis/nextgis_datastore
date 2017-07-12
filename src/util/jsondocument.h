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
#ifndef NGSJSONDOCUMENT_H
#define NGSJSONDOCUMENT_H

#include "json.h"

// gdal
#include "cpl_string.h"

namespace ngs {

class JSONArray;
/**
 * @brief The JSONObject class JSON object from JSONDocument
 */
class JSONObject
{
    friend class JSONArray;
    friend class JSONDocument;
public:
    enum class Type {
        Null,
        Object,
        Array,
        Boolean,
        String,
        Integer,
        Long,
        Double
    };

public:
    JSONObject();
    explicit JSONObject(const char* name, const JSONObject& parent);

private:
    JSONObject(json_object *jsonObject);

public:
    // setters
    void add(const char* name, const char* val);
    void add(const char* name, double val);
    void add(const char* name, int val);
    void add(const char* name, long val);
    void add(const char* name, const JSONArray& val);
    void add(const char* name, const JSONObject& val);
    void add(const char* name, bool val);

    void set(const char* name, const char* val);
    void set(const char* name, double val);
    void set(const char* name, int val);
    void set(const char* name, long val);
    void set(const char* name, bool val);

    // getters
    const char* getString(const char* name, const char *defaultVal) const;
    double getDouble(const char* name, double defaultVal) const;
    int getInteger(const char* name, int defaultVal) const;
    long getLong(const char* name, long defaultVal) const;
    bool getBool(const char* name, bool defaultVal) const;



    //
    void destroy(const char* name);
    JSONArray getArray(const char* name) const;
    JSONObject getObject(const char* name) const;
    enum Type getType() const;
    bool isValid() const;

protected:
    JSONObject getObjectByPath(const char *path, char *name) const;
    const char* getString(const char *defaultVal) const;
    double getDouble(double defaultVal) const;
    int getInteger(int defaultVal) const;
    long getLong(long defaultVal) const;
    bool getBool(bool defaultVal) const;

private:
    json_object *m_jsonObject;
};

/**
 * @brief The JSONArray class JSON array from JSONDocument
 */
class JSONArray : public JSONObject
{
    friend class JSONObject;
public:
    JSONArray();
private:
    explicit JSONArray(json_object *jsonObject);
public:
    int size() const;
    void add(const JSONObject& val);
    JSONObject operator[](int key);
    const JSONObject operator[](int key) const;
};

/**
 * @brief The JSONDocument class Wrapper class around json-c library
 */
class JSONDocument
{
public:
    JSONDocument();
    ~JSONDocument();
    bool save(const char* path);
    JSONObject getRoot();
    bool load(const char* path);
    bool load(GByte* data, int len = -1);

    // TODO: add JSONObject reader(stream);
    // JsonReader reader = new JsonReader(new InputStreamReader(in, "UTF-8"));
    // reader.beginObject()
    // while (reader.hasNext())
    // String name = reader.nextName()
    // reader.beginArray();
private:
    json_object *m_rootJsonObject;
};

}

#endif // NGSJSONDOCUMENT_H
