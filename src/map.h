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

#include <string>
#include <memory>
#include <vector>

#include "table.h"
#include "api.h"

namespace ngs {

using namespace std;

class MapStore;

class Layer
{
public:
    enum Type {
        GROUP,
        VECTOR,
        RASTER
    };

public:
    Layer();
    GIntBig id() const;
protected:
    string m_name;
    enum Type m_type;

/*    DatasetPtr m_dataset;
    Style m_style;
*/
};

typedef shared_ptr<Layer> LayerPtr;

class Map
{
public:
    Map(FeaturePtr feature, MapStore * mapstore);
    Map(const string& name, const string& description, short epsg, double minX,
        double minY, double maxX, double maxY, MapStore * mapstore);
    virtual ~Map();
    string name() const;
    void setName(const string &name);

    string description() const;
    void setDescription(const string &description);

    short epsg() const;
    void setEpsg(short epsg);

    double minX() const;
    void setMinX(double minX);

    double minY() const;
    void setMinY(double minY);

    double maxX() const;
    void setMaxX(double maxX);

    double maxY() const;
    void setMaxY(double maxY);

    long id() const;
    void setId(long id);

    vector<GIntBig> getLayerOrder() const;

    int save();
    int destroy();

    bool isDeleted() const;

    ngsRGBA getBackgroundColor() const;
    int setBackgroundColor(const ngsRGBA& color);

protected:
    int loadLayers(const GIntBig* pValues, int count);

protected:
    MapStore * const m_mapstore;
    string m_name;
    string m_description;
    short m_epsg;
    double m_minX, m_minY, m_maxX, m_maxY;
    long m_id;
    bool m_deleted;
    vector<LayerPtr> m_layers;
    ngsRGBA m_bkColor;
};

typedef shared_ptr<Map> MapPtr;
typedef weak_ptr<Map> MapWPtr;
}
#endif // MAP_H
