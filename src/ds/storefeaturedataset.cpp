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
#include "storefeaturedataset.h"

using namespace ngs;

StoreFeatureDataset::StoreFeatureDataset(OGRLayer * const layer) :
    FeatureDataset(layer)
{

}

/* TODO: createDataset() with history also add StoreFeatureDataset to override copyRows function
// 4. for each feature
// 4.1. read
// 4.2. create samples for several scales
// 4.3. create feature in storage dataset
// 4.4. create mapping of fields and original spatial reference metadata
*/
