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
#ifndef DATASET_H
#define DATASET_H

#include <string>
#include <memory>

namespace ngs {

using namespace std;

class DataStore;

/**
 * @brief The Dataset class is base class of DataStore. Each table, raster,
 * feature class, etc. are Dataset. The DataStore is an array of Datasets as
 * Map is array of Layers.
 */
class Dataset
{
public:
    enum Type {
        UNDEFINED,
        TABLE,
        REMOTE_TMS,
        LOCAL_TMS,
        LOCAL_RASTER,
        WMS,
        WFS,
        NGW_IMAGE
    };
public:
    Dataset(DataStore * datastore,
            const string& name, const string& alias = "");
    Type type() const;
    string name() const;
    string alias() const;
    int destroy();

    bool deleted() const;

protected:
    enum Type m_type;
    const string m_name;
    string m_alias;
    bool m_deleted;
    DataStore * const m_datastore;
};

typedef shared_ptr<Dataset> DatasetPtr;

}



#endif // DATASET_H
