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
    short id() const;
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
    Map();
    Map(const CPLString& name, const CPLString& description, unsigned short epsg,
        double minX, double minY, double maxX, double maxY);
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

    int load(const char* path);
    int save(const char* path);
    int destroy();

    bool isDeleted() const;

    ngsRGBA getBackgroundColor() const;
    int setBackgroundColor(const ngsRGBA& color);
    bool isBackgroundChanged() const;

protected:
    void setBackgroundChanged(bool bkChanged);

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
};

typedef shared_ptr<Map> MapPtr;
typedef weak_ptr<Map> MapWPtr;
}
#endif // MAP_H
