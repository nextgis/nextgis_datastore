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
#ifndef RENDERLAYER_H
#define RENDERLAYER_H

#include "mapview.h"
#include "style.h"

namespace ngs {

void FillGLBufferThread(void * layer);

/**
 * @brief The RenderLayer class Base for renderable map layers
 */
class RenderLayer : public Layer
{
    friend class MapView;
    friend void FillGLBufferThread(void * layer);
public:
    RenderLayer();
    RenderLayer(const CPLString& name, DatasetPtr dataset);
    virtual ~RenderLayer();
    void prepareFillThread(OGREnvelope extent, double zoom, float level);
    void cancelFillThread();

    virtual void draw(enum ngsDrawState state, OGREnvelope extent, double zoom,
                        float level);
    virtual float getComplete() const;
protected:
    virtual void clearTiles() = 0;
    virtual void drawTiles() = 0;
    virtual void fillRenderBuffers() = 0;
    virtual void finishFillThread();

protected:
    bool m_cancelPrepare;
    CPLJoinableThread* m_hPrepareThread;
    OGREnvelope m_renderExtent;
    unsigned char m_renderZoom;
    float m_renderLevel;
    float m_complete;
    MapView* m_mapView;
    CPLLock *m_hThreadLock;
    StyleUPtr m_style;
};

class FeatureRenderLayer : public RenderLayer
{
public:
    FeatureRenderLayer();
    FeatureRenderLayer(const CPLString& name, DatasetPtr dataset);
    virtual ~FeatureRenderLayer();

protected:
    void fillRenderBuffers(OGRGeometry *geom);
    void initStyle();

    // RenderLayer interface
protected:
    virtual void fillRenderBuffers() override;
    virtual void clearTiles() override;
    virtual void drawTiles() override;
    void refreshTiles();

    // Layer interface
public:
    virtual int load(const JSONObject &store, DatasetContainerPtr dataStore,
                     const CPLString &mapPath) override;
protected:
    CPLLock *m_hTilesLock;
    vector<GlBufferBucket> m_tiles;
};

}
#endif // RENDERLAYER_H
