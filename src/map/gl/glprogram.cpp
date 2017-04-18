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
#include "glprogram.h"

#include "api_priv.h"
#include "util/error.h"

namespace ngs {

GlProgram::GlProgram() : m_id(0)
{
}

GlProgram::~GlProgram()
{
    if(0 != m_id)
        glDeleteProgram(m_id);
}

bool GlProgram::load(const GLchar * const vertexShader,
                     const GLchar * const fragmentShader)
{
    GLuint vertexShaderId = loadShader(GL_VERTEX_SHADER, vertexShader);
    if(!vertexShaderId) {
        return errorMessage(_("Load vertex shader failed"));
    }

    GLuint fragmentShaderId = loadShader(GL_FRAGMENT_SHADER, fragmentShader);
    if(!fragmentShaderId) {
        return errorMessage(_("Load fragment shader failed"));
    }

    GLuint programId = glCreateProgram();
    if (!programId) {
        return errorMessage(_("Create program failed"));
    }

    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glLinkProgram(programId);

    if(!checkLinkStatus(programId)) {
        return errorMessage(_("Link program failed"));
    }

    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);

    m_id = programId;

    return true;
}

bool GlProgram::checkLinkStatus(GLuint obj) const
{
    GLint status;
    glGetProgramiv(obj, GL_LINK_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

bool GlProgram::checkShaderCompileStatus(GLuint obj) const
{
    GLint status;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

GLuint GlProgram::loadShader(GLenum type, const char *shaderSrc)
{
    GLuint shader;
    GLint compiled;

    // Create the shader object
    shader = glCreateShader(type);
    if (!shader) {
        return 0;
    }

    // Load the shader source
    glShaderSource(shader, 1, &shaderSrc, nullptr);

    // Compile the shader
    glCompileShader(shader);

    // Check the compile status
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if(!checkShaderCompileStatus(shader)) {
       glDeleteShader(shader);
       return 0;
    }

    return shader;
}

} // namespace ngs
