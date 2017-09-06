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
#include "buffer.h"

#include "cpl_conv.h"

namespace ngs {

constexpr GLuint GL_BUFFER_IVALID = 0;
constexpr unsigned short MAX_INDEX_BUFFER_SIZE = 65535;
constexpr unsigned char VERTEX_SIZE = 3;
// 5 = 3 for vertex + 2 for normal
constexpr unsigned char VERTEX_WITH_NORMAL_SIZE = 5;
//GL_UNSIGNED_BYTE, with a maximum value of 255.
//GL_UNSIGNED_SHORT, with a maximum value of 65,535
constexpr unsigned short MAX_VERTEX_BUFFER_SIZE = 65535 / 3;

GlBuffer::GlBuffer(BufferType type) : GlObject(),
    m_bufferIds{{GL_BUFFER_IVALID,GL_BUFFER_IVALID}},
    m_type(type)
{
    m_vertices.reserve(MAX_VERTEX_BUFFER_SIZE);
    m_indices.reserve(MAX_INDEX_BUFFER_SIZE);
}

GlBuffer::~GlBuffer()
{
    // NOTE: Delete buffer must be in GL context
}

bool GlBuffer::canStoreVertices(size_t amount, bool withNormals) const
{
    if(m_type == BF_TEX) {
        return (m_vertices.size() + amount * (
                    withNormals ? 7 : 5)) <  MAX_VERTEX_BUFFER_SIZE;
    }
    else {
        return (m_vertices.size() + amount * (
                    withNormals ? VERTEX_WITH_NORMAL_SIZE : VERTEX_SIZE)) <
                MAX_VERTEX_BUFFER_SIZE;
    }
}

void GlBuffer::destroy()
{
    if (m_bound) {
        ngsCheckGLError(glDeleteBuffers(GL_BUFFERS_COUNT, m_bufferIds.data()));
    }
}

GLuint GlBuffer::id(bool vertices) const
{
    if(vertices)
        return m_bufferIds[0];
    else
        return m_bufferIds[1];
}

size_t GlBuffer::maxIndices()
{
    return MAX_INDEX_BUFFER_SIZE;
}

size_t GlBuffer::maxVertices()
{
    return MAX_VERTEX_BUFFER_SIZE;
}

void GlBuffer::bind()
{
    if (m_bound || m_vertices.empty() || m_indices.empty())
        return;

    ngsCheckGLError(glGenBuffers(GL_BUFFERS_COUNT, m_bufferIds.data()));

    ngsCheckGLError(glBindBuffer(GL_ARRAY_BUFFER, id(true)));
    GLsizeiptr size = static_cast<GLsizeiptr>(sizeof(GLfloat) * m_vertices.size());
    ngsCheckGLError(glBufferData(GL_ARRAY_BUFFER, size, m_vertices.data(),
            GL_STATIC_DRAW));

    ngsCheckGLError(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id(false)));
    size = static_cast<GLsizeiptr>(sizeof(GLushort) * m_indices.size());
    ngsCheckGLError(glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, m_indices.data(),
            GL_STATIC_DRAW));
    m_bound = true;
}

void GlBuffer::rebind() const
{
    ngsCheckGLError(glBindBuffer(GL_ARRAY_BUFFER, id(true)));
    ngsCheckGLError(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id(false)));
}

} // namespace ngs

