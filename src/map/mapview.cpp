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
#include "api_priv.h"
#include "constants.h"
#include "renderlayer.h"

#include <iostream>
#ifdef _DEBUG
#include <chrono>
#endif //DEBUG

using namespace ngs;
using namespace std;

#define THREAD_LOOP_SLEEP 0.35

void ngs::RenderingThread(void * view)
{
    MapView* mapView = static_cast<MapView*>(view);
    GlView glView;
    if (!glView.init ()) {
        mapView->m_errorCode = ngsErrorCodes::INIT_FAILED;
        return;
    }

    mapView->m_errorCode = ngsErrorCodes::SUCCESS;
    mapView->setDisplayInit (true);

//    chrono::high_resolution_clock::time_point t1;

    // start rendering loop here
    while(!mapView->m_cancel){
        if(mapView->m_sizeChanged){
            // Put partially or full scene to buffer
            //glView.fillBuffer (mapView->getBufferData ());
            //mapView->notify (1.0, nullptr);

            glView.setSize (mapView->getDisplayWidht (),
                            mapView->getDisplayHeight ())  ;
            mapView->m_sizeChanged = false;
        }

        if(!glView.isOk ()){
#ifdef _DEBUG
            cerr << "No surface to draw" << endl;
#endif //_DEBUG
            CPLError(CE_Failure, CPLE_OpenFailed, "Get GL version failed.");
            CPLSleep(THREAD_LOOP_SLEEP);
            continue;
        }

        switch(mapView->getDrawStage ()){
        case MapView::DrawStage::Stop:
#ifdef _DEBUG
            cout << "MapView::DrawStage::Stop" << endl;
#endif // _DEBUG
            // Stop extract data threads
            mapView->cancelPrepareRender ();
            mapView->setDrawStage (MapView::DrawStage::Start);
            break;
        case MapView::DrawStage::Process:
            if(mapView->render (&glView)) {
                // if no more geodata - finish
                //glView.draw ();
                glView.fillBuffer (mapView->getBufferData ());
                // if notify return false - set stage to done
                mapView->notify (1.0, nullptr);
                mapView->setDrawStage (MapView::DrawStage::Done);

#ifdef _DEBUG
            // benchmark
//            auto duration = chrono::duration_cast<chrono::milliseconds>(
//                        chrono::high_resolution_clock::now() - t1 ).count();
//            cout << "GL Canvas refresh: " << duration << " ms" << endl;
#endif // _DEBUG

            // one more notify
            //CPLSleep(THREAD_LOOP_SLEEP);
            //glView.fillBuffer (mapView->getBufferData ());
            //mapView->notify (1.0, nullptr);
            }

            CPLSleep(THREAD_LOOP_SLEEP);
            break;
        case MapView::DrawStage::Start:
#ifdef _DEBUG
            cout << "MapView::DrawStage::Start" << endl;
            // benchmark
//            t1 = chrono::high_resolution_clock::now();
#endif // _DEBUG
            if(mapView->isBackgroundChanged ()){
                glView.setBackgroundColor (mapView->getBackgroundColor ());
                mapView->setBackgroundChanged (false);
            }

            glView.prepare (mapView->getSceneMatrix ());
            // Start extract data for current extent
            mapView->prepareRender ();
            mapView->setDrawStage (MapView::DrawStage::Process);
            break;

        case MapView::DrawStage::Done:
            // if not tasks sleep 100ms
            CPLSleep(THREAD_LOOP_SLEEP);
        }
    }

#ifdef _DEBUG
    cout << "Exit draw thread" << endl;
#endif // _DEBUG

    mapView->setDisplayInit (false);
}

MapView::MapView() : Map(), MapTransform(480, 640), m_displayInit(false),
    m_cancel(false), m_bufferData(nullptr), m_progressFunc(nullptr)
{
    initDisplay();
}

MapView::MapView(const CPLString &name, const CPLString &description,
                 unsigned short epsg, double minX, double minY, double maxX,
                 double maxY) : Map(name, description, epsg, minX, minY, maxX,
                                    maxY), MapTransform(480, 640),
    m_displayInit(false), m_cancel(false), m_bufferData(nullptr),
    m_progressFunc(nullptr)
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

void MapView::setDisplayInit(bool displayInit)
{
    m_displayInit = displayInit;
}

void *MapView::getBufferData() const
{
    if(m_sizeChanged)
        return nullptr;
    return m_bufferData;
}

int MapView::initBuffer(void *buffer, int width, int height, bool isYAxisInverted)
{
    m_bufferData = buffer;

    setDisplaySize (width, height, isYAxisInverted);

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

    return ngsErrorCodes::SUCCESS;
}

bool MapView::render(const GlView *glView)
{
    if(m_layers.empty())
        return true;

    double renderPercent = 0;

    for(auto it = m_layers.rbegin (); it != m_layers.rend (); ++it) {
        LayerPtr layer = *it;
        RenderLayer* renderLayer = static_cast<RenderLayer*>(layer.get());
        renderPercent += renderLayer->render(glView);
    }

    //glView->draw();
    // Notify drawing progress
    renderPercent /= m_layers.size ();
    if( renderPercent - m_renderPercent > NOTIFY_PERCENT ) {
        m_renderPercent = renderPercent;
    }
    else if(isEqual(renderPercent, 1.0))
        return true;
    return isEqual(m_renderPercent, 1.0) ? true : false;
}

void MapView::prepareRender()
{
    // FIXME: need to limit prepare thread for CPU core count or some constant
    // as they can produce lot of data and overload memory. See CPLWorkerThreadPool
    m_renderPercent = 0;
    for( size_t i = 0; i < m_layers.size (); ++i ) {
        RenderLayer* renderLayer = static_cast<RenderLayer*>(m_layers[i].get());
        renderLayer->prepareRender (getExtent (), getZoom(),
                                    static_cast<float>(i * 10) );
    }
}

void MapView::cancelPrepareRender()
{
    for(LayerPtr& layer : m_layers ) {
        RenderLayer* renderLayer = static_cast<RenderLayer*>(layer.get());
        renderLayer->cancelPrepareRender ();
    }
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

int ngs::MapView::createLayer(const CPLString &name, DatasetPtr dataset)
{
    LayerPtr layer;
    if(dataset->type () & ngsDatasetType(Featureset)) {
        layer.reset (new FeatureRenderLayer(name, dataset));
    }

    if(nullptr == layer)
        return ngsErrorCodes::UNSUPPORTED;

    m_layers.push_back (layer);
    return ngsErrorCodes::SUCCESS;
}

LayerPtr ngs::MapView::createLayer(Layer::Type type)
{
    switch (type) {
    case Layer::Type::Invalid:
        return LayerPtr();
    case Layer::Type::Vector:
        return LayerPtr(new FeatureRenderLayer);
    case Layer::Type::Group:
    case Layer::Type::Raster:
        // TODO:
        return Map::createLayer (type);
    }
}
