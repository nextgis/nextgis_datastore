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
class DataStore : public Dataset, public SpatialDataset
{
public:
    DataStore(ObjectContainer * const parent = nullptr,
              const CPLString & name = "",
              const CPLString & path = "");
    virtual ~DataStore();

    // static
public:
    static bool create(const char* path);
    static const char* extension();

    // Object interface
public:
    virtual bool canDestroy() const override { return access(m_path, W_OK) == 0; }

    // Dataset interface
public:
    virtual bool open(unsigned int openFlags,
                      const Options &options = Options()) override;

protected:
    virtual bool isNameValid(const char *name) const override;
    virtual void fillFeatureClasses() override;

protected:
    void enableJournal(bool enable);
    bool upgrade(int oldVersion);

protected:
    unsigned char m_disableJournalCounter;
};

}

#endif // NGSDATASTORE_H
