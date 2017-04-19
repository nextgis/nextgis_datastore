/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*  Author: NikitaFeodonit, nfeodonit@yandex.com
*******************************************************************************
*  Copyright (C) 2016-2017 NextGIS, <info@nextgis.com>
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#ifndef NGSGLVIEW_H
#define NGSGLVIEW_H

#include "mapview.h"

namespace ngs {

class GlView : public MapView
{
public:
    GlView();
    GlView(const CPLString& name, const CPLString& description,
            unsigned short epsg, const Envelope &bounds);
    virtual ~GlView() = default;

    // Map interface
public:
    virtual void setBackgroundColor(const ngsRGBA &color) override;

    // MapView interface
public:
    virtual bool draw(ngsDrawState state, const Progress &progress) override;

private:
    void clearBackground();

private:
    typedef struct _glcolor {
        float r;
        float g;
        float b;
        float a;
    } GlColor;
    GlColor m_glBkColor;

};

}  // namespace ngs

#endif  // NGSGLVIEW_H
