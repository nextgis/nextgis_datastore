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

#include "glview.h"

#include "gl/glfunctions.h"

namespace ngs {

GlView::GlView() : MapView()
{

}

GlView::GlView(const CPLString &name, const CPLString &description,
               unsigned short epsg, const Envelope &bounds) :
    MapView(name, description, epsg, bounds)
{

}

void GlView::setBackgroundColor(const ngsRGBA &color)
{
    Map::setBackgroundColor(color);
    m_glBkColor.r = float(color.R) / 255;
    m_glBkColor.g = float(color.G) / 255;
    m_glBkColor.b = float(color.B) / 255;
    m_glBkColor.a = float(color.A) / 255;
}

// NOTE: Should be run on OpenGL current context
void GlView::clearBackground()
{
    ngsCheckGLEerror(glClearColor(m_glBkColor.r, m_glBkColor.g, m_glBkColor.b,
                                  m_glBkColor.a));
    ngsCheckGLEerror(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}


}

