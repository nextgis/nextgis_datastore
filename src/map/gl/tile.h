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
#include "functions.h"
#include "image.h"
#include "ds/geometry.h"
#include "map/matrix.h"

namespace ngs {

class GlTile : public GlObject
{
public:
    GlTile(unsigned short tileSize, const TileItem& tileItem);
    virtual ~GlTile() = default;

    Matrix4 getSceneMatrix() const { return m_sceneMatrix; }
    Matrix4 getInvViewMatrix() const { return m_invViewMatrix; }
    GlImage* getImageRef() const { return const_cast<GlImage*>(&m_image); }
    const GlBuffer& getBuffer() const { return m_tile; }  
    const Tile &getTile() const { return  m_tileItem.tile; }
    const Envelope& getExtent() const { return m_tileItem.env; }
    bool filled() const { return m_filled; }
    void setFilled(bool filled = true) { m_filled = filled; }
    size_t getSizeInPixels() const { return m_image.getWidth()/* / 2*/; }

    // GlObject interface
public:
    virtual void bind() override;
    virtual void rebind() const override;
    virtual void destroy() override;

protected:
    TileItem m_tileItem;
    GlImage m_image;
    GLuint m_id;
    GlBuffer m_tile;
    Matrix4 m_sceneMatrix;
    Matrix4 m_invViewMatrix;
    bool m_filled;
};

typedef std::shared_ptr<GlTile> GlTilePtr;

} // namespace ngs

#endif // NGSGLTILE_H
