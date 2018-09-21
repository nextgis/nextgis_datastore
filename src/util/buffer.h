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

#include <memory>

#include "cpl_port.h"

namespace ngs {

class Buffer
{
public:
    Buffer();
    Buffer(GByte *data, int size, bool own = true);
    ~Buffer();

    // getters
    GByte *data() const { return m_data; }
    int size() const { return m_size; }

    Buffer &put(GUInt32 val);
    Buffer &put(float val);
    Buffer &put(GByte val);
    Buffer &put(GUInt16 val);
    Buffer &put(GUIntBig val);
    Buffer &put(GIntBig val);

    GUInt32 getULong();
    float getFloat();
    GByte getByte();
    GUInt16 getUShort();
    GUIntBig getUBig();
    GIntBig getBig();

    void seek(size_t position) { m_currentPos = position; }

private:
    int m_size;
    int m_mallocSize;
    GByte *m_data;
    size_t m_currentPos;
    bool m_own;
};

using BufferPtr = std::shared_ptr<Buffer>;

}

#endif // NGSBUFFER_H
