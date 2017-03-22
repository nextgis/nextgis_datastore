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

namespace ngs {

//-----------------------------------------------------------------------------
// Filter
//-----------------------------------------------------------------------------

Filter::Filter(const ngsCatalogObjectType type) : type(type)
{
}

bool Filter::canDisplay(ObjectPtr object) const
{
    if(type == CAT_UNKNOWN)
        return true;

    // Always dispaly containers except filtering of container type
    if(isContainer(object->getType() && !isContainer(type))
        return true;

    if(object->getType() == type)
        return true;

    if(isFeatureClass(object->getType()) && type == CAT_FC_ANY)
        return true;

    if(isRaster(object->getType()) && type == CAT_RASTER_ANY)
        return true;

    if(isRaster(object->getType()) && type == CAT_RASTER_ANY)
        return true;

    if(isTable(object->getType()) && type == CAT_TABLE_ANY)
        return true;

    return false;
}

bool Filter::isFeatureClass(const ngsCatalogObjectType type)
{
    return type >= CAT_FC_ANY && type < CAT_FC_ALL;
}

bool Filter::isContainer(const ngsCatalogObjectType type)
{
    return type >= CAT_CONTAINER_ANY && type < CAT_CONTAINER_ALL;
}

bool Filter::isRaster(const ngsCatalogObjectType type)
{
    return type >= CAT_RASTER_ANY && type < CAT_RASTER_ALL;
}

bool Filter::isTable(const ngsCatalogObjectType type)
{
    return type >= CAT_TABLE_ANY && type < CAT_TABLE_ALL;
}

//-----------------------------------------------------------------------------
// MultiFilter
//-----------------------------------------------------------------------------

MultiFilter::MultiFilter() : Filter(CAT_UNKNOWN)
{

}

bool MultiFilter::canDisplay(ObjectPtr object) const
{
    for(const enum ngsCatalogObjectType thisType : types) {
        if(object->getType() == thisType)
            return true;

        if(isContainer(object->getType()) && thisType == CAT_CONTAINER_ANY)
            return true;

        if(isFeatureClass(object->getType()) && thisType == CAT_FC_ANY)
            return true;

        if(isRaster(object->getType()) && thisType == CAT_RASTER_ANY)
            return true;

        if(isTable(object->getType()) && thisType == CAT_TABLE_ANY)
            return true;
    }
            return false;
}

void MultiFilter::addType(ngsCatalogObjectType newType)
{
    types.push_back(newType);
}

}
