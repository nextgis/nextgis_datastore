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
#ifndef NGSDATASTORE_H
#define NGSDATASTORE_H

#include "dataset.h"

namespace ngs {

/**
 * @brief The geodata storage and manipulation class for raster, vector geodata
 * and attachments
 */
class DataStore : public Dataset, public ISpatialDataset
{
public:
    DataStore(ObjectContainer * const parent = nullptr,
              const CPLString & name = "",
              const CPLString & path = "");
    virtual ~DataStore() = default;

    // static
public:
    static bool create(const char* path);
    static bool createMetadataTable(GDALDataset* ds);
    static bool createRastersTable(GDALDataset* ds);
    static bool createAttachmentsTable(GDALDataset* ds);

    // ISpatialDataset interface
public:
    virtual OGRSpatialReference *getSpatialReference() const override {
        return const_cast<OGRSpatialReference *>(&m_storeSpatialRef); }

    // Dataset interface
protected:
    virtual bool isNameValid(const char *name) const override;

protected:
    void enableJournal(bool enable);
    bool upgrade(int oldVersion);
/*
    virtual DatasetPtr createDataset(const CPLString &name,
                                     OGRFeatureDefn* const definition,
                                     const OGRSpatialReference *,
                                     OGRwkbGeometryType type = wkbUnknown,
                                     ProgressInfo *progressInfo = nullptr) override;

    static DataStorePtr create(const CPLString& path);
    static DataStorePtr open(const CPLString& path);

    //
    int createRemoteTMSRaster(const char* url, const char* name,
                              const char* alias, const char* copyright,
                              int epsg, int z_min, int z_max,
                              bool y_origin_top);

*/

protected:
    unsigned char m_disableJournalCounter;
    OGRSpatialReference m_storeSpatialRef;

};

}

#endif // NGSDATASTORE_H
