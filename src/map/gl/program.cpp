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
#include "program.h"

#include "cpl_string.h"

#include "api_priv.h"
#include "util/error.h"

namespace ngs {

GlProgram::GlProgram() : m_id(0),
    m_loaded(false)
{
}

GlProgram::~GlProgram()
{
    destroy();
}

void GlProgram::destroy()
{
    if(m_loaded) {
        glDeleteProgram(m_id);
        m_loaded = false;
    }
}

bool GlProgram::load(const GLchar * const vertexShader,
                     const GLchar * const fragmentShader)
{
    if(m_loaded) {
        return true;
    }

    CPLString fagShStr(fragmentShader);
#ifdef TARGET_OS_MAC
    fagShStr = "#version 120\n" + fagShStr;
#else
    fagShStr = "precision mediump float;\n" + fagShStr;
#endif // TARGET_OS_MAC

    GLuint vertexShaderId = loadShader(GL_VERTEX_SHADER, vertexShader);
    if(!vertexShaderId) {
        return errorMessage(_("Load vertex shader failed"));
    }

    GLuint fragmentShaderId = loadShader(GL_FRAGMENT_SHADER, fagShStr.c_str());
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

#ifdef _DEBUG
    GLint numActiveUniforms = 0;
    glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &numActiveUniforms);
    warningMessage("Number of active uniforms: %d", numActiveUniforms);
#endif //_DEBUG

    m_id = programId;
    m_loaded = true;

    return true;
}

void GlProgram::setMatrix(const char* varName, std::array<GLfloat, 16> mat4f)
{
    if(m_loaded) {
        ngsCheckGLError(glUniformMatrix4fv(getVariableId(varName),
                                           1, GL_FALSE, mat4f.data()));
    }
}

void GlProgram::setColor(const char* varName, const GlColor &color)
{
    if(m_loaded) {
        ngsCheckGLError(glUniform4f(getVariableId(varName),
                                    color.r, color.g, color.b, color.a));
    }
}

void GlProgram::setInt(const char *varName, GLint value)
{
    if(m_loaded) {
        ngsCheckGLError(glUniform1i(getVariableId(varName), value));
    }
}

void GlProgram::setInt(const char *varName, GLint value) const
{
    if(m_loaded) {
        ngsCheckGLError(glUniform1i(getVariableId(varName), value));
    }
}

void GlProgram::setFloat(const char *varName, GLfloat value)
{
    if(m_loaded) {
        ngsCheckGLError(glUniform1f(getVariableId(varName), value));
    }
}

void GlProgram::setVertexAttribPointer(const char *varName, GLint size,
                                       GLsizei stride, const GLvoid *pointer)
{
    if(m_loaded) {
        GLuint index = static_cast<GLuint>(getAttributeId(varName));
        ngsCheckGLError(glEnableVertexAttribArray(index));
        ngsCheckGLError(glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE,
                                              stride, pointer));
    }
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

GLint GlProgram::getVariableId(const char *varName)
{
    auto it = m_variables.find(varName);
    GLint variableId;
    if(it != m_variables.end()) {
        variableId = it->second;
    }
    else {
        variableId = glGetUniformLocation(m_id, varName);
        if(variableId > 0) {
            m_variables[varName] = variableId;
        }
    }
    return variableId;
}


GLint GlProgram::getVariableId(const char *varName) const
{
    auto it = m_variables.find(varName);
    GLint variableId;
    if(it != m_variables.end()) {
        variableId = it->second;
    }
    else {
        variableId = 0;
    }
    return variableId;
}

GLint GlProgram::getAttributeId(const char *varName)
{
    auto it = m_variables.find(varName);
    GLint variableId;
    if(it != m_variables.end()) {
        variableId = it->second;
    }
    else {
        variableId = glGetAttribLocation(m_id, varName);
        if(variableId >= 0) {
            m_variables[varName] = variableId;
        }
    }
    return variableId;

}

} // namespace ngs
