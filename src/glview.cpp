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
#include "glview.h"

#include <iostream>
#include "cpl_error.h"
#include "cpl_string.h"

// TODO: init and uninit in drawing thread.
//https://mkonrad.net/2014/12/08/android-off-screen-rendering-using-egl-pixelbuffers.html
//http://stackoverflow.com/questions/214437/opengl-fast-off-screen-rendering
//http://stackoverflow.com/questions/14785007/can-i-use-opengl-for-off-screen-rendering/14796456#14796456
//https://gist.github.com/CartBlanche/1271517
//http://stackoverflow.com/questions/21151259/replacing-glreadpixels-with-egl-khr-image-base-for-faster-pixel-copy
//https://vec.io/posts/faster-alternatives-to-glreadpixels-and-glteximage2d-in-opengl-es
//https://www.khronos.org/registry/egl/sdk/docs/man/html/eglIntro.xhtml
//https://wiki.maemo.org/SimpleGL_example
//http://stackoverflow.com/questions/12906971/difference-from-eglcreatepbuffersurface-and-eglcreatepixmapsurface-with-opengl-e
//http://stackoverflow.com/questions/25504188/is-it-possible-to-use-pixmaps-on-android-via-java-api-for-gles

//https://solarianprogrammer.com/2013/05/13/opengl-101-drawing-primitives/
//http://www.glprogramming.com/red/chapter02.html
//https://www3.ntu.edu.sg/home/ehchua/programming/opengl/CG_Introduction.html
//https://www3.ntu.edu.sg/home/ehchua/programming/android/Android_3D.html
//https://www.opengl.org/sdk/docs/man2/xhtml/gluUnProject.xml
//https://www.opengl.org/sdk/docs/man2/xhtml/gluProject.xml

//https://github.com/libmx3/mx3/blob/master/src/event_loop.cpp

//https://www.mapbox.com/blog/drawing-antialiased-lines/
//https://github.com/afiskon/cpp-opengl-vbo-vao-shaders/blob/master/main.cpp

using namespace ngs;

// test data
static const GLfloat globVertexBufferData[] = {
  -1.0f, -1.0f,  0.0f,
   1.0f, -1.0f,  0.0f,
   0.0f,  1.0f,  0.0f,
};

GlView::GlView() : m_eglDisplay(EGL_NO_DISPLAY), m_eglCtx(EGL_NO_CONTEXT),
    m_eglSurface(EGL_NO_SURFACE), m_programId(m_maxUint), m_displayWidth(100),
    m_displayHeight(100), m_extensionLoad(false)
{

}

GlView::~GlView()
{
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if(m_programId != m_maxUint)
        glDeleteProgram(m_programId);

    eglDestroyContext( m_eglDisplay, m_eglCtx );
    eglDestroySurface( m_eglDisplay, m_eglSurface );
    eglTerminate ( m_eglDisplay );

    m_eglDisplay = EGL_NO_DISPLAY;
    m_eglSurface = EGL_NO_SURFACE;
    m_eglCtx = EGL_NO_CONTEXT;
}

bool GlView::init()
{
    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Get GL display failed.");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize( m_eglDisplay, &major, &minor )) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Get GL version failed.");
        return false;
    }

    if ((major <= 1) && (minor < 1)) {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported GL version.");
        return false;
    }

#ifdef _DEBUG
    cout << "Vendor: " << eglQueryString(m_eglDisplay, EGL_VENDOR) << endl;
    cout << "Version: " << eglQueryString(m_eglDisplay, EGL_VERSION) << endl;
    cout << "Client APIs: " << eglQueryString(m_eglDisplay, EGL_CLIENT_APIS) << endl;
    cout << "Client Extensions: " << eglQueryString(m_eglDisplay, EGL_EXTENSIONS) << endl;
#endif // _DEBUG

    // EGL config attributes
    const EGLint confAttr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,    // very important!
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,          // we will create a pixelbuffer surface
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,     // if you need the alpha channel
        EGL_DEPTH_SIZE, 16,    // if you need the depth buffer
        EGL_NONE
    };

    // EGL context attributes
    const EGLint ctxAttr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,              // very important!
        EGL_NONE
    };

    // choose the first config, i.e. best config
    EGLint numConfigs;
    if(!eglChooseConfig(m_eglDisplay, confAttr, &m_eglConf, 1, &numConfigs)){
        CPLError(CE_Failure, CPLE_OpenFailed, "Choose GL config failed.");
        return false;
    }

    m_eglCtx = eglCreateContext(m_eglDisplay, m_eglConf, EGL_NO_CONTEXT, ctxAttr);
    if (m_eglCtx == EGL_NO_CONTEXT) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Create GL context failed.");
        return false;
    }

    m_eglSurface = EGL_NO_SURFACE;

    // TODO: need special class to load/undload and manage shaders
    /*m_programId = prepareProgram();
    if(m_maxUint == m_programId) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Prepare program (shaders) failed.");
        return false;
    }*/

    return true;
}

