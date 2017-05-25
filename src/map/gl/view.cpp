/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*  Author: NikitaFeodonit, nfeodonit@yandex.com
*******************************************************************************
*  Copyright (C) 2016-2017 NextGIS, <info@nextgis.com>
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "view.h"

#include "layer.h"
#include "style.h"
#include "util/error.h"

namespace ngs {

constexpr double EXTENT_EXTRA_BUFFER = 1.5;
constexpr int GLTILE_SIZE = 256;// 512;//256;

GlView::GlView() : MapView()
{
    initView();
}

GlView::GlView(const CPLString &name, const CPLString &description,
               unsigned short epsg, const Envelope &bounds) :
    MapView(name, description, epsg, bounds)
{
    initView();
}

void GlView::clearTiles()
{
    auto it = m_tiles.begin();
    while(it != m_tiles.end()) {
        (*it)->destroy();
        it = m_tiles.erase(it);
    }
}

void GlView::setBackgroundColor(const ngsRGBA &color)
{
    MapView::setBackgroundColor(color);
    m_glBkColor.r = float(color.R) / 255;
    m_glBkColor.g = float(color.G) / 255;
    m_glBkColor.b = float(color.B) / 255;
    m_glBkColor.a = float(color.A) / 255;
}

// NOTE: Should be run on OpenGL current context
void GlView::clearBackground()
{
    ngsCheckGLError(glClearColor(m_glBkColor.r, m_glBkColor.g, m_glBkColor.b,
                                  m_glBkColor.a));
    ngsCheckGLError(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

LayerPtr GlView::createLayer(const char *name, Layer::Type type)
{
    switch (type) {
    case Layer::Type::Vector:
        return LayerPtr(new GlFeatureLayer(name));
    case Layer::Type::Raster:
        return LayerPtr(new GlRasterLayer(name));
    default:
        return MapView::createLayer(name, type);
    }

}

void GlView::layerDataFillJobThreadFunc(ThreadData *threadData)
{
    LayerFillData* data = static_cast<LayerFillData*>(threadData);
    GlRenderLayer* renderLayer = ngsDynamicCast(GlRenderLayer, data->m_layer);
    if(nullptr != renderLayer) {
        renderLayer->fill(data->m_tile);
    }
}

#ifdef NGS_GL_DEBUG
bool GlView::draw(ngsDrawState /*state*/, const Progress &/*progress*/)
#else
bool GlView::draw(ngsDrawState state, const Progress &progress)
#endif // NGS_GL_DEBUG
{
    // Prepare
    prepareContext();


#ifdef NGS_GL_DEBUG
//    setRotate(ngsDirection::DIR_Z, M_PI/6);

    clearBackground();
    // Test drawing without layers
    testDrawPolygons(getSceneMatrix(), getInvViewMatrix());
//    testDrawPoints();
//    testDrawLines();
//    testDrawIcons();
    testDrawTiledPolygons();
    return true;
#else

    if(m_layers.empty()) {
        clearBackground();
        progress.onProgress(ngsCode::COD_FINISHED, 1.0,
                            _("No layers. Nothing to render."));
        return true;
    }

    switch (state) {
    case DS_REDRAW:
        clearTiles();
    [[clang::fallthrough]]; case DS_REFILL:
        for(GlTilePtr& tile : m_tiles) {
            tile->setFilled(false);
        }
    [[clang::fallthrough]]; case DS_NORMAL:
        // Get tiles for extent and mark to delete out of bounds tiles
        updateTilesList();
        // Start load layers data for tiles
        m_threadPool.clearThreadData();
        for(const GlTilePtr& tile : m_tiles) {
            if(tile->filled())
                continue;
            for(const LayerPtr& layer : m_layers) {
                m_threadPool.addThreadData(new LayerFillData(tile, layer, true));
            }
        }
    [[clang::fallthrough]]; case DS_PRESERVED:
        bool result = drawTiles(progress);
        // Free unnecessary Gl objects as this call is in Gl context
        freeResources();
        return result;
    }
#endif // NGS_GL_DEBUG
}

void GlView::updateTilesList()
{
    // Get tiles for current extent
    Envelope ext = getExtent();
    ext.resize(EXTENT_EXTRA_BUFFER);
    warningMessage("Zoom is: %d", getZoom());
    std::vector<TileItem> tileItems = getTilesForExtent(ext, getZoom(),
                                                    getYAxisInverted(),
                                                    getXAxisLooped());
    // Remove out of extent Gl tiles
    auto tileIt = m_tiles.begin();
    while(tileIt != m_tiles.end()) {
        bool markToDelete = true;        
        auto itemIt = tileItems.begin();
        while(itemIt != tileItems.end()) {
            if((*tileIt)->getTile() == (*itemIt).tile) {
                // Item already present in m_tiles
                tileItems.erase(itemIt);
                markToDelete = false;
                break;
            }
            ++itemIt;
        }

        if(markToDelete) {
            m_oldTiles.push_back(*tileIt);
            tileIt = m_tiles.erase(tileIt);
        }
        else {
            ++tileIt;
        }
    }

    // Add new Gl tiles
    for(const TileItem& tileItem : tileItems) {
        m_tiles.push_back(GlTilePtr(new GlTile(GLTILE_SIZE, tileItem)));
    }
}

void GlView::freeResources()
{
    auto freeResorceIt = m_freeResources.begin();
    while(freeResorceIt != m_freeResources.end()) {
        (*freeResorceIt)->destroy();
        freeResorceIt = m_freeResources.erase(freeResorceIt);
    }
}

bool GlView::drawTiles(const Progress &progress)
{
    drawOldTiles();

    // Preserve current viewport
    GLint viewport[4];
    glGetIntegerv( GL_VIEWPORT, viewport );

    double done = 0.0;
    for(const GlTilePtr& tile : m_tiles) {
        bool drawTile = true;
        if(tile->filled()) {
            done += m_layers.size();
        }
        else {
            if(tile->bound()) {
                tile->rebind();
            }
            else {
                tile->bind();
            }

            glViewport(0, 0, GLTILE_SIZE, GLTILE_SIZE);
            clearBackground();
            unsigned char filled = 0;
            for(const LayerPtr &layer : m_layers) {
                GlRenderLayer *renderLayer = ngsDynamicCast(GlRenderLayer,
                                                             layer);
                if(renderLayer && renderLayer->draw(tile)) {
                    filled++;
                }
            }

            if(filled == m_layers.size()) {
                // Free layer data
                for(const LayerPtr &layer : m_layers) {
                    GlRenderLayer *renderLayer = ngsDynamicCast(GlRenderLayer,
                                                                 layer);
                    if(renderLayer) {
                        renderLayer->free(tile);
                    }
                }
                tile->setFilled();
                done++;
            }

            // Draw tile on view
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

            // Make the window the target
            ngsCheckGLError(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 1)); // 0 - back, 1 - front.

            if(filled == 0) {
                drawTile = false;
            }
        }

        if(drawTile) { // Don't draw tiles with only background
            m_fboDrawStyle.setImage(tile->getImageRef());
            tile->getBuffer().rebind();
            m_fboDrawStyle.prepare(getSceneMatrix(), getInvViewMatrix());
            m_fboDrawStyle.draw(tile->getBuffer());
        }
    }

    size_t totalDrawCalls = m_layers.size() * m_tiles.size();
    if(isEqual(done, totalDrawCalls)) {
        freeOldTiles();
        progress.onProgress(ngsCode::COD_FINISHED, 1.0,
                            _("Map render finished."));
    }
    else {
        double complete = done / totalDrawCalls;
        progress.onProgress(ngsCode::COD_IN_PROCESS, complete,
                            _("Rendering ..."));
    }

    return true;
}

void GlView::drawOldTiles()
{
    for(const GlTilePtr& oldTile : m_oldTiles) {
        if(oldTile->filled()){
            m_fboDrawStyle.setImage(oldTile->getImageRef());
            oldTile->getBuffer().rebind();
            m_fboDrawStyle.prepare(getSceneMatrix(), getInvViewMatrix());
            m_fboDrawStyle.draw(oldTile->getBuffer());
        }
    }
}

void GlView::freeOldTiles()
{
    for(const GlTilePtr& oldTile : m_oldTiles) {
        freeResource(std::dynamic_pointer_cast<GlObject>(oldTile));
    }
    m_oldTiles.clear();
}

void GlView::initView()
{
    unsigned char numThreads = static_cast<unsigned char>(CPLGetNumCPUs());

    const char* numThreadsStr = CPLGetConfigOption("GDAL_NUM_THREADS", NULL);
    if(numThreadsStr) {
        if(!EQUAL(numThreadsStr, "ALL_CPUS")) {
            numThreads = static_cast<unsigned char>(atoi(numThreadsStr));
        }
    }

    if(numThreads < 1)
        numThreads = 1;

    m_threadPool.init(numThreads, layerDataFillJobThreadFunc);
}

#ifdef NGS_GL_DEBUG
void GlView::testDrawPoints() const
{
    GlBuffer buffer1;
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(0);
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(1);
    buffer1.addVertex(4187591.86613f);
    buffer1.addVertex(7509961.73580f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(2);

//    GLfloat vVertices[] = { 0.0f,  0.0f, 0.0f,
//                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
//                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
//                          };

    GlBuffer buffer2;
    buffer2.addVertex(1000000.0f);
    buffer2.addVertex(-500000.0f);
    buffer2.addVertex(-0.5f);
    buffer2.addIndex(0);
    buffer2.addVertex(-2236992.0f);
    buffer2.addVertex(3972353.0f);
    buffer2.addVertex(0.5f);
    buffer2.addIndex(1);
    buffer2.addVertex(5187591.0f);
    buffer2.addVertex(4509961.0f);
    buffer2.addVertex(0.5f);
    buffer2.addIndex(2);

//    GLfloat vVertices2[] = { 1000000.0f,  -500000.0f, -0.5f,
//                            -2236992.0f, 3972353.0f, 0.5f,
//                             5187591.0f, 4509961.0f, 0.5f
//                           };


    SimplePointStyle style(SimplePointStyle::PT_CIRCLE);


    buffer2.bind();
    style.setColor({0, 0, 0, 255});
    style.setSize(27.5f);
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer2);
    style.setColor({255, 0, 0, 255});
    style.setSize(25.0f);
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer2);


    style.setColor({0, 0, 0, 255});
    style.setSize(18.5f);
    style.setType(SimplePointStyle::PT_TRIANGLE);
    buffer1.bind();
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer1);
    style.setColor({0, 0, 255, 255});
    style.setSize(16.0f);
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer1);

    buffer2.destroy();
    buffer1.destroy();
}

