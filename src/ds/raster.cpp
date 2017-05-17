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
#include "raster.h"

namespace ngs {

Raster::Raster(std::vector<CPLString> siblingFiles,
               ObjectContainer * const parent,
               const enum ngsCatalogObjectType type,
               const CPLString &name,
               const CPLString &path) :
  Object(parent, type, name, path),
  DatasetBase(),
  m_siblingFiles(siblingFiles)
{
}

bool Raster::open(unsigned int openFlags, const Options &options)
{
    if(m_type == CAT_RASTER_TMS) {
        JSONDocument connectionFile;
        if(!connectionFile.load(m_path)) {
            return false;
        }
        JSONObject root = connectionFile.getRoot();
        CPLString url = root.getString(KEY_URL);
        url = url.replaceAll("{", "${");
        int epsg = root.getInteger(KEY_EPSG, 3857);
        m_spatialReference.importFromEPSG(epsg);
//        int z_min = root.getInteger(KEY_Z_MIN, 0);
        int z_max = root.getInteger(KEY_Z_MAX, 18);
        bool y_origin_top = root.getBool(KEY_Y_ORIGIN_TOP, true);

        double x_min = root.getDouble(KEY_X_MIN, DEFAULT_BOUNDS.getMinX());
        double x_max = root.getDouble(KEY_X_MAX, DEFAULT_BOUNDS.getMaxX());
        double y_min = root.getDouble(KEY_Y_MIN, DEFAULT_BOUNDS.getMinY());
        double y_max = root.getDouble(KEY_Y_MAX, DEFAULT_BOUNDS.getMaxY());

        const char* connStr = CPLSPrintf("<GDAL_WMS><Service name=\"TMS\">"
            "<ServerUrl>%s</ServerUrl></Service><DataWindow>"
            "<UpperLeftX>%f</UpperLeftX><UpperLeftY>%f</UpperLeftY>"
            "<LowerRightX>%f</LowerRightX><LowerRightY>%f</LowerRightY>"
            "<TileLevel>%d</TileLevel><TileCountX>1</TileCountX>"
            "<TileCountY>1</TileCountY><YOrigin>%s</YOrigin></DataWindow>"
            "<Projection>EPSG:%d</Projection><BlockSizeX>256</BlockSizeX>"
            "<BlockSizeY>256</BlockSizeY><BandsCount>3</BandsCount>"
            "<Cache/></GDAL_WMS>",
                                         url.c_str(), x_min, y_max, x_max, y_min,
                                         z_max, y_origin_top ? "top" : "bottom",
                                         epsg);
        return DatasetBase::open(connStr, openFlags, options);
    }
    else {
        if(DatasetBase::open(m_path, openFlags, options)) {
            const char *spatRefStr = m_DS->GetProjectionRef();
            m_spatialReference.SetFromUserInput(spatRefStr);
        }
    }
    return false;
}

OGRSpatialReference *Raster::getSpatialReference() const
{
    return const_cast<OGRSpatialReference*>(&m_spatialReference);
}

const char *Raster::getOptions(ngsOptionType optionType) const
{
    return DatasetBase::getOptions(m_type, optionType);
}

} // namespace ngs
