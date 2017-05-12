/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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
#ifndef NGSGLMAPLAYER_H
#define NGSGLMAPLAYER_H

#include "tile.h"
#include "map/layer.h"

namespace ngs {

/**
 * @brief The IRenderLayer class Interface for renderable map layers
 */
class IGlRenderLayer
{
public:
    virtual ~IGlRenderLayer() = default;
    /**
     * @brief fill Fill arrays for Gl drawing. Executed from separate thread.
     * @param tile Tile to load data
     */
    virtual void fill(GlTilePtr tile) = 0;
    /**
     * @brief free Free Gl objects. Run from Gl context.
     * @param tile Tile to free data
     */
    virtual void free(GlTilePtr tile) = 0;
    /**
     * @brief draw Draw data for specific tile. Run from Gl context.
     * @param tile Tile to draw
     * @return True of data for tile loaded, otherwise false.
     */
    virtual bool draw(GlTilePtr tile) = 0;
};

class GlFeatureLayer : public FeatureLayer, public IGlRenderLayer
{
public:
    GlFeatureLayer(const CPLString& name = DEFAULT_LAYER_NAME);

    // IGlRenderLayer interface
public:
    virtual void fill(GlTilePtr tile) override;
    virtual void free(GlTilePtr tile) override;
    virtual bool draw(GlTilePtr tile) override;
};

/*
void FillGLBufferThread(void * layer);
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
    virtual bool isComplete() const;
    virtual int getFeatureCount() const;

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
    bool m_isComplete;
    int m_featureCount;
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
    int getFidCount() const;

    // Layer interface
public:
    virtual int load(const JSONObject &store, DatasetContainerPtr dataStore,
                     const CPLString &mapPath) override;
protected:
    CPLLock *m_hTilesLock;
    vector<GlBufferBucketSharedPtr> m_tiles;
};

//https://www.khronos.org/files/opengles20-reference-card.pdf
//https://www.raywenderlich.com/4404/opengl-es-2-0-for-iphone-tutorial-part-2-textures
//https://library.vuforia.com/articles/Solution/How-To-Draw-a-2D-image-on-top-of-a-target-using-OpenGL-ES
//http://stackoverflow.com/questions/4036737/how-to-draw-a-texture-as-a-2d-background-in-opengl-es-2-0
//https://www.jayway.com/2010/12/30/opengl-es-tutorial-for-android-part-vi-textures/
//http://edndoc.esri.com/arcobjects/9.2/net/45c93c25-2ddb-4e1b-9bef-37c40b931597.htm
//http://androidblog.reindustries.com/a-real-opengl-es-2-0-2d-tutorial-part-2/
//http://obviam.net/index.php/texture-mapping-opengl-android-displaying-images-using-opengl-and-squares/

class RasterRenderLayer : public RenderLayer
{
public:
    RasterRenderLayer();
    RasterRenderLayer(const CPLString& name, DatasetPtr dataset);
    virtual ~RasterRenderLayer();
};
*/

} // namespace ngs

#endif // NGSGLMAPLAYER_H
