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
#ifndef NGSRASTERDATASET_H
#define NGSRASTERDATASET_H

#include "dataset.h"
#include "coordinatetransformation.h"

namespace ngs {

constexpr const char *KEY_URL = "url";
constexpr const char *KEY_EPSG = "epsg";
constexpr const char *KEY_Z_MIN = "z_min";
constexpr const char *KEY_Z_MAX = "z_max";
constexpr const char *KEY_Y_ORIGIN_TOP = "y_origin_top";
constexpr const char *KEY_X_MIN = "x_min";
constexpr const char *KEY_X_MAX = "x_max";
constexpr const char *KEY_Y_MIN = "y_min";
constexpr const char *KEY_Y_MAX = "y_max";

/**
 * @brief The Raster dataset class represent image or raster
 */
class Raster : public Object, public DatasetBase, public ISpatialDataset
{
public:
    Raster(std::vector<CPLString> siblingFiles,
           ObjectContainer * const parent = nullptr,
           const enum ngsCatalogObjectType type = ngsCatalogObjectType::CAT_RASTER_ANY,
           const CPLString & name = "",
           const CPLString & path = "");

    // DatasetBase interface
    virtual bool open(unsigned int openFlags, const Options &options = Options()) override;
    virtual const char *getOptions(ngsOptionType optionType) const override;

    // ISpatialDataset interface
public:
    virtual OGRSpatialReference *getSpatialReference() const override;

protected:
    OGRSpatialReference m_spatialReference;

private:
    std::vector<CPLString> m_siblingFiles;
};

typedef std::shared_ptr<Raster> RasterPtr;

}

#endif // NGSRASTERDATASET_H
