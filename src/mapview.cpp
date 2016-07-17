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
#include "glview.h"

#include <iostream>

using namespace ngs;
using namespace std;

#define THREAD_LOOP_SLEEP 0.1

void RenderingThread(void * view)
{
    MapView* pMapView = static_cast<MapView*>(view);
    GlView glView;
    if (!glView.init ()) {
        pMapView->setErrorCode(ngsErrorCodes::INIT_FAILED);
        return;
    }    

    pMapView->setErrorCode(ngsErrorCodes::SUCCESS);
    pMapView->setDisplayInit (true);

    // start rendering loop here
    while(!pMapView->cancel ()){
        if(pMapView->isSizeChanged ()){
            glView.setSize (pMapView->getDisplayWidht (),
                            pMapView->getDisplayHeight ());
            pMapView->setSizeChanged (false);
        }

        if(!glView.isOk ()){
#ifdef _DEBUG
            cerr << "No surface to draw" << endl;
#endif //_DEBUG
            CPLError(CE_Failure, CPLE_OpenFailed, "Get GL version failed.");
            CPLSleep(THREAD_LOOP_SLEEP);
            continue;
        }

        switch(pMapView->getDrawStage ()){
        case MapView::DrawStage::Stop:
#ifdef _DEBUG
            cout << "MapView::DrawStage::Stop" << endl;
#endif // _DEBUG
            // stop extract data threads
            pMapView->setDrawStage (MapView::DrawStage::Start);
            break;
        case MapView::DrawStage::Process:
#ifdef _DEBUG
            cout << "MapView::DrawStage::Process" << endl;
#endif // _DEBUG
            // TODO: get ready portion of geodata to draw
            // fill buffer with pixels glReadPixels
            // and notify listeners
            glView.draw ();

            // if no more geodata - finish
            glView.fillBuffer (pMapView->getBufferData ());
            pMapView->setDrawStage (MapView::DrawStage::Done);
            // if notify return false - set stage to done
            pMapView->notify (1.0, nullptr);
            break;
        case MapView::DrawStage::Start:
#ifdef _DEBUG
            cout << "MapView::DrawStage::Start" << endl;
#endif // _DEBUG
            if(pMapView->isBackgroundChanged ()){
                glView.setBackgroundColor (pMapView->getBackgroundColor ());
                pMapView->setBackgroundChanged (false);
            }

            glView.prepare (pMapView->getSceneMatrix ());
            pMapView->setDrawStage (MapView::DrawStage::Process);
            break;

        case MapView::DrawStage::Done:
            // if not tasks sleep 100ms
            CPLSleep(THREAD_LOOP_SLEEP);
        }
    }

#ifdef _DEBUG
    cout << "Exit draw thread" << endl;
#endif // _DEBUG

    pMapView->setDisplayInit (false);
}

MapView::MapView(FeaturePtr feature, MapStore *mapstore) : Map(feature, mapstore),
    MapTransform(480, 640), m_displayInit(false), m_cancel(false),
    m_bufferData(nullptr), m_progressFunc(nullptr)
{
    initDisplay();
}

MapView::~MapView()
{
    m_cancel = true;
    // wait thread exit
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

void *MapView::getBufferData() const
{
    return m_bufferData;
}

int MapView::initBuffer(void *buffer, int width, int height)
{
    m_bufferData = buffer;

    setDisplaySize (width, height);

    return ngsErrorCodes::SUCCESS;
}

int MapView::draw(const ngsProgressFunc &progressFunc, void* progressArguments)
{
    m_progressFunc = progressFunc;
    m_progressArguments = progressArguments;

    if(m_drawStage == DrawStage::Process){
        m_drawStage = DrawStage::Stop;
    }
    else {
        m_drawStage = DrawStage::Start;
    }

    // TODO:
    // 1.   Finish drawing if any and put scene to buffer
    // 1.1. Stop extract data threads
    // 1.2. Put current drawn scene to buffer
    // 1.3. Notify drawing progress
    // 2.   Start new drawing
    // 2.1. Start extract data for current extent
    // 2.2. Put partially or full scene to buffer
    // 2.3. Notify drawing progress

    return ngsErrorCodes::SUCCESS;
}

int MapView::notify(double complete, const char *message)
{
    if(nullptr != m_progressFunc) {
        return m_progressFunc(complete, message, m_progressArguments);
    }
    return TRUE;
}

enum MapView::DrawStage MapView::getDrawStage() const
{
    return m_drawStage;
}

void MapView::setDrawStage(const DrawStage &drawStage)
{
    m_drawStage = drawStage;
}



