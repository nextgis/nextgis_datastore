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

#include <cstring>

#include "cpl_conv.h"

namespace ngs {

constexpr unsigned int DEFAULT_BUFFER_SIZE = 1024;

Buffer::Buffer() :
    m_size(0),
    m_mallocSize(DEFAULT_BUFFER_SIZE),
    m_data(static_cast<GByte*>(CPLMalloc(DEFAULT_BUFFER_SIZE))),
    m_currentPos(0),
    m_own(true)
{

}

Buffer::Buffer(GByte* data, int size, bool own) :
    m_size(size),
    m_mallocSize(size),
    m_data(data),
    m_currentPos(0),
    m_own(own)
{

}

Buffer::~Buffer()
{
    if(m_own) {
        CPLFree(m_data);
    }
}

Buffer &Buffer::put(GUInt32 val)
{
    size_t size = sizeof(GUInt32);
    if(static_cast<size_t>(m_mallocSize) < m_currentPos + size) {
        m_mallocSize += DEFAULT_BUFFER_SIZE;
        m_data = static_cast<GByte*>(CPLRealloc(m_data, static_cast<size_t>(m_mallocSize)));
    }

    std::memcpy(m_data + m_currentPos, &val, size);
    m_currentPos += size;
    m_size += size;

    return *this;
}

Buffer &Buffer::put(float val)
{
    size_t size = sizeof(float);
    if(static_cast<size_t>(m_mallocSize) < m_currentPos + size) {
        m_mallocSize += DEFAULT_BUFFER_SIZE;
        m_data = static_cast<GByte*>(CPLRealloc(m_data, static_cast<size_t>(m_mallocSize)));
    }

    std::memcpy(m_data + m_currentPos, &val, size);
    m_currentPos += size;
    m_size += size;

    return *this;
}

Buffer &Buffer::put(GByte val)
{
    size_t size = sizeof(GByte);
    if(static_cast<size_t>(m_mallocSize) < m_currentPos + size) {
        m_mallocSize += DEFAULT_BUFFER_SIZE;
        m_data = static_cast<GByte*>(CPLRealloc(m_data, static_cast<size_t>(m_mallocSize)));
    }

    std::memcpy(m_data + m_currentPos, &val, size);
    m_currentPos += size;
    m_size += size;

    return *this;
}

Buffer &Buffer::put(GUInt16 val)
{
    size_t size = sizeof(GUInt16);
    if(static_cast<size_t>(m_mallocSize) < m_currentPos + size) {
        m_mallocSize += DEFAULT_BUFFER_SIZE;
        m_data = static_cast<GByte*>(CPLRealloc(m_data, static_cast<size_t>(m_mallocSize)));
    }

    std::memcpy(m_data + m_currentPos, &val, size);
    m_currentPos += size;
    m_size += size;

    return *this;
}

Buffer &Buffer::put(GUIntBig val)
{
    size_t size = sizeof(GUIntBig);
    if(static_cast<size_t>(m_mallocSize) < m_currentPos + size) {
        m_mallocSize += DEFAULT_BUFFER_SIZE;
        m_data = static_cast<GByte*>(CPLRealloc(m_data, static_cast<size_t>(m_mallocSize)));
    }

    std::memcpy(m_data + m_currentPos, &val, size);
    m_currentPos += size;
    m_size += size;

    return *this;
}

Buffer &Buffer::put(GIntBig val)
{
    size_t size = sizeof(GIntBig);
    if(static_cast<size_t>(m_mallocSize) < m_currentPos + size) {
        m_mallocSize += DEFAULT_BUFFER_SIZE;
        m_data = static_cast<GByte*>(CPLRealloc(m_data, static_cast<size_t>(m_mallocSize)));
    }

    std::memcpy(m_data + m_currentPos, &val, size);
    m_currentPos += size;
    m_size += size;

    return *this;
}

GUInt32 Buffer::getULong()
{
    GUInt32 val = 0;
    size_t size = sizeof(GUInt32);
    if(m_currentPos + size > static_cast<size_t>(m_size))
        return val;
    std::memcpy(&val, m_data + m_currentPos, size);
    m_currentPos += size;
    return val;
}

float Buffer::getFloat()
{
    float val = 0.0;
    size_t size = sizeof(float);
    if(m_currentPos + size > static_cast<size_t>(m_size))
        return val;
    std::memcpy(&val, m_data + m_currentPos, size);
    m_currentPos += size;
    return val;
}

GByte Buffer::getByte()
{
    GByte val = 0;
    size_t size = sizeof(GByte);
    if(m_currentPos + size > static_cast<size_t>(m_size))
        return val;
    std::memcpy(&val, m_data + m_currentPos, size);
    m_currentPos += size;
    return val;
}

GUInt16 Buffer::getUShort()
{
    GUInt16 val = 0;
    size_t size = sizeof(GUInt16);
    if(m_currentPos + size > static_cast<size_t>(m_size))
        return val;
    std::memcpy(&val, m_data + m_currentPos, size);
    m_currentPos += size;
    return val;
}

GUIntBig Buffer::getUBig()
{
    GUIntBig val = 0;
    size_t size = sizeof(GUIntBig);
    if(m_currentPos + size > static_cast<size_t>(m_size))
        return val;
    std::memcpy(&val, m_data + m_currentPos, size);
    m_currentPos += size;
    return val;
}

GIntBig Buffer::getBig()
{
    GIntBig val = 0;
    size_t size = sizeof(GIntBig);
    if(m_currentPos + size > static_cast<size_t>(m_size))
        return val;
    std::memcpy(&val, m_data + m_currentPos, size);
    m_currentPos += size;
    return val;
}

}
