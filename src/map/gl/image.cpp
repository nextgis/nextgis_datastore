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
#include "image.h"

#include "cpl_conv.h"

namespace ngs {

GlImage::GlImage() : GlObject(),
    m_id(0), m_smooth(false)
{
}

void GlImage::bind()
{
    if (m_bound)
        return;
    ngsCheckGLError(glGenTextures(1, &m_id));
    rebind();
    ngsCheckGLError(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, m_imageData));
    CPLFree(m_imageData);
    m_imageData = nullptr;
    m_bound = true;
}

void GlImage::rebind() const
{
    ngsCheckGLError(glBindTexture(GL_TEXTURE_2D, m_id));
    ngsCheckGLError(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    ngsCheckGLError(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    ngsCheckGLError(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_smooth ? GL_LINEAR : GL_NEAREST));
    ngsCheckGLError(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_smooth ? GL_LINEAR : GL_NEAREST));
}

void GlImage::destroy()
{
    if (m_bound) {
        ngsCheckGLError(glDeleteTextures(1, &m_id));
    }
    else if(m_imageData) {
        CPLFree(m_imageData);
    }
}

} // namespace ngs
