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

#ifndef NGSGLVIEW_H
#define NGSGLVIEW_H

#include "mapview.h"
#include "style.h"
#include "tile.h"
#include "util/threadpool.h"

#ifdef _DEBUG
//#define NGS_GL_DEBUG
#endif

namespace ngs {

class GlView : public MapView
{
public:
    GlView();
    GlView(const CPLString& name, const CPLString& description,
            unsigned short epsg, const Envelope &bounds);
    virtual ~GlView() = default;
    void freeResource(const GlObjectPtr& resource) {
        m_freeResources.push_back(resource);
    }

    // Run in GL context
protected:
    void clearTiles();
    void updateTilesList();
    void freeResources();
    bool drawTiles(const Progress &progress);
    void drawOldTiles();
    void freeOldTiles();
    void initView();

    // Map interface
public:
    virtual void setBackgroundColor(const ngsRGBA &color) override;

    // Map interface
protected:
    virtual LayerPtr createLayer(const char* name = DEFAULT_LAYER_NAME,
                                 Layer::Type type = Layer::Type::Invalid) override;

    // MapView interface
public:
    virtual bool draw(ngsDrawState state, const Progress &progress) override;

protected:
    virtual void clearBackground() override;

    // static
protected:
    static bool layerDataFillJobThreadFunc(ThreadData *threadData);

#ifdef NGS_GL_DEBUG
    // Test functions
private:
    void testDrawPoints() const;
    void testDrawPolygons(const Matrix4 &msMatrix, const Matrix4 &vsMatrix) const;
    void testDrawLines() const;
    void testDrawIcons() const;
    void testDrawTiledPolygons() const;
    void testDrawTile(const TileItem &tile) const;
    void testDrawTileContent(const GlTilePtr& tile);
#endif // NGS_GL_DEBUG

private:
    GlColor m_glBkColor;
    std::vector<GlObjectPtr> m_freeResources;
    std::vector<GlTilePtr> m_tiles, m_oldTiles;
    SimpleImageStyle m_fboDrawStyle;
    ThreadPool m_threadPool;

    class LayerFillData : public ThreadData {
    public:
        LayerFillData(GlTilePtr tile, LayerPtr layer, bool own) :
            ThreadData(own), m_tile(tile), m_layer(layer) {}
        GlTilePtr m_tile;
        LayerPtr m_layer;
    };
};

}  // namespace ngs

#endif  // NGSGLVIEW_H