void GlView::setSize(int width, int height)
{
    if(m_displayWidth == width && m_displayHeight == height)
        return;

#ifdef _DEBUG
    cout << "Size changed" << endl;
#endif // _DEBUG

    eglDestroySurface( m_eglDisplay, m_eglSurface );

    // create a pixelbuffer surface
    // surface attributes
    // the surface size is set to the input frame size
    const EGLint surfaceAttr[] = {
         EGL_WIDTH, width,
         EGL_HEIGHT, height,
         EGL_NONE
    };

    m_eglSurface = eglCreatePbufferSurface( m_eglDisplay, m_eglConf, surfaceAttr);
    if(EGL_NO_SURFACE != m_eglSurface){
        eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglCtx);
        m_displayWidth = width;
        m_displayHeight = height;

        if(!m_extensionLoad){
            const GLubyte *str = glGetString(GL_EXTENSIONS);
            if (str != nullptr) {
                const char* pszList = reinterpret_cast<const char*>(str);
#ifdef _DEBUG
                cout << "GL extensions: " << pszList << endl;
#endif // _DEBUG
                char **papszTokens=CSLTokenizeString2(pszList," ",
                                        CSLT_STRIPLEADSPACES|CSLT_STRIPENDSPACES);
                for (int i = 0; i < CSLCount(papszTokens); ++i) {
                    if(EQUAL(papszTokens[i], "GL_ARB_vertex_array_object")){
                        bindVertexArrayFn = reinterpret_cast<int (*)(GLuint)>(
                                    eglGetProcAddress("glBindVertexArray"));
                        deleteVertexArraysFn = reinterpret_cast<int (*)(GLsizei, const GLuint*)>(
                                    eglGetProcAddress("glDeleteVertexArrays"));
                        genVertexArraysFn = reinterpret_cast<int (*)(GLsizei, GLuint*)>(
                                    eglGetProcAddress("glGenVertexArrays"));
                    }
                    else if(EQUAL(papszTokens[i], "GL_OES_vertex_array_object")){
                        bindVertexArrayFn = reinterpret_cast<int (*)(GLuint)>(
                                    eglGetProcAddress("glBindVertexArrayOES"));
                        deleteVertexArraysFn = reinterpret_cast<int (*)(GLsizei, const GLuint*)>(
                                    eglGetProcAddress("glDeleteVertexArraysOES"));
                        genVertexArraysFn = reinterpret_cast<int (*)(GLsizei, GLuint*)>(
                                    eglGetProcAddress("glGenVertexArraysOES"));
                    }
                    else if(EQUAL(papszTokens[i], "GL_APPLE_vertex_array_object")){
                        bindVertexArrayFn = reinterpret_cast<int (*)(GLuint)>(
                                    eglGetProcAddress("glBindVertexArrayAPPLE"));
                        deleteVertexArraysFn = reinterpret_cast<int (*)(GLsizei, const GLuint*)>(
                                    eglGetProcAddress("glDeleteVertexArraysAPPLE"));
                        genVertexArraysFn = reinterpret_cast<int (*)(GLsizei, GLuint*)>(
                                    eglGetProcAddress("glGenVertexArraysAPPLE"));
                    }

                }
                CSLDestroy(papszTokens);

                m_extensionLoad = true;
            }
        }
    }
}

bool GlView::isOk()
{
    return EGL_NO_SURFACE != m_eglSurface;
}

void GlView::setBackgroundColor(const ngsRGBA &color)
{
    m_bkColor.r = float(color.R) / 255;
    m_bkColor.g = float(color.G) / 255;
    m_bkColor.b = float(color.B) / 255;
    m_bkColor.a = float(color.A) / 255;
}

void GlView::fillBuffer(void *buffer)
{
    eglSwapBuffers(m_eglDisplay, m_eglSurface);
    glReadPixels(0, 0, m_displayWidth, m_displayHeight, GL_RGBA,
                 GL_UNSIGNED_BYTE, buffer);

}

void GlView::clearBackground()
{
    glClearColor(m_bkColor.r, m_bkColor.g, m_bkColor.b, m_bkColor.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GlView::draw()
{
    // TODO:
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(globVertexBufferData), globVertexBufferData, GL_STATIC_DRAW);

    GLuint vao;
    genVertexArraysFn(1, &vao);

      bindVertexArrayFn(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
      glBindBuffer(GL_ARRAY_BUFFER, 0); // unbind VBO
    bindVertexArrayFn(0); // unbind VAO
}

void GlView::reportGlStatus(GLuint obj) {
    GLint length;
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
    GLchar *pLog = new GLchar[length];
    glGetShaderInfoLog(obj, length, &length, pLog);
    CPLError(CE_Failure, CPLE_AppDefined, "%s", pLog);
    delete [] pLog;
}

bool GlView::checkShaderCompileStatus(GLuint obj) {
    GLint status;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

bool GlView::checkProgramLinkStatus(GLuint obj) {
    GLint status;
    glGetProgramiv(obj, GL_LINK_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

GLuint GlView::prepareProgram() {

    // WARNING: this is only for testing!
    const GLchar * const vertexShaderSourcePtr = ""
          "#version 330 core\n"
          "layout(location = 0) in vec3 vertexPos;\n"
          "void main(){\n"
          "  gl_Position.xyz = vertexPos;\n"
          "  gl_Position.w = 1.0;\n"
          "}";

    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderId, 1, &vertexShaderSourcePtr, nullptr);
    glCompileShader(vertexShaderId);

    if( !checkShaderCompileStatus(vertexShaderId) )
        return m_maxUint;

    const GLchar * const fragmentShaderSourcePtr = ""
          "#version 330 core\n"
          "out vec3 color;\n"
          "void main() { color = vec3(0,0,1); }\n";

    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderId, 1, &fragmentShaderSourcePtr, nullptr);
    glCompileShader(fragmentShaderId);

    if (!checkShaderCompileStatus(fragmentShaderId) )
        return m_maxUint;

    GLuint programId = glCreateProgram();
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);
    glLinkProgram(programId);

    if( !checkProgramLinkStatus(programId) )
        return m_maxUint;

    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);

    return programId;
}
