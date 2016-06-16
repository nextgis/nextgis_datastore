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

// TODO: init and uninit in drawing thread.
//https://mkonrad.net/2014/12/08/android-off-screen-rendering-using-egl-pixelbuffers.html
//http://stackoverflow.com/questions/214437/opengl-fast-off-screen-rendering
//http://stackoverflow.com/questions/14785007/can-i-use-opengl-for-off-screen-rendering/14796456#14796456
//https://gist.github.com/CartBlanche/1271517
//http://stackoverflow.com/questions/21151259/replacing-glreadpixels-with-egl-khr-image-base-for-faster-pixel-copy
//https://vec.io/posts/faster-alternatives-to-glreadpixels-and-glteximage2d-in-opengl-es
//https://www.khronos.org/registry/egl/sdk/docs/man/html/eglIntro.xhtml
//https://wiki.maemo.org/SimpleGL_example
//http://stackoverflow.com/questions/12906971/difference-from-eglcreatepbuffersurface-and-eglcreatepixmapsurface-with-opengl-e
//http://stackoverflow.com/questions/25504188/is-it-possible-to-use-pixmaps-on-android-via-java-api-for-gles

void RenderingThread(void * view)
{
    MapView* pMapView = static_cast<MapView*>(view);
    EGLDisplay display;
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        pMapView->setErrorCode(ngsErrorCodes::GL_GET_DISPLAY_FAILED);
        return;
    }

    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) {
        pMapView->setErrorCode(ngsErrorCodes::GL_INIT_FAILED);
        return;
    }

    if ((major <= 1) && (minor < 1)) {
        pMapView->setErrorCode(ngsErrorCodes::GL_UNSUPPORTED_VERSION);
        return;
    }

#ifdef _DEBUG
    cout << "Vendor: " << eglQueryString(display, EGL_VENDOR) << endl;
    cout << "Version: " << eglQueryString(display, EGL_VERSION) << endl;
    cout << "Client APIs: " << eglQueryString(display, EGL_CLIENT_APIS) << endl;
    cout << "Client Extensions: " << eglQueryString(display, EGL_EXTENSIONS) << endl;
#endif // _DEBUG

    pMapView->setErrorCode(ngsErrorCodes::SUCCESS);
    pMapView->setDisplayInit (true);

    // start rendering loop here
    while(!pMapView->cancel ()){
        // get task from quere
        // if not tasks sleep else
        // render layers

        CPLSleep(0.3);
    }

    pMapView->setDisplayInit (false);

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate ( display );

    display = EGL_NO_DISPLAY;
}

MapView::MapView(FeaturePtr feature, MapStore *mapstore) : Map(feature, mapstore),
    m_displayInit(false), m_cancel(false)
{
    initDisplay();
}

MapView::~MapView()
{
    m_cancel = true;
    CPLJoinThread(m_hThread);
}

bool MapView::isDisplayInit() const
{
    return m_displayInit;
}

int MapView::initDisplay()
{
    m_hThread = CPLCreateJoinableThread(RenderingThread, this);
    return m_errorCode;
}

int MapView::errorCode() const
{
    return m_errorCode;
}

void MapView::setErrorCode(int errorCode)
{
    m_errorCode = errorCode;
}

void MapView::setDisplayInit(bool displayInit)
{
    m_displayInit = displayInit;
}

bool MapView::cancel() const
{
    return m_cancel;
}