void GlView::testDrawPolygons(const Matrix4 &msMatrix, const Matrix4 &vsMatrix) const
{
    GlBuffer buffer1;
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(0);
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(1);
    buffer1.addVertex(4187591.86613f);
    buffer1.addVertex(7509961.73580f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(2);

//    GLfloat vVertices[] = { 0.0f,  0.0f, 0.0f,
//                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
//                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
//                          };

    GlBuffer buffer2;
    buffer2.addVertex(1000000.0f);
    buffer2.addVertex(-500000.0f);
    buffer2.addVertex(-0.5f);
    buffer2.addIndex(0);
    buffer2.addVertex(-2236992.0f);
    buffer2.addVertex(3972353.0f);
    buffer2.addVertex(0.5f);
    buffer2.addIndex(1);
    buffer2.addVertex(5187591.0f);
    buffer2.addVertex(4509961.0f);
    buffer2.addVertex(0.5f);
    buffer2.addIndex(2);

//    GLfloat vVertices2[] = { 1000000.0f,  -500000.0f, -0.5f,
//                            -2236992.0f, 3972353.0f, 0.5f,
//                             5187591.0f, 4509961.0f, 0.5f
//                           };

    SimpleFillStyle style;

    style.setColor({255, 0, 0, 255});
    buffer2.bind();
    style.prepare(msMatrix, vsMatrix);
    style.draw(buffer2);

    style.setColor({0, 0, 255, 255});
    buffer1.bind();
    style.prepare(msMatrix, vsMatrix);
    style.draw(buffer1);

    buffer2.destroy();
    buffer1.destroy();
}

void GlView::testDrawLines() const
{
    // Line cap and join http://archive.xaraxone.com/webxealot/workbook63/a-line-gallery-02.png
    OGRPoint pt1(0.0, 0.0);
    OGRPoint pt2(-8236992.95426, 4972353.09638);
    Normal normal = ngsGetNormals(pt1, pt2);

    GlBuffer buffer1;
    // 0
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(-normal.x);
    buffer1.addVertex(-normal.y);
    buffer1.addIndex(0);

    // 1
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(-normal.x);
    buffer1.addVertex(-normal.y);
    buffer1.addIndex(1);

    // 2
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(normal.x);
    buffer1.addVertex(normal.y);
    buffer1.addIndex(2);

    // 3
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(normal.x);
    buffer1.addVertex(normal.y);

    buffer1.addIndex(1);
    buffer1.addIndex(2);
    buffer1.addIndex(3);

    OGRPoint pt3(4187591.86613, 7509961.73580);
    normal = ngsGetNormals(pt2, pt3);

    // 4
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(-normal.x);
    buffer1.addVertex(-normal.y);
    buffer1.addIndex(4);

    // 5
    buffer1.addVertex(4187591.86613f);
    buffer1.addVertex(7509961.73580f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(-normal.x);
    buffer1.addVertex(-normal.y);
    buffer1.addIndex(5);

    // 6
    buffer1.addVertex(4187591.86613f);
    buffer1.addVertex(7509961.73580f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(normal.x);
    buffer1.addVertex(normal.y);
    buffer1.addIndex(6);

    // 7
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(normal.x);
    buffer1.addVertex(normal.y);

    buffer1.addIndex(4);
    buffer1.addIndex(6);
    buffer1.addIndex(7);

/*  // Test Round cap
    int parts = 6;
    float start = std::asinf(normal.y);
    if(normal.x < 0 && normal.y < 0)
        start = M_PI + -(start);
    else if(normal.x < 0 && normal.y > 0)
        start = M_PI_2 + start;
    else if(normal.x > 0 && normal.y < 0)
        start = M_PI + M_PI + start;

    float end = M_PI + start;
    float step = (end - start) / parts;
    float current = start;
    int index = 4;
    for(int i = 0 ; i < parts; ++i) {
        float x = std::cosf(current);
        float y = std::sinf(current);
        current += step;
        buffer1.addVertex(0.0f);
        buffer1.addVertex(0.0f);
        buffer1.addVertex(0.0f);
        buffer1.addVertex(x);
        buffer1.addVertex(y);

        x = std::cosf(current);
        y = std::sinf(current);
        buffer1.addVertex(0.0f);
        buffer1.addVertex(0.0f);
        buffer1.addVertex(0.0f);
        buffer1.addVertex(x);
        buffer1.addVertex(y);

        buffer1.addVertex(0.0f);
        buffer1.addVertex(0.0f);
        buffer1.addVertex(0.0f);
        buffer1.addVertex(0.0f);
        buffer1.addVertex(0.0f);
        buffer1.addIndex(index++);
        buffer1.addIndex(index++);
        buffer1.addIndex(index++);
    }
*/

/*    // Test Square cap
    // 4
    float scX1 = -(normal.y + normal.x);
    float scY1 = -(normal.y - normal.x);
    float scX2 = normal.x - normal.y;
    float scY2 = normal.x + normal.y;

    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(scX1);
    buffer1.addVertex(scY1);

    // 5
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(scX2);
    buffer1.addVertex(scY2);

    buffer1.addIndex(0);
    buffer1.addIndex(2);
    buffer1.addIndex(5);

    buffer1.addIndex(5);
    buffer1.addIndex(4);
    buffer1.addIndex(0);
*/
    // No test Butt cap

    // Add seond segment

    // Test Miter join

    // Test Round joim

    // Test Beveled join

    //    buffer1.addVertex(4187591.86613f);
    //    buffer1.addVertex(7509961.73580f);
    //    buffer1.addVertex(0.0f);
    //    buffer1.addIndex(2);

    //    GLfloat vVertices[] = { 0.0f,  0.0f, 0.0f,
    //                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
    //                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
    //                          };

    //    GlBuffer buffer2;
    //    buffer2.addVertex(1000000.0f);
    //    buffer2.addVertex(-500000.0f);
    //    buffer2.addVertex(-0.5f);
    //    buffer2.addIndex(0);
    //    buffer2.addVertex(-2236992.0f);
    //    buffer2.addVertex(3972353.0f);
    //    buffer2.addVertex(0.5f);
    //    buffer2.addIndex(1);
    //    buffer2.addVertex(5187591.0f);
    //    buffer2.addVertex(4509961.0f);
    //    buffer2.addVertex(0.5f);
    //    buffer2.addIndex(2);

    //    GLfloat vVertices2[] = { 1000000.0f,  -500000.0f, -0.5f,
    //                            -2236992.0f, 3972353.0f, 0.5f,
    //                             5187591.0f, 4509961.0f, 0.5f
    //                           };


    SimpleLineStyle style;
    style.setLineWidth(25.0f);
    style.setColor({255, 0, 0, 255});
//    buffer2.bind();
//    style.prepare(getSceneMatrix(), getInvViewMatrix());
//    style.draw(buffer2);

//    style.setColor({0, 0, 255, 255});
    buffer1.bind();
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer1);

//    buffer2.destroy();
    buffer1.destroy();
}

void GlView::testDrawIcons() const
{
    GLubyte* chessData = static_cast<GLubyte*>(CPLMalloc(3 * 3 *
                                                         sizeof(GLubyte) * 4));
// 0
    chessData[0] = 255;
    chessData[1] = 255;
    chessData[2] = 255;
    chessData[3] = 255;
// 1
    chessData[4] = 0;
    chessData[5] = 0;
    chessData[6] = 0;
    chessData[7] = 50;
// 2
    chessData[8] = 255;
    chessData[9] = 255;
    chessData[10] = 255;
    chessData[11] = 255;
// 3
    chessData[12] = 0;
    chessData[13] = 0;
    chessData[14] = 0;
    chessData[15] = 50;
// 4
    chessData[16] = 255;
    chessData[17] = 255;
    chessData[18] = 255;
    chessData[19] = 255;
// 5
    chessData[20] = 0;
    chessData[21] = 0;
    chessData[22] = 0;
    chessData[23] = 50;
// 6
    chessData[24] = 255;
    chessData[25] = 255;
    chessData[26] = 255;
    chessData[27] = 255;
// 7
    chessData[28] = 0;
    chessData[29] = 0;
    chessData[30] = 0;
    chessData[31] = 50;
// 8
    chessData[32] = 255;
    chessData[33] = 255;
    chessData[34] = 255;
    chessData[35] = 255;

    GlImage image;
    image.setImage(chessData, 3, 3);

    GlBuffer buffer1;
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(0);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(5000000.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(1.0f);
    buffer1.addIndex(1);
    buffer1.addVertex(5000000.0f);
    buffer1.addVertex(5000000.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(1.0f);
    buffer1.addVertex(1.0f);
    buffer1.addIndex(2);
    buffer1.addVertex(5000000.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(1.0f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(0);
    buffer1.addIndex(2);
    buffer1.addIndex(3);


//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(0.0f);
//    buffer1.addIndex(0);
//    buffer1.addVertex(-8236992.95426f);
//    buffer1.addVertex(4972353.09638f);
//    buffer1.addVertex(0.0f);
//    buffer1.addIndex(1);
//    buffer1.addVertex(4187591.86613f);
//    buffer1.addVertex(7509961.73580f);
//    buffer1.addVertex(0.0f);
//    buffer1.addIndex(2);

//    GLfloat vVertices[] = { 0.0f,  0.0f, 0.0f,
//                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
//                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
//                          };

//    GlBuffer buffer2;
//    buffer2.addVertex(1000000.0f);
//    buffer2.addVertex(-500000.0f);
//    buffer2.addVertex(-0.5f);
//    buffer2.addIndex(0);
//    buffer2.addVertex(-2236992.0f);
//    buffer2.addVertex(3972353.0f);
//    buffer2.addVertex(0.5f);
//    buffer2.addIndex(1);
//    buffer2.addVertex(5187591.0f);
//    buffer2.addVertex(4509961.0f);
//    buffer2.addVertex(0.5f);
//    buffer2.addIndex(2);

//    GLfloat vVertices2[] = { 1000000.0f,  -500000.0f, -0.5f,
//                            -2236992.0f, 3972353.0f, 0.5f,
//                             5187591.0f, 4509961.0f, 0.5f
//                           };


    SimpleImageStyle style;
    style.setImage(image);


//    buffer2.bind();
//    style.prepare(getSceneMatrix(), getInvViewMatrix());
//    style.draw(buffer2);

    buffer1.bind();
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer1);

//    buffer2.destroy();
    buffer1.destroy();
}

void GlView::testDrawTile(const TileItem &tile) const
{
    // FIXME: Don't use NPOT texture. Use standard tile size 256 or 512. Is MPAA needed in this case - I think yes as SPAA is workonly in doublesized images?

//        double beg = worldToDisplay(OGRRawPoint(tile.env.getMinX(), tile.env.getMinY())).x;
//        double end = worldToDisplay(OGRRawPoint(tile.env.getMaxX(), tile.env.getMinY())).x;

//        double pixels = end - beg;

//    int s = pixels + pixels + pixels + pixels;
    GlTile glTile(GLTILE_SIZE, tile);
    glTile.bind();

    ngsCheckGLError(glClearColor(1.0f, 0.0f, 1.0f, 1.0f));
    ngsCheckGLError(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    GLint viewport[4];
    glGetIntegerv( GL_VIEWPORT, viewport );

    glViewport(0, 0, GLTILE_SIZE, GLTILE_SIZE);

    // Draw in first tile
    testDrawPolygons(glTile.getSceneMatrix(), glTile.getInvViewMatrix());

    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    // Draw tile in view
    // Make the window the target
    ngsCheckGLError(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 1)); // 0 - back, 1 - front.

    SimpleImageStyle style1; //SimpleImageStyle TileFBO draw

    style1.setImage(glTile.getImage());
    glTile.getBuffer().rebind();
    style1.prepare(getSceneMatrix(), getInvViewMatrix());
    style1.draw(glTile.getBuffer());

    glTile.destroy();
}

void GlView::testDrawTiledPolygons() const
{
    CPLDebug("GlView", "Zoom is %d", getZoom());
    std::vector<TileItem> tiles = getTilesForExtent(getExtent(), getZoom(),
                                                    getYAxisInverted(), false);

    testDrawTile(tiles[3]);
    testDrawTile(tiles[2]);
    testDrawTile(tiles[1]);
    testDrawTile(tiles[0]);
}


void GlView::testDrawTileContent(const GlTilePtr& tile)
{
    Envelope ext = tile->getExtent();
    ext.resize(0.9);

    std::array<OGRPoint, 6> points;
    points[0] = OGRPoint(ext.getMinX(), ext.getMinY());
    points[1] = OGRPoint(ext.getMinX(), ext.getMaxY());
    points[2] = OGRPoint(ext.getMaxX(), ext.getMaxY());
    points[3] = OGRPoint(ext.getMaxX(), ext.getMinY());
    points[4] = OGRPoint(ext.getMinX(), ext.getMinY());
    points[5] = OGRPoint(ext.getMaxX(), ext.getMaxY());
    for(size_t i = 0; i < points.size() - 1; ++i) {
        Normal normal = ngsGetNormals(points[i], points[i + 1]);

        GlBuffer buffer1;
        // 0
        buffer1.addVertex(points[i].getX());
        buffer1.addVertex(points[i].getY());
        buffer1.addVertex(0.0f);
        buffer1.addVertex(-normal.x);
        buffer1.addVertex(-normal.y);
        buffer1.addIndex(0);

        // 1
        buffer1.addVertex(points[i + 1].getX());
        buffer1.addVertex(points[i + 1].getY());
        buffer1.addVertex(0.0f);
        buffer1.addVertex(-normal.x);
        buffer1.addVertex(-normal.y);
        buffer1.addIndex(1);

        // 2
        buffer1.addVertex(points[i].getX());
        buffer1.addVertex(points[i].getY());
        buffer1.addVertex(0.0f);
        buffer1.addVertex(normal.x);
        buffer1.addVertex(normal.y);
        buffer1.addIndex(2);

        // 3
        buffer1.addVertex(points[i + 1].getX());
        buffer1.addVertex(points[i + 1].getY());
        buffer1.addVertex(0.0f);
        buffer1.addVertex(normal.x);
        buffer1.addVertex(normal.y);

        buffer1.addIndex(1);
        buffer1.addIndex(2);
        buffer1.addIndex(3);

        SimpleLineStyle style;
        style.setLineWidth(14.0f);
        style.setColor({0, 0, 255, 255});
        buffer1.bind();
        style.prepare(tile->getSceneMatrix(), tile->getInvViewMatrix());
        style.draw(buffer1);

        buffer1.destroy();
        style.destroy();
    }
}

#endif // NGS_GL_DEBUG

} // namespace ngs
