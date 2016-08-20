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
#ifndef MAP_H
#define MAP_H

#include "layer.h"
#include "api_priv.h"
#include "datastore.h"

namespace ngs {

using namespace std;

class MapStore;

class Map
{
public:
    Map();
    Map(const CPLString& name, const CPLString& description, unsigned short epsg,
        double minX, double minY, double maxX, double maxY);
    Map(DataStorePtr dataStore);
    Map(const CPLString& name, const CPLString& description, unsigned short epsg,
        double minX, double minY, double maxX, double maxY,
        DataStorePtr dataStore);
    virtual ~Map();
    CPLString name() const;
    void setName(const CPLString &name);

    CPLString description() const;
    void setDescription(const CPLString &description);

    unsigned short epsg() const;
    void setEpsg(unsigned short epsg);

    double minX() const;
    void setMinX(double minX);

    double minY() const;
    void setMinY(double minY);

    double maxX() const;
    void setMaxX(double maxX);

    double maxY() const;
    void setMaxY(double maxY);

    virtual int open(const char* path);
    virtual int save(const char* path);
    virtual int destroy();
    virtual int close();

    bool isDeleted() const;

    ngsRGBA getBackgroundColor() const;
    int setBackgroundColor(const ngsRGBA& color);
    bool isBackgroundChanged() const;

    virtual int createLayer(const CPLString &name, DatasetPtr dataset);
    size_t layerCount() const;

protected:
    void setBackgroundChanged(bool bkChanged);
    virtual LayerPtr createLayer(enum Layer::Type type);

protected:
    CPLString m_name;
    CPLString m_description;
    CPLString m_path;
    unsigned short m_epsg;
    double m_minX, m_minY, m_maxX, m_maxY;
    bool m_deleted;
    vector<LayerPtr> m_layers;
    ngsRGBA m_bkColor;
    bool m_bkChanged;
    DataStorePtr m_DataStore;
};

typedef shared_ptr<Map> MapPtr;
// typedef weak_ptr<Map> MapWPtr;
}
#endif // MAP_H
