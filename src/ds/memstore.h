/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#ifndef NGSMEMSTORE_H
#define NGSMEMSTORE_H

#include "dataset.h"

namespace ngs {

/**
 * @brief The memory geodata storage and manipulation class for raster and vector
 * geodata and plain tables
 */
class MemoryStore : public Dataset
{
public:
    explicit MemoryStore(ObjectContainer * const parent = nullptr,
              const CPLString & name = "",
              const CPLString & path = "");
    virtual ~MemoryStore();

    // static
public:
    static bool create(const char* path, const Options& options);
    static const char* extension();

    // Dataset interface
public:
    virtual bool open(unsigned int openFlags,
                      const Options &options = Options()) override;
    virtual bool deleteFeatures(const char* name) override;

    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual bool create(const enum ngsCatalogObjectType type, const CPLString& name,
                        const Options& options) override;
    virtual bool isReadOnly() const override;

    // Dataset
protected:
    virtual OGRLayer* createAttachmentsTable(const char* /*name*/) override {
        return nullptr;
    }
    virtual bool destroyAttachmentsTable(const char* /*name*/) override {
        return true;
    }
    virtual OGRLayer* getAttachmentsTable(const char* /*name*/) override {
        return nullptr;
    }

protected:
    virtual bool isNameValid(const char* name) const override;
    virtual CPLString normalizeFieldName(const CPLString& name) const override;
    virtual void fillFeatureClasses() override;
    void addLayer(const CPLJSONObject& layer);

};

} // namespace ngs

#endif // NGSMEMSTORE_H
