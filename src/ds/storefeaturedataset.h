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
#ifndef STOREFEATUREDATASET_H
#define STOREFEATUREDATASET_H

#include "featuredataset.h"

#define HISTORY_APPEND "_history"


namespace ngs {

const static array<char, 4> zoomLevels = {{6, 9, 12, 15}};

class StoreFeatureDataset;
typedef shared_ptr<StoreFeatureDataset> StoreFeatureDatasetPtr;

class StoreFeatureDataset : public FeatureDataset
{
    friend class DataStore;
public:
    StoreFeatureDataset(OGRLayer * const layer);
    virtual int copyFeatures(const FeatureDataset *srcDataset,
                             const FieldMapPtr fieldMap,
                             OGRwkbGeometryType filterGeomType,
                             unsigned int skipGeometryFlags,
                             ngsProgressFunc progressFunc,
                             void *progressArguments) override;

protected:
    StoreFeatureDatasetPtr m_history;
};

}
#endif // STOREFEATUREDATASET_H
