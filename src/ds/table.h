/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
#ifndef NGSTABLE_H
#define NGSTABLE_H

// gdal
#include "ogrsf_frmts.h"

#include "catalog/object.h"
#include "ngstore/codes.h"

namespace ngs {

class FieldMapPtr : public std::shared_ptr<int>
{
public:
    explicit FieldMapPtr(unsigned long size);
    int &operator[](int key);
    const int &operator[](int key) const;
};

typedef struct _Field {
    CPLString m_name;
    CPLString m_originalName;
    CPLString m_alias;
    OGRFieldType m_type;
} Field;

class FeaturePtr : public std::shared_ptr<OGRFeature>
{
public:
    FeaturePtr(OGRFeature* feature);
    FeaturePtr();
    FeaturePtr& operator=(OGRFeature* feature);
    operator OGRFeature*() const { return get(); }
};

class Table;
typedef std::shared_ptr<Table> TablePtr;

class Table : public Object
{
    friend class Dataset;
public:
    explicit Table(OGRLayer* layer,
          ObjectContainer* const parent = nullptr,
          const enum ngsCatalogObjectType type = CAT_TABLE_ANY,
          const CPLString& name = "");
    virtual ~Table();
    FeaturePtr createFeature() const;
    FeaturePtr getFeature(GIntBig id) const;
    bool insertFeature(const FeaturePtr& feature);
    bool updateFeature(const FeaturePtr& feature);
    bool deleteFeature(GIntBig id);
    GIntBig featureCount(bool force = true) const;
    void reset() const;
    FeaturePtr nextFeature() const;
    virtual int copyRows(const TablePtr srcTable,
                         const FieldMapPtr fieldMap,
                         const Progress& progress = Progress());
    const char* fidColumn() const;    
    std::vector<Field> fields() const { return m_fields; }
    virtual char** getMetadata(const char* domain) const override;
    void fillFields();

    // Object interface
public:
    virtual bool destroy() override;

protected:
    OGRFeatureDefn * definition() const;

protected:
    OGRLayer* m_layer;
    std::vector<Field> m_fields;
};

}
#endif // NGSTABLE_H
