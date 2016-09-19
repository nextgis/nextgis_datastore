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
#include "renderlayer.h"
#include "geometryutil.h"
#include "featuredataset.h"
#include "constants.h"

#include <iostream>
#include <algorithm>
#ifdef _DEBUG
#  include <chrono>
#endif //DEBUG

using namespace ngs;

void ngs::FillGLBufferThread(void * layer)
{
    RenderLayer* renderLayer = static_cast<RenderLayer*>(layer);

#ifdef _DEBUG
    chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();
     cout << "Start FillGLBufferThread" << endl;
#endif //DEBUG
    renderLayer->fillRenderBuffers ();

#ifdef _DEBUG
    auto duration = chrono::duration_cast<chrono::milliseconds>(
                            chrono::high_resolution_clock::now() - t1 ).count();
    cout << "Finish FillGLBufferThread at " << duration << " ms" << endl;
#endif //DEBUG
    renderLayer->finishFillThread();
}

//------------------------------------------------------------------------------
// RenderLayer
//------------------------------------------------------------------------------

RenderLayer::RenderLayer() : Layer(), m_hPrepareThread(nullptr), m_mapView(nullptr),
    m_hThreadLock(CPLCreateLock(LOCK_SPIN))
{
    m_type = Layer::Type::Invalid;
}

RenderLayer::RenderLayer(const CPLString &name, DatasetPtr dataset) :
    Layer(name, dataset), m_hPrepareThread(nullptr), m_mapView(nullptr),
    m_hThreadLock(CPLCreateLock(LOCK_SPIN))
{
    m_type = Layer::Type::Invalid;
}

RenderLayer::~RenderLayer()
{
    cancelFillThread();    
    CPLDestroyLock(m_hThreadLock);
}


void RenderLayer::prepareFillThread(OGREnvelope extent, double zoom, float level)
{
    // start bucket of buffers preparation
    m_cancelPrepare = false;
    //m_renderPercent = 0;
    m_renderExtent = extent;
    m_renderZoom = static_cast<unsigned char>(zoom);
    m_renderLevel = level;

    // create or refill virtual tiles for current extent and zooms

    CPLLockHolder tilesHolder(m_hThreadLock);
    if(nullptr == m_hPrepareThread)
        m_hPrepareThread = CPLCreateJoinableThread(FillGLBufferThread, this);
}

void RenderLayer::cancelFillThread()
{
    CPLLockHolder tilesHolder(m_hThreadLock);
    if(m_hPrepareThread) {
        m_cancelPrepare = true;
        // wait thread exit
        CPLJoinThread(m_hPrepareThread);
        m_hPrepareThread = nullptr;
    }
}

void RenderLayer::draw(ngsDrawState state, OGREnvelope extent, double zoom,
                         float level)
{
    switch (state) {
    case DS_REDRAW:
        // clear cache
        clearTiles();
    case DS_NORMAL:
        // start extract data thread
        prepareFillThread(extent, zoom, level);
    case DS_PRESERVED:
        // draw cached data
        drawTiles ();
        break;
    }
}

float RenderLayer::getComplete() const
{
    return m_complete;
}

void RenderLayer::finishFillThread()
{
    CPLLockHolder tilesHolder(m_hThreadLock);
    m_hPrepareThread = nullptr;
}

//------------------------------------------------------------------------------
// FeatureRenderLayer
//------------------------------------------------------------------------------

struct tileIs {
    tileIs( const GlBufferBucket &item ) {
        toFind.x = item.X();
        toFind.y = item.Y();
        toFind.z = item.zoom ();
        toFind.crossExtent = item.crossExtent();
    }
    bool operator() (const TileItem &item) {
        return item.x == toFind.x &&
               item.y == toFind.y &&
               item.z == toFind.z &&
               item.crossExtent == toFind.crossExtent;
    }
    TileItem toFind;
};

FeatureRenderLayer::FeatureRenderLayer() : RenderLayer(),
    m_hTilesLock(CPLCreateLock(LOCK_SPIN))
{
    m_type = Layer::Type::Vector;
}

FeatureRenderLayer::FeatureRenderLayer(const CPLString &name, DatasetPtr dataset) :
    RenderLayer(name, dataset), m_hTilesLock(CPLCreateLock(LOCK_SPIN))
{
    m_type = Layer::Type::Vector;
    initStyle();
}

FeatureRenderLayer::~FeatureRenderLayer()
{
    CPLDestroyLock(m_hTilesLock);
}

void FeatureRenderLayer::initStyle()
{
    FeatureDataset* featureDataset = ngsDynamicCast(FeatureDataset, m_dataset);
    OGRwkbGeometryType geomType = featureDataset->getGeometryType ();
    switch(OGR_GT_Flatten(geomType)) {
    case wkbMultiPoint:
    case wkbPoint:
        m_style.reset(new SimpleFillStyle());
        m_style->setColor ({0, 255, 0, 255});
        break;
    case wkbMultiLineString:
    case wkbLineString:
        //->setColor ({0, 0, 255, 255});
        break;
    case wkbMultiPolygon:
    case wkbPolygon:
        m_style.reset(new SimpleFillStyle());
        m_style->setColor ({255, 0, 0, 255});
        break;
    default:
        break;
    }
}

