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
#ifndef TABLE_H
#define TABLE_H

#include "dataset.h"
#include "ogrsf_frmts.h"

namespace ngs {

class FieldMapPtr : public shared_ptr<int>
{
public:
    FieldMapPtr(unsigned long size);
    int &operator[](int key);
    const int &operator[](int key) const;
};

class FeaturePtr : public shared_ptr< OGRFeature >
{
public:
    FeaturePtr(OGRFeature* feature);
    FeaturePtr();
    FeaturePtr& operator=(OGRFeature* feature);
    operator OGRFeature*() const;
};

class Table : public virtual Dataset
{
public:
    Table(OGRLayer * const layer);
    FeaturePtr createFeature() const;
    FeaturePtr getFeature(GIntBig id) const;
    int insertFeature(const FeaturePtr& feature);
    int updateFeature(const FeaturePtr& feature);
    int deleteFeature(GIntBig id);
    GIntBig featureCount(bool force = true) const;
    void reset() const;
    FeaturePtr nextFeature() const;
    ResultSetPtr executeSQL(const CPLString& statement,
                            const CPLString& dialect = "") const;
    virtual int destroy(unsigned int taskId = 0,
                        ngsProgressFunc progressFunc = nullptr,
                        void* progressArguments = nullptr);
    virtual int copyRows(const Table *pSrcTable, const FieldMapPtr fieldMap,
                         unsigned int taskId = 0,
                         ngsProgressFunc progressFunc = nullptr,
                         void* progressArguments = nullptr);
    OGRFeatureDefn* getDefinition() const;
protected:
    OGRLayer * const m_layer;
};

}
#endif // TABLE_H
