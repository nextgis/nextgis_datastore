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
#ifndef DATASTORE_H
#define DATASTORE_H

#include "ogrsf_frmts.h"
#include "gdal_frmts.h"

#include <string>
#include <memory>

namespace ngs {

enum ngsLayerType {
    REMOTE_TMS,
    LOCAL_TMS,
    LOCAL_RASTER,
    WMS,
    WFS,
    NGW_IMAGE
};

using namespace std;

class OGRFeaturePtr : public unique_ptr< OGRFeature, void (*)(OGRFeature*) >
{
public:
    OGRFeaturePtr(OGRFeature* pFeature);
    OGRFeaturePtr();
    OGRFeaturePtr( OGRFeaturePtr& other );
    OGRFeaturePtr& operator=(OGRFeaturePtr& feature);
    OGRFeaturePtr& operator=(OGRFeature* pFeature);
    operator OGRFeature*() const;
};


/**
 * @brief The main geodata storage and manipulation class
 */
class DataStore
{
public:
    DataStore(const char* path, const char* dataPath, const char* cachePath);
    ~DataStore();
    int create();
    int open();
    int destroy();
    int openOrCreate();
    int createRemoteTMSRaster(const char* url, const char* name,
                              const char* alias, const char* copyright,
                              int epsg, int z_min, int z_max,
                              bool y_origin_top);
public:
    string& reportFormats();

protected:
    void initGDAL();
    void registerDrivers();
    int createMetadataTable();
    int createRastersTable();
    int createAttachmentsTable();
    int upgrade(int oldVersion);

protected:
    string m_sPath;
    string m_sCachePath;
    string m_sDataPath;
    GDALDataset *m_poDS;
    bool m_bDriversLoaded;

private:
    string m_sFormats;
};

}

#endif // DATASTORE_H
