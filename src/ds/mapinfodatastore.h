/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019 NextGIS, <info@nextgis.com>
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
#ifndef MAPINFODATASTORE_H
#define MAPINFODATASTORE_H

#include "dataset.h"

namespace ngs {


class MapInfoDataStore : public Dataset, public SpatialDataset
{
public:
    explicit MapInfoDataStore(ObjectContainer * const parent = nullptr,
              const std::string &name = "",
              const std::string &path = "");
    virtual ~MapInfoDataStore() override = default;
    // static
public:
    static bool create(const std::string &path);
    static std::string extension();

    // Dataset interface
public:
    virtual bool open(unsigned int openFlags = GDAL_OF_SHARED|GDAL_OF_UPDATE|GDAL_OF_VERBOSE_ERROR,
                      const Options &options = Options()) override;
    virtual FeatureClass *createFeatureClass(const std::string &name,
                                             enum ngsCatalogObjectType objectType,
                                             OGRFeatureDefn * const definition,
                                             SpatialReferencePtr spatialRef,
                                             OGRwkbGeometryType type,
                                             const Options &options = Options(),
                                             const Progress &progress = Progress()) override;
    virtual Table *createTable(const std::string& name,
                               enum ngsCatalogObjectType objectType,
                               OGRFeatureDefn * const definition,
                               const Options &options = Options(),
                               const Progress &progress = Progress()) override;
    // Dataset interface
protected:
    virtual OGRLayer *createAttachmentsTable(const std::string &name) override;
    virtual OGRLayer *createEditHistoryTable(const std::string &name) override;

    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type,
                        const std::string& name,
                        const Options &options) override;

protected:
    virtual bool isNameValid(const std::string &name) const override;
    virtual std::string normalizeFieldName(const std::string &name) const override;
    virtual void fillFeatureClasses() const override;
    OGRLayer *createHashTable(GDALDataset *ds, const std::string &name);

protected:
    bool upgrade(int oldVersion);
};

} // namespace ngs

#endif // MAPINFODATASTORE_H
