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

namespace ngs {

void FillGLBufferThread(void * layer);

typedef struct _buffer {
    vector<GLfloat> m_vertices;
    vector<GLushort> m_indices;
} ngsVertexBuffer;

typedef unique_ptr<ngsVertexBuffer> ngsVertexBufferPtr;

/**
 * @brief The RenderLayer class Base for renderable map layers
 */
class RenderLayer : public Layer
{
    friend void FillGLBufferThread(void * layer);
public:
    RenderLayer();
    RenderLayer(const CPLString& name, DatasetPtr dataset);
    virtual ~RenderLayer();
    void prepareRender(OGREnvelope extent, double zoom, float level);
    void cancelPrepareRender();
    /**
     * @brief render Render data to GL scene. Can be run only from GL rendering
     * thread
     * @param glView GL view pointer
     * @return draw percent
     */
    //virtual double render(const GlView* glView) = 0;
protected:
    virtual void fillRenderBuffers() = 0;
    char getCloseOvr();

protected:
    bool m_cancelPrepare;
    CPLJoinableThread* m_hPrepareThread;
    double m_renderPercent;
    OGREnvelope m_renderExtent;
    double m_renderZoom;
    float m_renderLevel;

    // TODO: Style m_style;
};

class FeatureRenderLayer : public RenderLayer
{
public:
    FeatureRenderLayer();
    FeatureRenderLayer(const CPLString& name, DatasetPtr dataset);
    virtual ~FeatureRenderLayer();

protected:
    void fillRenderBuffers(OGRGeometry *geom);

    // RenderLayer interface
protected:
    virtual void fillRenderBuffers() override;

    // RenderLayer interface
public:
    //virtual double render(const GlView *glView) override;

protected:
    vector<ngsVertexBufferPtr> m_pointBucket;
    vector<ngsVertexBufferPtr> m_polygonBucket;
    ngsVertexBufferPtr m_currentPointBuffer;
    ngsVertexBufferPtr m_currentPolygonBuffer;
    bool m_bucketFilled;
    CPLLock *m_hBucketLock, *m_hCurrentBufferLock;
};

}
#endif // RENDERLAYER_H
