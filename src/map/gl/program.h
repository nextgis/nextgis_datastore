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
#ifndef NGSGLPROGRAM_H
#define NGSGLPROGRAM_H

#include "functions.h"

#include <array>
#include <map>

#include "map/glm/mat4x4.hpp"

namespace ngs {

class GlProgram
{
public:
    GlProgram();
    ~GlProgram();
    bool load(const GLchar * const vertexShader,
              const GLchar * const fragmentShader);

    bool loaded() const { return m_loaded; }
    void use() const { ngsCheckGLError(glUseProgram(m_id)); }
    void setMatrix(const std::string &varName, const glm::mat4 &mat4f);
    void setColor(const std::string &varName, const GlColor &color);
    void setInt(const std::string &varName, GLint value);
    void setInt(const std::string &varName, GLint value) const;
    void setFloat(const std::string &varName, GLfloat value);
    void setVertexAttribPointer(const std::string &varName, GLint size,
                                GLsizei stride, const GLvoid *pointer);
    void destroy();

protected:
    bool checkLinkStatus(GLuint obj) const;
    bool checkShaderCompileStatus(GLuint obj) const;
    GLuint loadShader(GLenum type, const std::string &shaderSrc);
    GLint getVariableId(const std::string &varName);
    GLint getVariableId(const std::string &varName) const;
    GLint getAttributeId(const std::string &varName);
protected:
    GLuint m_id;
    bool m_loaded;
    std::map<std::string, GLint> m_variables;
};

} // namespace ngs

#endif // NGSGLPROGRAM_H
