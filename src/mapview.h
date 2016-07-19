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

namespace ngs {

class MapView : public Map, public MapTransform
{
public:
    enum DrawStage {
        Start = 1,
        Stop,
        Done,
        Process
    };

public:
    MapView();
    MapView(const CPLString& name, const CPLString& description, unsigned short epsg,
            double minX, double minY, double maxX, double maxY);
    virtual ~MapView();
    bool isDisplayInit() const;
    int initDisplay();
    int errorCode() const;
    void setDisplayInit(bool displayInit);
    bool cancel() const;
    int initBuffer(void* buffer, int width, int height);
    int draw(const ngsProgressFunc &progressFunc, void* progressArguments = nullptr);
    DrawStage getDrawStage() const;
    void setDrawStage(const DrawStage &drawStage);

public:
    void setErrorCode(int errorCode);
    void *getBufferData() const;
    int notify(double complete, const char* message);

protected:
    bool m_displayInit;
    int m_errorCode;
    CPLJoinableThread* m_hThread;
    bool m_cancel;
    void* m_bufferData;    
    ngsProgressFunc m_progressFunc;
    void* m_progressArguments;
    enum DrawStage m_drawStage;
};

}
#endif // MAPVIEW_H
