/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016,2017 NextGIS, <info@nextgis.com>
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
#ifndef NGSMAP_H
#define NGSMAP_H

#include "api_priv.h"

#include "layer.h"

#include "ds/datastore.h"

namespace ngs {

class Map
{
public:
    Map();
    Map(const CPLString& name, const CPLString& description, unsigned short epsg,
        double minX, double minY, double maxX, double maxY);
    virtual ~Map() = default;

    const CPLString &getName() const {return m_name;}
    void setName(const CPLString &name) { m_name = name;}
    const CPLString &getDescription() const {return m_description;}
    void setDescription(const CPLString &description) {m_description = description;}
    unsigned short getEpsg() const {return m_epsg;}
    void setEpsg(unsigned short epsg) {m_epsg = epsg;}
    void setBounds(double minX, double minY, double maxX, double maxY) {
        m_minX = minX;
        m_minY = minY;
        m_maxX = maxX;
        m_maxY = maxY;
    }
    double getMinX() const {return m_minX;}
    double getMinY() const {return m_minY;}
    double getMaxX() const {return m_maxX;}
    double getMaxY() const {return m_maxY;}
    ngsRGBA getBackgroundColor() const {return  m_bkColor;}
    void setBackgroundColor(const ngsRGBA& color) {m_bkColor = color;}
    bool getRelativePaths() const {return m_relativePaths;}
    void setRelativePaths(bool relativePaths) {m_relativePaths = relativePaths;}

    virtual int open(const char* path);
    virtual bool save(const char* path);
    virtual bool close();

//    virtual int createLayer(const CPLString &name, DatasetPtr dataset);
    size_t layerCount() const {return m_layers.size();}

protected:
    virtual LayerPtr createLayer(enum Layer::Type type);

protected:
    CPLString m_name;
    CPLString m_description;
    unsigned short m_epsg;
    double m_minX, m_minY, m_maxX, m_maxY;
    std::vector<LayerPtr> m_layers;
    ngsRGBA m_bkColor;
    bool m_relativePaths;
};

}
#endif // NGSMAP_H
