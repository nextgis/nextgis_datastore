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

MapView::MapView() : Map(), MapTransform(480, 640), m_displayInit(false)
{
}

MapView::MapView(const CPLString &name, const CPLString &description,
                 unsigned short epsg, double minX, double minY, double maxX,
                 double maxY) : Map(name, description, epsg, minX, minY, maxX,
                                    maxY), MapTransform(480, 640),
    m_displayInit(false), m_progressFunc(nullptr)
{
}

MapView::MapView(DataStorePtr dataSource) : Map(dataSource),
    MapTransform(480, 640), m_displayInit(false), m_progressFunc(nullptr)
{
}

MapView::MapView(const CPLString &name, const CPLString &description,
                 unsigned short epsg, double minX, double minY, double maxX,
                 double maxY, DataStorePtr dataSource) : Map(name, description,
                    epsg, minX, minY, maxX, maxY, dataSource),
    MapTransform(480, 640), m_displayInit(false), m_progressFunc(nullptr)
{
}

MapView::~MapView()
{
    close ();
}

bool MapView::isDisplayInit() const
{
    return m_displayInit;
}

int MapView::initDisplay()
{
    if(m_glFunctions.init ()) {
        m_displayInit = true;
        return ngsErrorCodes::SUCCESS;
    }

    return ngsErrorCodes::INIT_FAILED;
}

int MapView::draw(const ngsProgressFunc &progressFunc, void* progressArguments)
{
/*    m_progressFunc = progressFunc;
    m_progressArguments = progressArguments;

    if(m_drawStage == DrawStage::Process){
        m_drawStage = DrawStage::Stop;
    }
    else {
        m_drawStage = DrawStage::Start;
    }*/

    m_glFunctions.clearBackground ();
    m_glFunctions.prepare (getSceneMatrix());
    m_glFunctions.testDrawPreserved (); //.testDraw ();

    return ngsErrorCodes::SUCCESS;
}
/*
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

/*    glView->draw();
    renderPercent = 1;*//*

    // Notify drawing progress
    renderPercent /= m_layers.size ();
    if( renderPercent - m_renderPercent > NOTIFY_PERCENT ) {
        m_renderPercent = renderPercent;
    }
    else if(isEqual(renderPercent, 1.0))
        return true;
    return isEqual(m_renderPercent, 1.0) ? true : false;
}
*/

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

int MapView::createLayer(const CPLString &name, DatasetPtr dataset)
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

LayerPtr MapView::createLayer(Layer::Type type)
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


int ngs::MapView::setBackgroundColor(const ngsRGBA &color)
{
    m_glFunctions.setBackgroundColor(color);
    return Map::setBackgroundColor (color);
}
