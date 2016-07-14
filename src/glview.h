/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*******************************************************************************
*  Copyright (C) 2016 NextGIS, <info@nextgis.com>
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#ifndef GLVIEW_H
#define GLVIEW_H

#include "api.h"

#include <GLES2/gl2.h>
#include "EGL/egl.h"

#include <limits>

namespace ngs {

using namespace std;

typedef struct _glrgb {
    float r;
    float g;
    float b;
    float a;
} GlColor;

typedef int (*ngsBindVertexArray)(GLuint array);
typedef int (*ngsDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
typedef int (*ngsGenVertexArrays)(GLsizei n, GLuint* arrays);

class GlView
{
public:
    GlView();
    ~GlView();
    bool init();
    void setSize(int width, int height);
    bool isOk();
    void setBackgroundColor(const ngsRGBA &color);
    void fillBuffer(void* buffer);
    void clearBackground();
    void draw();

protected:
    GLuint prepareProgram();
    bool checkProgramLinkStatus(GLuint obj);
    bool checkShaderCompileStatus(GLuint obj);
    void reportGlStatus(GLuint obj);

protected:
    const GLuint m_maxUint = numeric_limits<GLuint>::max();

    EGLDisplay m_eglDisplay;
    EGLContext m_eglCtx;
    EGLSurface m_eglSurface;
    EGLConfig m_eglConf;
    GLuint m_programId;

    GlColor m_bkColor;
    int m_displayWidth, m_displayHeight;

    bool m_extensionLoad;

    ngsBindVertexArray bindVertexArrayFn;
    ngsDeleteVertexArrays deleteVertexArraysFn;
    ngsGenVertexArrays genVertexArraysFn;
};

}

#endif // GLVIEW_H
