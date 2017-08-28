/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*  Author:  NikitaFeodonit, nfeodonit@yandex.com
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

#include <array>

#include "map/mapview.h"
#include "map/overlay.h"
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
    double pixelSize(int zoom);

    // Map interface
public:
    virtual void setBackgroundColor(const ngsRGBA &color) override;
    virtual bool close() override;

    // Map interface
protected:
    virtual LayerPtr createLayer(const char* name = DEFAULT_LAYER_NAME,
                                 Layer::Type type = Layer::Type::Invalid) override;
    virtual bool openInternal(const CPLJSONObject& root, MapFile * const mapFile) override;
    virtual bool saveInternal(CPLJSONObject &root, MapFile * const mapFile) override;

    // MapView interface
public:
    virtual bool draw(ngsDrawState state, const Progress &progress) override;
    virtual void invalidate(const Envelope& bounds) override;
    virtual bool setSelectionStyleName(enum ngsStyleType styleType,
                                       const char* name) override;
    virtual bool setSelectionStyle(enum ngsStyleType styleType,
                                   const CPLJSONObject& style) override {
        return m_selectionStyles[styleType]->load(style);
    }
    virtual const char* selectionStyleName(enum ngsStyleType styleType) const override {
        return m_selectionStyles[styleType]->name();
    }
    virtual CPLJSONObject selectionStyle(enum ngsStyleType styleType) const override {
        return m_selectionStyles[styleType]->save();
    }

    // MapView interface
protected:
    virtual void clearBackground() override;
    virtual void createOverlays() override;

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
    Envelope m_invalidRegion;
    SimpleImageStyle m_fboDrawStyle;
    std::array<StylePtr, 3> m_selectionStyles;
    ThreadPool m_threadPool;

    class LayerFillData : public ThreadData {
    public:
        LayerFillData(GlTilePtr tile, LayerPtr layer, bool own) :
            ThreadData(own), m_tile(tile), m_layer(layer) {}
        virtual ~LayerFillData() = default;
        GlTilePtr m_tile;
        LayerPtr m_layer;
    };
    class OverlayFillData : public ThreadData
    {
    public:
        OverlayFillData(OverlayPtr overlay, bool own)
                : ThreadData(own)
                , m_overlay(overlay)
        {
        }
        virtual ~OverlayFillData() = default;
        OverlayPtr m_overlay;
    };
};

}  // namespace ngs

#endif  // NGSGLVIEW_H
