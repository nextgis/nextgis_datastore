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
#ifndef NGSBUFFER_H
#define NGSBUFFER_H

#include "functions.h"

#include <array>
#include <vector>

namespace ngs {

constexpr GLsizei GL_BUFFERS_COUNT = 2;
constexpr unsigned char VERTEX_SIZE = 3;
// 5 = 3 for vertex + 2 for normal
constexpr unsigned char VERTEX_WITH_NORMAL_SIZE = 5;
//GL_UNSIGNED_BYTE, with a maximum value of 255.
//GL_UNSIGNED_SHORT, with a maximum value of 65,535
constexpr unsigned short MAX_VERTEX_BUFFER_SIZE = 65535;

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
    GlBuffer(enum BufferType type = BF_TEX);
    ~GlBuffer();

    bool canStoreVertices(size_t amount, bool withNormals = false) const {
        return (m_vertices.size()
                + amount * (withNormals ? VERTEX_WITH_NORMAL_SIZE :
                                          VERTEX_SIZE)) < MAX_VERTEX_BUFFER_SIZE;
    }
    GLuint id(bool vertices) const;
    GLsizei indexSize() const {
        return static_cast<GLsizei>(m_indices.size());
    }

    void addVertex(float value) { m_vertices.push_back(value); }
    void addIndex(unsigned short value) { m_indices.push_back(value); }

    enum BufferType type() const { return m_type; }

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

typedef std::shared_ptr<GlBuffer> GlBufferPtr;

} // namespace ngs

#endif // NGSBUFFER_H
