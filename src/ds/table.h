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
#ifndef TABLE_H
#define TABLE_H

#include "dataset.h"
#include "ogrsf_frmts.h"

namespace ngs {

class FeaturePtr : public shared_ptr< OGRFeature >
{
public:
    FeaturePtr(OGRFeature* pFeature);
    FeaturePtr();
    FeaturePtr& operator=(OGRFeature* pFeature);
    operator OGRFeature*() const;
};

class Table : public Dataset
{
public:
    Table(OGRLayer* const layer, DataStore * datastore,
          const string& name, const string& alias = "");
    FeaturePtr createFeature() const;
    FeaturePtr getFeature(GIntBig id) const;
    int insertFeature(const FeaturePtr& feature);
    int updateFeature(const FeaturePtr& feature);
    int deleteFeature(GIntBig id);
    GIntBig featureCount(bool force = true) const;
    void reset() const;
    FeaturePtr nextFeature() const;
protected:
    OGRLayer * const m_layer;
};

}
#endif // TABLE_H
