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

#include <iostream>
#include <string>

#include "api_priv.h"
#include "renderlayer.h"
#include "ngstore/util/constants.h"

#ifdef _DEBUG
#   include <chrono>
#endif //DEBUG

namespace ngs {


MapView::MapView() : Map(), MapTransform(480, 640)
{
}

MapView::MapView(const CPLString &name, const CPLString &description,
                 unsigned short epsg, const Envelope &bounds) :
    Map(name, description, epsg, bounds), MapTransform(480, 640)
{
}

//int MapView::draw(enum ngsDrawState state, const ngsProgressFunc &progressFunc,
//                  void* progressArguments)
//{
//    m_glFunctions.clearBackground ();

//    if(m_layers.empty()) {
//        return ngsErrorCodes::EC_SUCCESS;
//    }

//    m_progressFunc = progressFunc;
//    m_progressArguments = progressArguments;

//    float level = 0;
//    for(auto it = m_layers.rbegin (); it != m_layers.rend (); ++it) {
//        LayerPtr layer = *it;
//        RenderLayer* renderLayer = ngsStaticCast(RenderLayer, layer);
//        renderLayer->draw(state, getExtent (), getZoom (), level++);
//    }

//    /* debug
//    m_glFunctions.clearBackground ();
//    m_glFunctions.prepare (getSceneMatrix());
//    m_glFunctions.testDrawPreserved (); //.testDraw ();
//*/
//    return ngsErrorCodes::EC_SUCCESS;
//}

//int MapView::notify()
//{
//    if(nullptr != m_progressFunc) {
//        float completePortion = 0;
//        bool fullComlete = true;
//        int featureCount = -1;
//        for(auto it = m_layers.rbegin (); it != m_layers.rend (); ++it) {
//            LayerPtr layer = *it;
//            RenderLayer* renderLayer = ngsStaticCast(RenderLayer, layer);
//            completePortion += renderLayer->getComplete ();
//            fullComlete &= renderLayer->isComplete();
//            if(isDebugMode()) {
//                int count = renderLayer->getFeatureCount();
//                if (count > -1) {
//                    if (-1 == featureCount) {
//                        featureCount = 0;
//                    }
//                    featureCount += count;
//                }
//            }
//        }
//        completePortion /= m_layers.size();
//        if (fullComlete) {
//            // 0..1 means progress percent from 0 to 1,
//            // > 1 means full completing, then progress percent equals (completePortion - 1)
//            completePortion += 1.0f;
//        }

//        CPLString message;
//        if(isDebugMode()) {
//            message = CPLSPrintf("%d", featureCount);
//        }

//        return m_progressFunc(
//                    ngsErrorCodes::EC_SUCCESS/*getId()*/,
//                    static_cast<double>(completePortion),
//                    isDebugMode() ? message.c_str() : nullptr,
//                    m_progressArguments);
//    }
//    return TRUE;
//}

//int MapView::createLayer(const CPLString &name, DatasetPtr dataset)
//{
//    LayerPtr layer;
//    if(dataset->type () & ngsDatasetType(Featureset)) {
//        layer.reset (new FeatureRenderLayer(name, dataset));
//    }

//    if(nullptr == layer)
//        return ngsErrorCodes::EC_UNSUPPORTED;

//    RenderLayer* renderLayer = ngsStaticCast(RenderLayer, layer);
//    if(renderLayer)
//        renderLayer->m_mapView = this;
//    m_layers.push_back (layer);
//    return ngsErrorCodes::EC_SUCCESS;
//}

LayerPtr MapView::createLayer(Layer::Type /*type*/)
{
//    switch (type) {
//    case Layer::Type::Invalid:
        return LayerPtr();
//    case Layer::Type::Vector:
//    {
//        RenderLayer* renderLayer = new FeatureRenderLayer;
//        renderLayer->m_mapView = this;
//        return LayerPtr(renderLayer);
//    }
//    case Layer::Type::Group:
//    case Layer::Type::Raster:
//        // TODO: add raster and group layers create
//        return Map::createLayer (type);
//    }
}

}
