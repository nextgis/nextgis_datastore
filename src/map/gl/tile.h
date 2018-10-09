/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#ifndef NGSGLTILE_H
#define NGSGLTILE_H

#include "buffer.h"
#include "image.h"
#include "ds/geometry.h"
#include "map/glm/mat4x4.hpp"

namespace ngs {

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
constexpr int GLTILE_SIZE = 256;
#elif __ANDROID__
constexpr int GLTILE_SIZE = 256;
#else
constexpr int GLTILE_SIZE = 512;
#endif

class GlTile : public GlObject
{
public:
    explicit GlTile(unsigned short tileSize, const TileItem &tileItem);
    explicit GlTile(const GlTile other, bool initNew);
    virtual ~GlTile() override = default;

    glm::mat4 getSceneMatrix() const { return m_sceneMatrix; }
    glm::mat4 getInvViewMatrix() const { return m_invViewMatrix; }
    GlImage *getImageRef() const { return const_cast<GlImage*>(&m_image); }
    const GlBuffer &getBuffer() const { return m_tile; }
    const Tile &getTile() const { return  m_tileItem.tile; }
    const Envelope &getExtent() const { return m_tileItem.env; }
    bool filled() const { return m_filled; }
    void setFilled(bool filled = true) { m_filled = filled; }
    size_t getSizeInPixels() const {
        return size_t(m_originalTileSize);///*m_image.getWidth()*/ * 256.0 / GLTILE_SIZE);
    }
    unsigned short tileSize() const { return  m_tileSize; }

    static void prepareContext();

    // GlObject interface
public:
    virtual void bind() override;
    virtual void rebind() const override;
    virtual void destroy() override;

protected:
    void init(unsigned short tileSize, const Envelope& tileItemEnv,
              char crossExtent);

protected:
    TileItem m_tileItem;
    GlImage m_image;
    GLuint m_id, m_did;
    GlBuffer m_tile;
    glm::mat4 m_sceneMatrix;
    glm::mat4 m_invViewMatrix;
    bool m_filled;
    unsigned short m_tileSize, m_originalTileSize;
    Envelope m_originalEnv;
};

typedef std::shared_ptr<GlTile> GlTilePtr;

} // namespace ngs

#endif // NGSGLTILE_H
