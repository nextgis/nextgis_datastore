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
#include "layer.h"

#include "ngstore/util/constants.h"

namespace ngs {

// Layers
constexpr const char* LAYER_NAME = "name";
constexpr const char* LAYER_TYPE = "type";
constexpr const char* DEFAULT_LAYER_NAME = "new layer";

//------------------------------------------------------------------------------
// Layer
//------------------------------------------------------------------------------

Layer::Layer() : m_name(DEFAULT_LAYER_NAME), m_type(Layer::Type::Invalid)
{
}

Layer::Layer(const CPLString &name, Type type) : m_name(name),
    m_type(type)
{
}

int Layer::load(const JSONObject &store, DatasetContainerPtr dataStore,
                const CPLString &mapPath)
{
    m_name = store.getString(LAYER_NAME, DEFAULT_LAYER_NAME);
    unsigned int type = static_cast<unsigned int>(store.getInteger (
                                 LAYER_SOURCE_TYPE, ngsDatasetType (Undefined)));
    if(type & ngsDatasetType (Store)) {
        CPLString path = store.getString (LAYER_SOURCE, "");
        CPLString datasetName = CPLGetBasename (path);
        if(dataStore) {
            m_dataset = dataStore->getDataset (datasetName);
        }
        else {
            path = CPLGetDirname (path);
            // load dataset by path and name
            dataStore = DataStore::open (path);
            if(nullptr != dataStore) {
                if(mapPath.empty ())
                    m_dataset = dataStore->getDataset (datasetName);
                else
                    m_dataset = dataStore->getDataset (CPLFormFilename(mapPath,
                                                           datasetName, NULL));
            }
        }
    }
    return ngsErrorCodes::EC_SUCCESS;
}

JSONObject Layer::save(const CPLString &/*mapPath*/) const
{
    JSONObject out;
    out.add(LAYER_NAME, m_name);
    out.add(LAYER_TYPE, static_cast<int>(m_type));
// TODO: Check absolute or relative catalog path
//    if(nullptr != m_dataset) {
//        // relative or absolute path
//        if(mapPath.empty ()) {
//            out.add(LAYER_SOURCE, m_dataset->path ());
//        }
//        else {
//            CPLString relPath = CPLExtractRelativePath(mapPath,
//                                                       m_dataset->path (), NULL);
//            if(relPath.empty ())
//                relPath = m_dataset->path ();
//            out.add(LAYER_SOURCE, relPath);
//        }
//    }
    return out;
}

}
