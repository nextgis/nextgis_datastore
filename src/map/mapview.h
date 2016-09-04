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
#ifndef MAPVIEW_H
#define MAPVIEW_H

#include "map.h"
#include "maptransform.h"
#include "glview.h"

namespace ngs {

class MapView : public Map, public MapTransform
{
public:
    enum class DrawStage {
        Start = 1,
        Stop,
        Done,
        Process
    };

public:
    MapView();
    MapView(const CPLString& name, const CPLString& description,
            unsigned short epsg,
            double minX, double minY, double maxX, double maxY);
    MapView(DataStorePtr dataSource);
    MapView(const CPLString& name, const CPLString& description,
            unsigned short epsg,
            double minX, double minY, double maxX, double maxY,
            DataStorePtr dataSource);
    virtual ~MapView();
    bool isDisplayInit() const;
    int initDisplay();
//    int initBuffer(void* buffer, int width, int height, bool isYAxisInverted);
    int draw(const ngsProgressFunc &progressFunc, void* progressArguments = nullptr);
    DrawStage getDrawStage() const;

protected:
//    bool render(const GlView* glView);
    void prepareRender();
    void cancelPrepareRender();
    void setDrawStage(const DrawStage &drawStage);
    int notify(double complete, const char* message);

protected:
    bool m_displayInit;
    ngsProgressFunc m_progressFunc;
    void* m_progressArguments;
    double m_renderPercent;
    enum DrawStage m_drawStage;
    GlFuctions m_glFunctions;

    // Map interface
public:
    virtual int createLayer(const CPLString &name, DatasetPtr dataset) override;

protected:
    virtual LayerPtr createLayer(enum Layer::Type type) override;

    // Map interface
public:
    virtual int setBackgroundColor(const ngsRGBA &color) override;
};

}
#endif // MAPVIEW_H
