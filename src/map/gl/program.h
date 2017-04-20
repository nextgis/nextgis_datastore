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

namespace ngs {

class GlProgram
{
public:
    GlProgram();
    ~GlProgram();
    bool load(const GLchar * const vertexShader,
              const GLchar * const fragmentShader);

    bool isLoad() const { return m_loaded; }
    void use() const { ngsCheckGLError(glUseProgram(m_id)); }
    void setMatrix(const char* varName, std::array<GLfloat, 16> mat4f);
    void setColor(const char *varName, const GlColor& color);
    void setInt(const char *varName, GLint value);
    void setInt(const char *varName, GLint value) const;
    void setFloat(const char *varName, GLfloat value);
    void setVertexAttribPointer(const char *varName, GLint size,
                                GLsizei stride, const GLvoid * pointer);
    void destroy();

protected:
    bool checkLinkStatus(GLuint obj) const;
    bool checkShaderCompileStatus(GLuint obj) const;
    GLuint loadShader(GLenum type, const char *shaderSrc);
    GLint getVariableId(const char* varName);
    GLint getVariableId(const char* varName) const;
    GLint getAttributeId(const char* varName);
protected:
    GLuint m_id;
    bool m_loaded;
    std::map<const char*, GLint> m_variables;
};

//typedef std::unique_ptr<GlProgram> GlProgramUPtr;

}

#endif // NGSGLPROGRAM_H
