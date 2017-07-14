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

#include <set>

#include "style.h"
#include "tile.h"
#include "map/layer.h"

namespace ngs {

/**
 * @brief The GlRenderLayer class Interface for renderable map layers
 */
class GlRenderLayer
{
public:
    GlRenderLayer();
    virtual ~GlRenderLayer();
    /**
     * @brief fill Fill arrays for Gl drawing. Executed from separate thread.
     * @param tile Tile to load data
     */
    virtual bool fill(GlTilePtr tile) = 0;
    /**
     * @brief free Free Gl objects. Run from Gl context.
     * @param tile Tile to free data
     */
    virtual void free(GlTilePtr tile);
    /**
     * @brief draw Draw data for specific tile. Run from Gl context.
     * @param tile Tile to draw
     * @return True of data for tile loaded, otherwise false.
     */
    virtual bool draw(GlTilePtr tile) = 0;

protected:
    std::map<Tile, GlObjectPtr> m_tiles;
    StylePtr m_style;
    CPLMutex *m_dataMutex;
};

/**
 * @brief The VectorGlObject class Storage for vector data
 */
class VectorGlObject : public GlObject
{
public:
    VectorGlObject();
    const std::vector<GlBufferPtr>& buffers() const { return m_buffers; }
    void addBuffer(GlBuffer* buffer) { m_buffers.push_back(GlBufferPtr(buffer)); }

    // GlObject interface
public:
    virtual void bind() override;
    virtual void rebind() const override;
    virtual void destroy() override;
private:
    std::vector<GlBufferPtr> m_buffers;
};

/**
 * @brief The GlFeatureLayer class OpenGl vector layer
 */
class GlFeatureLayer : public FeatureLayer, public GlRenderLayer
{
public:
    explicit GlFeatureLayer(const CPLString& name = DEFAULT_LAYER_NAME);
    virtual ~GlFeatureLayer() = default;

    // IGlRenderLayer interface
public:
    virtual bool fill(GlTilePtr tile) override;
    virtual bool draw(GlTilePtr tile) override;

    // Layer interface
public:
    virtual bool load(const CPLJSONObject &store, ObjectContainer *objectContainer) override;
    virtual CPLJSONObject save(const ObjectContainer *objectContainer) const override;

    // FeatureLayer interface
public:
    virtual void setFeatureClass(const FeatureClassPtr &featureClass) override;

protected:
    VectorGlObject *fillPoints(const VectorTile &tile);
    VectorGlObject *fillLines(const VectorTile &tile);
    VectorGlObject *fillPolygons(const VectorTile &tile);
    unsigned short addLineCap(const SimplePoint& point, const Normal& normal,
                              unsigned short index, GlBuffer* buffer);
    size_t lineCapVerticesCount() const;
    unsigned short addLineJoin(const SimplePoint& point, const Normal& prevNormal,
                               const Normal& normal, unsigned short index,
                               GlBuffer* buffer);
    size_t lineJoinVerticesCount() const;
protected:
    std::set<GIntBig> m_skipFIDs;
};

/**
 * @brief The RasterGlObject class Storage for image and extent data
 */
class RasterGlObject : public GlObject
{
public:
    explicit RasterGlObject(GlBuffer* tileExtentBuff, GlImage *image);
    GlImage* getImageRef() const { return m_image.get(); }
    GlBuffer* getBufferRef() const { return m_extentBuffer.get(); }

    // GlObject interface
public:
    virtual void bind() override;
    virtual void rebind() const override;
    virtual void destroy() override;

private:
    GlBufferPtr m_extentBuffer;
    GlImagePtr m_image;
};

/**
 * @brief The GlRasterLayer class OpenGL raster layer
 */
class GlRasterLayer : public RasterLayer, public GlRenderLayer
{
public:
    explicit GlRasterLayer(const CPLString& name = DEFAULT_LAYER_NAME);

    // IGlRenderLayer interface
public:
    virtual bool fill(GlTilePtr tile) override;
    virtual bool draw(GlTilePtr tile) override;

    // Layer interface
public:
    virtual bool load(const CPLJSONObject &store, ObjectContainer *objectContainer) override;
    virtual CPLJSONObject save(const ObjectContainer *objectContainer) const override;

    // RasterLayer interface
public:
    virtual void setRaster(const RasterPtr &raster) override;

private:
    unsigned char m_red, m_green, m_blue, m_alpha, m_transparancy;
    GDALDataType m_dataType;
};

} // namespace ngs

#endif // NGSGLMAPLAYER_H
