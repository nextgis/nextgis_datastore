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
#include "ngstore/catalog/filter.h"

#include "catalog/mapfile.h"
#include "ds/datastore.h"

namespace ngs {

//-----------------------------------------------------------------------------
// Filter
//-----------------------------------------------------------------------------

// FIXME: declaration of ‘type’ shadows a member of ‘ngs::Filter’ [-Wshadow]
// NOTE: shadowed declaration is here: lib/include/ngstore/catalog/filter.h:53:31
Filter::Filter(const enum ngsCatalogObjectType type) : type(type)
{
}

bool Filter::canDisplay(ObjectPtr object) const
{
    if(!object)
        return  false;

    if(type == CAT_UNKNOWN)
        return true;

    // Always display containers except filtering of container type
    if(isContainer(object->type()) && !isContainer(type))
        return true;

    if(object->type() == type)
        return true;

    if(isFeatureClass(object->type()) && (type == CAT_FC_ANY ||
                                             type == CAT_RASTER_FC_ANY))
        return true;

    if(isRaster(object->type()) && (type == CAT_RASTER_ANY ||
                                       type == CAT_RASTER_FC_ANY))
        return true;

    if(isDatabase(object->type()) && (type == CAT_CONTAINER_GDB ||
                                         type == CAT_CONTAINER_POSTGRES))
        return true;

    if(isTable(object->type()) && type == CAT_TABLE_ANY)
        return true;

    return false;
}

bool Filter::isFeatureClass(const enum ngsCatalogObjectType type)
{
    return type >= CAT_FC_ANY && type < CAT_FC_ALL;
}

bool Filter::isSimpleDataset(const enum ngsCatalogObjectType type)
{
    return type ==  CAT_FC_ESRI_SHAPEFILE ||
            type == CAT_FC_MAPINFO_TAB ||
            type == CAT_FC_MAPINFO_MIF ||
            type == CAT_FC_GML ||
            type == CAT_FC_GEOJSON ||
            type == CAT_FC_CSV;
}

bool Filter::isContainer(const enum ngsCatalogObjectType type)
{
    return type >= CAT_CONTAINER_ANY && type < CAT_CONTAINER_ALL;
}

bool Filter::isRaster(const enum ngsCatalogObjectType type)
{
    return type >= CAT_RASTER_ANY && type < CAT_RASTER_ALL;
}

bool Filter::isTable(const enum ngsCatalogObjectType type)
{
    return type >= CAT_TABLE_ANY && type < CAT_TABLE_ALL;
}

bool Filter::isDatabase(const enum ngsCatalogObjectType type)
{
    return type == CAT_CONTAINER_GDB || type == CAT_CONTAINER_POSTGRES ||
           type == CAT_CONTAINER_NGS || type == CAT_CONTAINER_GPKG;
}

bool Filter::isFileBased(const enum ngsCatalogObjectType type)
{
    return type == CAT_CONTAINER_WFS ||
          (type >= CAT_CONTAINER_KML && type < CAT_CONTAINER_GPKG) ||
           type == CAT_CONTAINER_SIMPLE ||
          (type >= CAT_RASTER_BMP && type < CAT_RASTER_POSTGIS) ||
          (type >= CAT_FC_ESRI_SHAPEFILE && type < CAT_FC_POSTGIS) ||
          (type >= CAT_FC_GML && type < CAT_FC_MEM) ||
          (type >= CAT_FC_KMLKMZ && type < CAT_FC_GDB) ||
           type == CAT_FC_CSV;
}

GDALDriver *Filter::getGDALDriver(const enum ngsCatalogObjectType type)
{

    switch (type) {
    case CAT_CONTAINER_GPKG:
    case CAT_TABLE_GPKG:
    case CAT_FC_GPKG:
    case CAT_RASTER_GPKG:
    case CAT_CONTAINER_NGS:
        return GetGDALDriverManager()->GetDriverByName("GPKG");
    case CAT_CONTAINER_SQLITE:
    case CAT_TABLE_LITE:
    case CAT_FC_LITE:
    case CAT_RASTER_LITE:
        return GetGDALDriverManager()->GetDriverByName("SQLite");
    case CAT_CONTAINER_GDB:
    case CAT_TABLE_GDB:
    case CAT_FC_GDB:
    case CAT_RASTER_GDB:
        return GetGDALDriverManager()->GetDriverByName("OpenFileGDB");
    case CAT_CONTAINER_POSTGRES:
    case CAT_FC_POSTGIS:
    case CAT_RASTER_POSTGIS:
    case CAT_TABLE_POSTGRES:
        return GetGDALDriverManager()->GetDriverByName("PostgreSQL");
    case CAT_CONTAINER_WFS:
    case CAT_FC_WFS:
        return GetGDALDriverManager()->GetDriverByName("WFS");
    case CAT_CONTAINER_WMS:
    case CAT_RASTER_WMS:
    case CAT_RASTER_TMS:
        return GetGDALDriverManager()->GetDriverByName("WMS");
    case CAT_CONTAINER_KML:
        return GetGDALDriverManager()->GetDriverByName("KML");
    case CAT_CONTAINER_KMZ:
        return GetGDALDriverManager()->GetDriverByName("LIBKML");
    case CAT_CONTAINER_SXF:
        return GetGDALDriverManager()->GetDriverByName("SXF");
    case CAT_FC_ESRI_SHAPEFILE:
        return GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
    case CAT_FC_MAPINFO_TAB:
    case CAT_FC_MAPINFO_MIF:
    case CAT_TABLE_MAPINFO_TAB:
    case CAT_TABLE_MAPINFO_MIF:
        return GetGDALDriverManager()->GetDriverByName("MapInfo File");
    case CAT_FC_DXF:
        return GetGDALDriverManager()->GetDriverByName("DXF");
    case CAT_FC_GML:
        return GetGDALDriverManager()->GetDriverByName("GML");
    case CAT_FC_GEOJSON:
        return GetGDALDriverManager()->GetDriverByName("GeoJSON");
    case CAT_FC_S57:
        return GetGDALDriverManager()->GetDriverByName("S57");
    case CAT_FC_CSV:
        return GetGDALDriverManager()->GetDriverByName("CSV");
    case CAT_RASTER_BMP:
        return GetGDALDriverManager()->GetDriverByName("BMP");
    case CAT_RASTER_TIFF:
        return GetGDALDriverManager()->GetDriverByName("GTiff");
    case CAT_RASTER_TIL:
        return GetGDALDriverManager()->GetDriverByName("TIL");
    case CAT_RASTER_IMG:
        return GetGDALDriverManager()->GetDriverByName("HFA");
    case CAT_RASTER_JPEG:
        return GetGDALDriverManager()->GetDriverByName("JPEG");
    case CAT_RASTER_PNG:
        return GetGDALDriverManager()->GetDriverByName("PNG");
    case CAT_RASTER_GIF:
        return GetGDALDriverManager()->GetDriverByName("GIF");
    case CAT_RASTER_SAGA:
        return GetGDALDriverManager()->GetDriverByName("SAGA");
    case CAT_RASTER_VRT:
        return GetGDALDriverManager()->GetDriverByName("VRT");
    case CAT_TABLE_CSV:
        return GetGDALDriverManager()->GetDriverByName("CSV");
    case CAT_TABLE_ODS:
        return GetGDALDriverManager()->GetDriverByName("ODS");
    case CAT_TABLE_XLS:
        return GetGDALDriverManager()->GetDriverByName("XLS");
    case CAT_TABLE_XLSX:
        return GetGDALDriverManager()->GetDriverByName("XLSX");
    default:
        return nullptr;
    }
}

const char *Filter::getExtension(const enum ngsCatalogObjectType type)
{
    GDALDriver *driver = getGDALDriver(type);
    switch (type) {
    case CAT_CONTAINER_POSTGRES:
        return "dbconn";
    case CAT_CONTAINER_WFS:
    case CAT_CONTAINER_WMS:
    case CAT_CONTAINER_NGW:
        return "wconn";
    case CAT_CONTAINER_NGS:
        return DataStore::extension();
    case CAT_FILE_NGMAPDOCUMENT:
        return MapFile::getExtension();
    case CAT_CONTAINER_KMZ:
        return "kmz";
    case CAT_FC_MAPINFO_TAB:
    case CAT_TABLE_MAPINFO_TAB:
        return "tab";
    case CAT_FC_MAPINFO_MIF:
    case CAT_TABLE_MAPINFO_MIF:
        return "mif";
    case CAT_FC_GEOJSON:
        return "geojson";
    case CAT_CONTAINER_GDB:
    case CAT_CONTAINER_KML:
    case CAT_CONTAINER_SXF:
    case CAT_FC_ESRI_SHAPEFILE:
    case CAT_FC_DXF:
    case CAT_FC_GML:
    case CAT_FC_CSV:
    case CAT_RASTER_BMP:
    case CAT_RASTER_TIFF:
    case CAT_RASTER_TIL:
    case CAT_RASTER_IMG:
    case CAT_RASTER_JPEG:
    case CAT_RASTER_PNG:
    case CAT_RASTER_GIF:
    case CAT_RASTER_SAGA:
    case CAT_RASTER_VRT:
    case CAT_TABLE_CSV:
    case CAT_TABLE_ODS:
    case CAT_TABLE_XLS:
    case CAT_TABLE_XLSX:
    case CAT_CONTAINER_GPKG:
        if(nullptr != driver)
            return driver->GetMetadataItem(GDAL_DMD_EXTENSION);
        else
            return "";
    default:
        return "";
    }
}

//-----------------------------------------------------------------------------
// MultiFilter
//-----------------------------------------------------------------------------

MultiFilter::MultiFilter() : Filter(CAT_UNKNOWN)
{

}

bool MultiFilter::canDisplay(ObjectPtr object) const
{
    if(!object)
        return false;

    for(const auto thisType : types) {
        if(object->type() == thisType)
            return true;

        if(isContainer(object->type()) && thisType == CAT_CONTAINER_ANY)
            return true;

        if(isFeatureClass(object->type()) && thisType == CAT_FC_ANY)
            return true;

        if(isRaster(object->type()) && thisType == CAT_RASTER_ANY)
            return true;

        if(isTable(object->type()) && thisType == CAT_TABLE_ANY)
            return true;
    }
            return false;
}

void MultiFilter::addType(enum ngsCatalogObjectType newType)
{
    types.push_back(newType);
}

}
