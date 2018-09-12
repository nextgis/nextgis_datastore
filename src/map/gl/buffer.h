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
#ifndef NGSGLBUFFER_H
#define NGSGLBUFFER_H

#include "functions.h"

#include <array>
#include <vector>

namespace ngs {

constexpr GLsizei GL_BUFFERS_COUNT = 2;

class GlBuffer : public GlObject
{
public:
    enum BufferType {
        BF_PT,
        BF_LINE,
        BF_FILL,
        BF_TEX
    };
public:
    explicit GlBuffer(enum BufferType type = BF_TEX);
    virtual ~GlBuffer() override;

    bool canStoreVertices(size_t amount, bool withNormals = false) const;
    GLuint id(bool vertices) const;
    GLsizei indexSize() const {
        return static_cast<GLsizei>(m_indices.size());
    }
    size_t vertexSize() const {
        return m_vertices.size();
    }

    void addVertex(float value) { m_vertices.push_back(value); }
    void addIndex(unsigned short value) { m_indices.push_back(value); }

    enum BufferType type() const { return m_type; }
    static size_t maxIndices();
    static size_t maxVertices();

    // GlObject interface
public:
    virtual void bind() override;
    virtual void rebind() const override;
    virtual void destroy() override;


private:
    std::vector<GLfloat> m_vertices;
    std::vector<GLushort> m_indices;
    std::array<GLuint, GL_BUFFERS_COUNT> m_bufferIds;
    enum BufferType m_type;
};

using GlBufferPtr = std::shared_ptr<GlBuffer>;

} // namespace ngs

#endif // NGSGLBUFFER_H
