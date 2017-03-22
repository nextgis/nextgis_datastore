/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#ifndef NGSFILTER_H
#define NGSFILTER_H

#include "catalog/object.h"

/**
 * @brief The Catalog Object Types enum
 */
enum ngsCatalogObjectType {
    CAT_UNKNOWN,
    CAT_CONTAINER_ANY,          /** Any container border */
    CAT_CONTAINER_ROOT,
    CAT_CONTAINER_DIR,
    CAT_CONTAINER_GDB,
    CAT_CONTAINER_GDB_SET,
    CAT_CONTAINER_POSTGRES,
    CAT_CONTAINER_POSTGRES_SCHEMA,
    CAT_CONTAINER_WFS,
    CAT_CONTAINER_WMS,
    CAT_CONTAINER_NGW,
    CAT_CONTAINER_KML,
    CAT_CONTAINER_KMZ,
    CAT_CONTAINER_SXF,
    CAT_CONTAINER_ALL,
    CAT_FC_ANY,                 /** Any Feature class */
    CAT_FC_ESRI_SHAPEFILE,
    CAT_FC_MAPINFO_TAB,
    CAT_FC_MAPINFO_MIF,
    CAT_FC_DXF,
    CAT_FC_POSTGIS,
    CAT_FC_GML,
    CAT_FC_GEOJSON,
    CAT_FC_WFS,
    CAT_FC_MEM,
    CAT_FC_KMLKMZ,
    CAT_FC_SXF,
    CAT_FC_S57,
    CAT_FC_GDB,
    CAT_FC_CSV,
    CAT_FC_ALL,
    CAT_RASTER_ANY,             /** Any raster */
    CAT_RASTER_BMP,
    CAT_RASTER_TIFF,
    CAT_RASTER_TIL,
    CAT_RASTER_IMG,
    CAT_RASTER_JPEG,
    CAT_RASTER_PNG,
    CAT_RASTER_GIF,
    CAT_RASTER_SAGA,
    CAT_RASTER_VRT,
    CAT_RASTER_WMS,
    CAT_RASTER_TMS,
    CAT_RASTER_POSTGIS,
    CAT_RASTER_GDB,
    CAT_RASTER_ALL,
    CAT_TABLE_ANY,              /** Any table */
    CAT_TABLE_POSTGRES,
    CAT_TABLE_MAPINFO_TAB,
    CAT_TABLE_MAPINFO_MIF,
    CAT_TABLE_CSV,
    CAT_TABLE_GDB,
    CAT_TABLE_ODS,
    CAT_TABLE_XLS,
    CAT_TABLE_XLSX,
    CAT_TABLE_ALL,
    CAT_QUERY_RESULT
};

namespace ngs {

namespace catalog {

class Filter
{
public:
    Filter(const ngsCatalogObjectType type = CAT_UNKNOWN);
    virtual ~Filter();
    virtual bool canDisplay(ObjectPtr object) const;

public:
    static bool isFeatureClass(const ngsCatalogObjectType type);
    static bool isRaster(const ngsCatalogObjectType type);
    static bool isTable(const ngsCatalogObjectType type);
    static bool isContainer(const ngsCatalogObjectType type);
protected:
    enum ngsCatalogObjectType type;
};

}

}
#endif // NGSFILTER_H
