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

#include "gdal_priv.h"

#include <vector>

namespace ngs {

/**
 * @brief The simple catalog filter class
 */
class Filter
{
public:
    explicit Filter(const enum ngsCatalogObjectType type = CAT_UNKNOWN);
    virtual ~Filter() = default;
    virtual bool canDisplay(ObjectPtr object) const;

public:
    static bool isFeatureClass(const enum ngsCatalogObjectType type);
    static bool isSimpleDataset(const enum ngsCatalogObjectType type);
    static bool isRaster(const enum ngsCatalogObjectType type);
    static bool isTable(const enum ngsCatalogObjectType type);
    static bool isContainer(const enum ngsCatalogObjectType type);
    static bool isDatabase(const enum ngsCatalogObjectType type);
    static bool isFileBased(const enum ngsCatalogObjectType type);
    static bool isLocalDir(const enum ngsCatalogObjectType type);
    static bool isConnection(const enum ngsCatalogObjectType type);
    static GDALDriver *getGDALDriver(const enum ngsCatalogObjectType type);
    static std::string extension(const enum ngsCatalogObjectType type);
protected:
    static bool canDisplay(enum ngsCatalogObjectType type, ObjectPtr object);
protected:
    enum ngsCatalogObjectType m_type;
};

class MultiFilter : public Filter
{
public:
    MultiFilter();
    virtual ~MultiFilter() override = default;
    virtual bool canDisplay(ObjectPtr object) const override;
    void addType(enum ngsCatalogObjectType newType);

protected:
    std::vector<enum ngsCatalogObjectType> m_types;
};

}

#endif // NGSFILTER_H
