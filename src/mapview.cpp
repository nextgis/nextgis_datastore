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
#include "mapview.h"
#include "api.h"

#include <iostream>

#include <GLES2/gl2.h>

using namespace ngs;
using namespace std;

MapView::MapView(FeaturePtr feature, MapStore *mapstore) : Map(feature, mapstore),
    m_displayInit(false)
{
    initDisplay();
}

bool MapView::isDisplayInit() const
{
    return m_displayInit;
}

int MapView::initDisplay()
{
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY) {
        return ngsErrorCodes::GL_GET_DISPLAY_FAILED;
    }

    EGLint major, minor;
    if (!eglInitialize(m_display, &major, &minor)) {
        return ngsErrorCodes::GL_INIT_FAILED;
    }

    if ((major <= 1) && (minor < 1)) {
        return ngsErrorCodes::GL_UNSUPPORTED_VERSION;
    }

#ifdef _DEBUG
    cout << "Vendor: " << eglQueryString(m_display, EGL_VENDOR) << endl;
    cout << "Version: " << eglQueryString(m_display, EGL_VERSION) << endl;
    cout << "Client APIs: " << eglQueryString(m_display, EGL_CLIENT_APIS) << endl;
    cout << "Client Extensions: " << eglQueryString(m_display, EGL_EXTENSIONS) << endl;
#endif // _DEBUG

    m_displayInit = true;
    return ngsErrorCodes::SUCCESS;
}