void FeatureRenderLayer::fillRenderBuffers()
{
    m_complete = 0;
    float counter = 0;
    OGREnvelope renderExtent = m_renderExtent;
    FeaturePtr feature;
    OGRGeometry* geom;
    FeatureDataset* featureDataset = ngsDynamicCast(FeatureDataset, m_dataset);
    bool fidExists;

    vector<TileItem> tiles = getTilesForExtent(m_renderExtent, m_renderZoom,
                                            false, m_mapView->getXAxisLooped ());
    // remove already exist tiles
    auto iter = m_tiles.begin ();
    while (iter != m_tiles.end()) {
        auto pos = find_if(tiles.begin(), tiles.end(), tileIs(*iter) );
        if(pos != tiles.end ()) {
            tiles.erase (pos);
        }
        ++iter;
    }

    for(TileItem &tileItem : tiles) {
        GlBufferBucket tile(tileItem.x, tileItem.y, tileItem.z, tileItem.env,
                            tileItem.crossExtent);
        if(m_cancelPrepare)
            return;

        if(! (isEqual(renderExtent.MaxX, m_renderExtent.MaxX) &&
              isEqual(renderExtent.MaxY, m_renderExtent.MaxY) &&
              isEqual(renderExtent.MinX, m_renderExtent.MinX) &&
              isEqual(renderExtent.MinY, m_renderExtent.MinY))) {
            // if extent changed - refresh tiles
            return fillRenderBuffers();
        }

        GeometryPtr spatialFilter (envelopeToGeometry(tile.extent (),
                                    featureDataset->getSpatialReference ()));
        ResultSetPtr resSet = featureDataset->getGeometries (tile.zoom (),
                                                             spatialFilter);
        while ((feature = resSet->GetNextFeature ()) != nullptr) {
            if(m_cancelPrepare) {
                return;
            }

            fidExists = false;
            for(GlBufferBucket& tile1 : m_tiles) {
                if( tile1.zoom () == m_renderZoom &&
                    tile1.crossExtent () == tile.crossExtent () &&
                    tile1.hasFid (feature->GetFID ()) ) {
                    fidExists = true;
                    break;
                }
            }

            if(fidExists)
                continue;

            geom = feature->GetGeometryRef ();
            if(nullptr == geom)
                continue;

            // TODO: draw filled polygones with border
            tile.fill(feature->GetFID (), geom, m_renderLevel);
        }

        tile.setFilled(true);

        m_tiles.push_back (tile);

        counter++;
        m_complete = counter / tiles.size ();
        if(m_mapView) {
            m_mapView->notify();
        }
    }


    // free memory remove not visible tiles

    iter = m_tiles.begin ();
    OGREnvelope testExt = resizeEnvelope (m_renderExtent, 2);
    while (iter != m_tiles.end()) {
        const GlBufferBucket &currentTile = *iter;

        if (currentTile.crossExtent () == 0 &&
                (currentTile.zoom () != m_renderZoom ||
                !currentTile.intersects (testExt))) {
            CPLLockHolder tilesHolder(m_hTilesLock);
            iter = m_tiles.erase(iter);
        }
        else {
            ++iter;
        }
    }

    m_complete = 1;
    if(m_mapView) {
        m_mapView->notify();
    }
}

void ngs::FeatureRenderLayer::clearTiles()
{
    CPLLockHolder tilesHolder(m_hTilesLock);
    m_tiles.clear ();
}

void ngs::FeatureRenderLayer::drawTiles()
{
    // load program if already not, set matrix and fill color in prepare
    m_style->prepare (m_mapView->getSceneMatrix ());
    CPLLockHolder tilesHolder(m_hTilesLock);
    for(GlBufferBucket& tile : m_tiles) {
        tile.draw (*m_style.get ());
    }
}

void FeatureRenderLayer::refreshTiles()
{
    vector<TileItem> tiles = getTilesForExtent(m_renderExtent, m_renderZoom,
                                            false, m_mapView->getXAxisLooped ());

    // remove exist items in m_tiles from tiles
    // remove non exist items in tiles from m_tiles
    CPLLockHolder tilesHolder(m_hTilesLock);
    auto iter = m_tiles.begin ();
    while (iter != m_tiles.end())
    {
        if(tiles.empty ())
            break;
        auto pos = find_if(tiles.begin(), tiles.end(), tileIs(*iter) );
        if(pos == tiles.end ())
        {
            // erase returns the new iterator
            iter = m_tiles.erase(iter);
        }
        else
        {
            tiles.erase (pos);
            ++iter;
        }
    }

    for(const TileItem& tile : tiles) {
        m_tiles.push_back (GlBufferBucket(tile.x, tile.y, tile.z, tile.env,
                                          tile.crossExtent));
    }
}


int FeatureRenderLayer::load(const JSONObject &store,
                                  DatasetContainerPtr dataStore,
                                  const CPLString &mapPath)
{
    int nRet = Layer::load (store, dataStore, mapPath);
    if(nRet != ngsErrorCodes::EC_SUCCESS)
        return nRet;
    initStyle();
    return nRet;
}
