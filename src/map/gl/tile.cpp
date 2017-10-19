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
#include "tile.h"

namespace ngs {

GlTile::GlTile(const GlTile other, bool /*initNew*/) : GlObject(),
    m_tileItem(other.m_tileItem),
    m_id(0),
    m_did(0),
    m_filled(false)
{
    m_originalTileSize = other.m_originalTileSize;
    m_originalEnv = other.m_originalEnv;

    init(m_originalTileSize, m_originalEnv, m_tileItem.tile.crossExtent);
}

GlTile::GlTile(unsigned short tileSize, const TileItem& tileItem) : GlObject(),
    m_tileItem(tileItem),
    m_id(0),
    m_did(0),
    m_filled(false)
{
    m_originalTileSize = tileSize;
    m_originalEnv = tileItem.env;

    init(tileSize, tileItem.env, tileItem.tile.crossExtent);
}

void GlTile::init(unsigned short tileSize, const Envelope& tileItemEnv,
                  char crossExtent)
{
    unsigned short newTileSize = static_cast<unsigned short>(
                std::ceil(double(tileSize) * TILE_RESIZE));
    float extraVal = (float(newTileSize - tileSize) / 2) / newTileSize;

    m_image.setImage(nullptr, newTileSize, newTileSize);
    m_image.setSmooth(true);

    Envelope env = tileItemEnv;
    env.move(crossExtent * DEFAULT_BOUNDS.width(), 0.0);
    m_tile.addVertex(static_cast<float>(env.minX()));
    m_tile.addVertex(static_cast<float>(env.minY()));
    m_tile.addVertex(0.0f);
    m_tile.addVertex(0.0f + extraVal);
    m_tile.addVertex(0.0f + extraVal);
    m_tile.addIndex(0);
    m_tile.addVertex(static_cast<float>(env.minX()));
    m_tile.addVertex(static_cast<float>(env.maxY()));
    m_tile.addVertex(0.0f);
    m_tile.addVertex(0.0f + extraVal);
    m_tile.addVertex(1.0f - extraVal);
    m_tile.addIndex(1);
    m_tile.addVertex(static_cast<float>(env.maxX()));
    m_tile.addVertex(static_cast<float>(env.maxY()));
    m_tile.addVertex(0.0f);
    m_tile.addVertex(1.0f - extraVal);
    m_tile.addVertex(1.0f - extraVal);
    m_tile.addIndex(2);
    m_tile.addVertex(static_cast<float>(env.maxX()));
    m_tile.addVertex(static_cast<float>(env.minY()));
    m_tile.addVertex(0.0f);
    m_tile.addVertex(1.0f - extraVal);
    m_tile.addVertex(0.0f + extraVal);
    m_tile.addIndex(0);
    m_tile.addIndex(2);
    m_tile.addIndex(3);

    env.resize(TILE_RESIZE);
    m_sceneMatrix.ortho(env.minX(), env.maxX(),
                        env.minY(), env.maxY(),
                        DEFAULT_BOUNDS.minX(), DEFAULT_BOUNDS.maxX());
    m_invViewMatrix.ortho(0, newTileSize, 0, newTileSize, -1.0, 1.0);

    m_tileSize = newTileSize;
    m_tileItem.env = env;
}

void GlTile::bind()
{
    if (m_bound)
        return;

    ngsCheckGLError(glGenFramebuffers(1, &m_id));
    // Set up the FBO with one texture attachment
    ngsCheckGLError(glBindFramebuffer(GL_FRAMEBUFFER, m_id));
    m_image.bind();
    ngsCheckGLError(glFramebufferTexture2D(GL_FRAMEBUFFER,
                                           GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D, m_image.id(), 0));

    ngsCheckGLError(glGenRenderbuffers(1, &m_did));
    ngsCheckGLError(glBindRenderbuffer(GL_RENDERBUFFER, m_did));
    ngsCheckGLError(glRenderbufferStorage(GL_RENDERBUFFER,
                                          GL_DEPTH_COMPONENT16,
                                          m_tileSize, m_tileSize));
    //Attach depth buffer to FBO
    ngsCheckGLError(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                              GL_DEPTH_ATTACHMENT,
                                              GL_RENDERBUFFER, m_did));

    ngsCheckGLError(glCheckFramebufferStatus(GL_FRAMEBUFFER));

    m_tile.bind();

    m_bound = true;
}

void GlTile::rebind() const
{
    ngsCheckGLError(glBindFramebuffer(GL_FRAMEBUFFER, m_id));
    m_image.rebind();
    ngsCheckGLError(glFramebufferTexture2D(GL_FRAMEBUFFER,
                                           GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D, m_image.id(), 0));
    ngsCheckGLError(glBindRenderbuffer(GL_RENDERBUFFER, m_did));
    ngsCheckGLError(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                              GL_DEPTH_ATTACHMENT,
                                              GL_RENDERBUFFER, m_did));
    m_tile.rebind();
}

void GlTile::destroy()
{
    if (m_bound) {
        ngsCheckGLError(glDeleteRenderbuffers(1, &m_did));
        ngsCheckGLError(glDeleteFramebuffers(1, &m_id));
    }
    m_image.destroy();
    m_tile.destroy();
}

void GlTile::prepareContext()
{
    #ifdef GL_PROGRAM_POINT_SIZE_EXT
        ngsCheckGLError(glEnable(GL_PROGRAM_POINT_SIZE_EXT));
    #endif

    #ifdef GL_MULTISAMPLE
        if( CPLTestBool("GL_MULTISAMPLE") )
            ngsCheckGLError(glEnable(GL_MULTISAMPLE));
    #endif

    // NOTE: In usual cases no need in depth test
    //    ngsCheckGLError(glDisable(GL_DEPTH_TEST));
        ngsCheckGLError(glEnable(GL_DEPTH_TEST));
    //    ngsCheckGLError(glDepthMask(GL_TRUE));
        ngsCheckGLError(glDepthFunc(GL_LEQUAL));
        ngsCheckGLError(glDepthRange(0.0, 1.0));
    #ifdef GL_POLYGON_SMOOTH_HINT
        ngsCheckGLError(glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST));
    #endif
    //    ngsCheckGLError(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
}

} // namespace ngs
