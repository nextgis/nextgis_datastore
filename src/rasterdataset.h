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
#ifndef RASTERDATASET_H
#define RASTERDATASET_H

#include "dataset.h"

namespace ngs {

/**
 * @brief The Raster dataset class represent image or raster
 */
class RasterDataset : public Dataset
{
public:
    RasterDataset(DataStore * datastore,
                  const string& name, const string& alias = "");
};

class RemoteTMSDataset : public RasterDataset
{
public:
    RemoteTMSDataset(DataStore * datastore,
                     const string& name, const string& alias = "");
};

}

#endif // RASTERDATASET_H
