/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*  Author: NikitaFeodonit, nfeodonit@yandex.com
*******************************************************************************
*  Copyright (C) 2016-2017 NextGIS, <info@nextgis.com>
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
#include <algorithm>
#include "cpl_error.h"
#include "cpl_string.h"
#include "glview.h"
#include <iostream>
#include "map/glfillers.h"
#include "style.h"
#include "util/constants.h"

/* Links:
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
*/

#define MAX_VERTEX_BUFFER_SIZE 16383
#define MAX_INDEX_BUFFER_SIZE 16383
#define MAX_GLOBAL_VERTEX_BUFFER_SIZE 327180000
#define MAX_GLOBAL_INDEX_BUFFER_SIZE 327180000

using namespace ngs;

//------------------------------------------------------------------------------
bool ngs::checkGLError(const char *cmd) {
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        const char *error = nullptr;
        switch (err) {
            case GL_INVALID_ENUM:
                error = "INVALID_ENUM";
            break;
            case GL_INVALID_VALUE:
                error = "INVALID_VALUE";
            break;
            case GL_INVALID_OPERATION:
                error = "INVALID_OPERATION";
            break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                error = "INVALID_FRAMEBUFFER_OPERATION";
            break;
            case GL_OUT_OF_MEMORY:
                error = "OUT_OF_MEMORY";
            break;
#ifdef GL_STACK_UNDERFLOW
            case GL_STACK_UNDERFLOW:
                error = "STACK_UNDERFLOW";
            break;
#endif
#ifdef GL_STACK_OVERFLOW
            case GL_STACK_OVERFLOW:
                error = "STACK_OVERFLOW";
            break;
#endif
            default:
                error = "(unknown)";
        }

        CPLError(CE_Failure, CPLE_AppDefined, "%s: Error GL_%s", cmd, error);

        return true;
    }
    return false;
}

void ngs::reportGlStatus(GLuint obj) {
    GLint length;
    ngsCheckGLEerror(glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length));
    GLchar *log = new GLchar[length];
    ngsCheckGLEerror(glGetShaderInfoLog(obj, length, &length, log));
    CPLError(CE_Failure, CPLE_AppDefined, "%s", log);
    delete [] log;
}

#ifdef OFFSCREEN_GL
//------------------------------------------------------------------------------
bool ngs::checkEGLError(const char *cmd) {
    EGLint err = eglGetError();
    if(err != EGL_SUCCESS){
        const char *error = nullptr;
        switch (err) {
        case EGL_NOT_INITIALIZED:
            error = "NOT_INITIALIZED";
            break;
        case EGL_BAD_ACCESS:
            error = "BAD_ACCESS";
        break;
        case EGL_BAD_ALLOC:
            error = "BAD_ALLOC";
            break;
        case EGL_BAD_ATTRIBUTE:
            error = "BAD_ATTRIBUTE";
            break;
        case EGL_BAD_CONTEXT:
            error = "BAD_CONTEXT";
            break;
        case EGL_BAD_CONFIG:
            error = "BAD_CONFIG";
            break;
        case EGL_BAD_CURRENT_SURFACE:
            error = "BAD_CURRENT_SURFACE";
            break;
        case EGL_BAD_DISPLAY:
            error = "BAD_DISPLAY";
            break;
        case EGL_BAD_SURFACE:
            error = "BAD_SURFACE";
            break;
        case EGL_BAD_MATCH:
            error = "BAD_MATCH";
            break;
        case EGL_BAD_PARAMETER:
            error = "BAD_PARAMETER";
            break;
        case EGL_BAD_NATIVE_PIXMAP:
            error = "BAD_NATIVE_PIXMAP";
            break;
        case EGL_BAD_NATIVE_WINDOW:
            error = "BAD_NATIVE_WINDOW";
            break;
        case EGL_CONTEXT_LOST:
            error = "CONTEXT_LOST";
            break;
        }
        CPLError(CE_Failure, CPLE_AppDefined, "%s: Error EGL_%s", cmd, error);
        return true;
    }
    return false;
}

static GlDisplayPtr gGlDisplay = nullptr;
GlDisplayPtr ngs::getGlDisplay()
{
    if(gGlDisplay)
        return gGlDisplay;
    gGlDisplay.reset (new GlDisplay());
    if(gGlDisplay->init())
        return gGlDisplay;
    gGlDisplay.reset ();
    return gGlDisplay;
}

//------------------------------------------------------------------------------
// GlDisplay
//------------------------------------------------------------------------------

GlDisplay::GlDisplay() : m_eglDisplay(EGL_NO_DISPLAY)
{

}

GlDisplay::~GlDisplay()
{
    eglMakeCurrent ( m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglTerminate ( m_eglDisplay );
    m_eglDisplay = EGL_NO_DISPLAY;
}

bool GlDisplay::init()
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

    ngsCheckEGLEerror(eglBindAPI(EGL_OPENGL_ES_API));

    EGLint numConfigs = 0;
#ifdef _DEBUG
    cout << "Vendor: " << eglQueryString(m_eglDisplay, EGL_VENDOR) << endl;
    cout << "Version: " << eglQueryString(m_eglDisplay, EGL_VERSION) << endl;
    cout << "Client APIs: " << eglQueryString(m_eglDisplay, EGL_CLIENT_APIS) << endl;
    cout << "Client Extensions: " << eglQueryString(m_eglDisplay, EGL_EXTENSIONS) << endl;

    EGLConfig configs[10];
    EGLint id, red, depth, type;
    eglGetConfigs(m_eglDisplay, configs, 10, &numConfigs);
       cout << "Got " << numConfigs << " EGL configs:" << endl;
       for (EGLint i = 0; i < numConfigs; ++i) {
          eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_CONFIG_ID, &id);
          eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_SURFACE_TYPE, &type);
          eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_RED_SIZE, &red);
          eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_DEPTH_SIZE, &depth);
          cout << id <<
                  " Type = " << type <<
                  " Red Size = " << red <<
                  " Depth Size = " << depth <<
                  endl;
       }
#endif // _DEBUG

    // EGL config attributes
    const EGLint confAttr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,    // very important!
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,          // we will create a pixelbuffer surface
        EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,     // if you need the alpha channel
        EGL_DEPTH_SIZE, 16,    // if you need the depth buffer
        EGL_NONE
    };
    numConfigs = 0;

    // choose the first config, i.e. best config
    if(!eglChooseConfig(m_eglDisplay, confAttr, &m_eglConf, 1, &numConfigs) || numConfigs != 1){
        CPLError(CE_Failure, CPLE_OpenFailed, "Choose GL config failed.");
        return false;
    }

#ifdef _DEBUG
      eglGetConfigAttrib(m_eglDisplay, m_eglConf, EGL_CONFIG_ID, &id);
      eglGetConfigAttrib(m_eglDisplay, m_eglConf, EGL_SURFACE_TYPE, &type);
      eglGetConfigAttrib(m_eglDisplay, m_eglConf, EGL_RED_SIZE, &red);
      eglGetConfigAttrib(m_eglDisplay, m_eglConf, EGL_DEPTH_SIZE, &depth);
      cout << "Selected config: " <<
              " Type = " << type <<
              " Red Size = " << red <<
              " Depth Size = " << depth <<
              endl;
#endif // _DEBUG
    return true;
}

EGLDisplay GlDisplay::eglDisplay() const
{
    return m_eglDisplay;
}

EGLConfig GlDisplay::eglConf() const
{
    return m_eglConf;
}

//------------------------------------------------------------------------------
// GlView
//------------------------------------------------------------------------------

GlView::GlView() : m_eglCtx(EGL_NO_CONTEXT),
    m_eglSurface(EGL_NO_SURFACE), m_displayWidth(100),
    m_displayHeight(100), m_extensionLoad(false), m_programLoad(false),
    m_programId(0)
{
}

GlView::~GlView()
{
    if(m_programId)
        glDeleteProgram(m_programId);

    eglDestroyContext( m_glDisplay->eglDisplay (), m_eglCtx );
    eglDestroySurface( m_glDisplay->eglDisplay (), m_eglSurface );

    m_eglSurface = EGL_NO_SURFACE;
    m_eglCtx = EGL_NO_CONTEXT;
}

bool GlView::init()
{
    // get or create Gl display
    m_glDisplay = getGlDisplay ();
    if(!m_glDisplay) {
        CPLError(CE_Failure, CPLE_OpenFailed, "GL display is not initialized.");
        return false;
    }

    // EGL context attributes
    const EGLint ctxAttr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,              // very important!
        EGL_NONE
    };

    m_eglCtx = eglCreateContext(m_glDisplay->eglDisplay (),
                                m_glDisplay->eglConf (), EGL_NO_CONTEXT, ctxAttr);
    if (m_eglCtx == EGL_NO_CONTEXT) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Create GL context failed.");
        return false;
    }

#ifdef _DEBUG
    checkEGLError("eglCreateContext");
#endif // _DEBUG

    m_eglSurface = EGL_NO_SURFACE;

    return true;
}

void GlView::setSize(int width, int height)
{
    if(m_displayWidth == width && m_displayHeight == height)
        return;

    m_displayWidth = width;
    m_displayHeight = height;

#ifdef _DEBUG
    cout << "Size changed" << endl;
#endif // _DEBUG

    if(!createSurface ())
        return;

#ifdef _DEBUG
    EGLint w, h;
    eglQuerySurface(m_glDisplay->eglDisplay (), m_eglSurface, EGL_WIDTH, &w);
    checkEGLError ("eglQuerySurface");
    cout << "EGL_WIDTH: " << w << endl;
    eglQuerySurface(m_glDisplay->eglDisplay (), m_eglSurface, EGL_HEIGHT, &h);
    checkEGLError ("eglQuerySurface");
    cout << "EGL_HEIGHT: " << h << endl;
#endif // _DEBUG

    loadExtensions();
    loadProgram ();
    ngsCheckGLEerror(glClearColor(m_bkColor.r, m_bkColor.g, m_bkColor.b,
                                   m_bkColor.a));

    ngsCheckGLEerror(glEnable(GL_DEPTH_TEST));
    ngsCheckGLEerror(glViewport ( 0, 0, m_displayWidth, m_displayHeight ));
}

bool GlView::isOk() const
{
    return EGL_NO_SURFACE != m_eglSurface;
}

void GlView::setBackgroundColor(const ngsRGBA &color)
{
    m_bkColor.r = float(color.R) / 255;
    m_bkColor.g = float(color.G) / 255;
    m_bkColor.b = float(color.B) / 255;
    m_bkColor.a = float(color.A) / 255;
    ngsCheckGLEerror(glClearColor(m_bkColor.r, m_bkColor.g, m_bkColor.b,
                                  m_bkColor.a));
}

void GlView::fillBuffer(void *buffer) const
{
    if(nullptr == buffer)
        return;
    ngsCheckEGLEerror(eglSwapBuffers(m_glDisplay->eglDisplay (), m_eglSurface));
    ngsCheckGLEerror(glReadPixels(0, 0, m_displayWidth, m_displayHeight,
                                   GL_RGBA, GL_UNSIGNED_BYTE, buffer));
}

void GlView::clearBackground()
{    
    ngsCheckGLEerror(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void GlView::prepare(const Matrix4 &mat)
{
    clearBackground();
    ngsCheckGLEerror(glUseProgram(m_programId));

#ifdef _DEBUG
    GLint numActiveUniforms = 0;
    glGetProgramiv(m_programId, GL_ACTIVE_UNIFORMS, &numActiveUniforms);
    cout << "Number active uniforms: " << numActiveUniforms << endl;
#endif //_DEBUG
    GLint location = glGetUniformLocation(m_programId, "mvMatrix");

    array<GLfloat, 16> mat4f = mat.dataF ();
    ngsCheckGLEerror(glUniformMatrix4fv(location, 1, GL_FALSE, mat4f.data()));
}

// WARNING: this is for test
void GlView::draw() const
{
    int uColorLocation = glGetUniformLocation(m_programId, "u_Color");

    GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                          };

    glUniform4f(uColorLocation, 1.0f, 0.0f, 0.0f, 1.0f);

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                           -2236992.0f, 3972353.0f, 0.5f,
                            5187591.0f, 4509961.0f, 0.5f
                          };

    glUniform4f(uColorLocation, 0.0f, 0.0f, 1.0f, 1.0f);

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices2 ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));
}

void GlView::drawPolygons(const vector<GLfloat> &vertices,
                          const vector<GLushort> &indices) const
{
    if(vertices.empty() || indices.empty ())
        return;
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));
    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                            vertices.data ()));

    // IBO
    /*GLuint IBO;
    glGenBuffers(1, &IBO );
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size () * sizeof(GLushort), indices.data (), GL_STATIC_DRAW);*/

    //glDrawArrays(GL_TRIANGLES, 0, 3); // this works
    //glDrawElements(GL_TRIANGLES, indices.size (), GL_UNSIGNED_INT, 0); // this doesnt


    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES, indices.size (),
                                    GL_UNSIGNED_SHORT, indices.data ()));
}

void GlView::loadProgram()
{
    if(!m_programLoad) {
        // TODO: need special class to load/undload and manage shaders
        m_programId = prepareProgram();
        if(!m_programId) {
            CPLError(CE_Failure, CPLE_OpenFailed, "Prepare program (shaders) failed.");
            return;
        }
        m_programLoad = true;
    }
}


bool GlView::checkShaderCompileStatus(GLuint obj) const {
    GLint status;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

bool GlView::checkProgramLinkStatus(GLuint obj) const {
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
    const GLchar * const vertexShaderSourcePtr =
            "attribute vec4 vPosition;    \n"
            "uniform mat4 mvMatrix;       \n"
            "void main()                  \n"
            "{                            \n"
            "   gl_Position = mvMatrix * vPosition;  \n"
            "}                            \n";

    GLuint vertexShaderId = loadShader(GL_VERTEX_SHADER, vertexShaderSourcePtr);

    if( !vertexShaderId )
        return 0;

    const GLchar * const fragmentShaderSourcePtr =
            "precision mediump float;                     \n"
            "uniform vec4 u_Color;                        \n"
            "void main()                                  \n"
            "{                                            \n"
            "  gl_FragColor = u_Color;                    \n"
            "}                                            \n";

    GLuint fragmentShaderId = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSourcePtr);
    if( !fragmentShaderId )
        return 0;

    GLuint programId = glCreateProgram();
    if ( !programId )
       return 0;

    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glBindAttribLocation ( programId, 0, "vPosition" );
    glLinkProgram(programId);

    if( !checkProgramLinkStatus(programId) )
        return 0;

    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);

    return programId;
}

GLuint GlView::loadShader ( GLenum type, const char *shaderSrc )
{
   GLuint shader;
   GLint compiled;

   // Create the shader object
   shader = glCreateShader ( type );

   if ( !shader )
    return 0;

   // Load the shader source
   glShaderSource ( shader, 1, &shaderSrc, nullptr );

   // Compile the shader
   glCompileShader ( shader );

   // Check the compile status
   glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

   if (!checkShaderCompileStatus(shader) ) {
      glDeleteShader ( shader );
      return 0;
   }

   return shader;

}

void GlView::loadExtensions()
{
/*  if(!m_extensionLoad){
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
    }*/

}

//------------------------------------------------------------------------------
// GlOffScreenView
//------------------------------------------------------------------------------

GlOffScreenView::GlOffScreenView() : GlView(), m_defaultFramebuffer(0)
{
    memset(m_renderbuffers, 0, sizeof(m_renderbuffers));
}

GlOffScreenView::~GlOffScreenView()
{

}

bool GlOffScreenView::createSurface()
{
    if(m_eglSurface != EGL_NO_SURFACE)
        eglDestroySurface( m_glDisplay->eglDisplay (), m_eglSurface );

    destroyFBO ();

    // create a pixelbuffer surface
    // surface attributes
    // the surface size is set to the input frame size
    const EGLint surfaceAttr[] = {
         EGL_WIDTH, m_displayWidth,
         EGL_HEIGHT, m_displayHeight,
         EGL_LARGEST_PBUFFER, EGL_TRUE,
         EGL_NONE
    };

    // NOTE: need to create both pbuffer and FBO to draw into offscreen buffer
    // see: http://stackoverflow.com/q/28817777/2901140

    m_eglSurface = eglCreatePbufferSurface( m_glDisplay->eglDisplay (),
                                            m_glDisplay->eglConf (), surfaceAttr);

#ifdef _DEBUG
    checkEGLError("eglCreatePbufferSurface");
#endif // _DEBUG

    if(EGL_NO_SURFACE != m_eglSurface){

        if(!eglMakeCurrent(m_glDisplay->eglDisplay (), m_eglSurface, m_eglSurface,
                           m_eglCtx)) {
            CPLError(CE_Failure, CPLE_OpenFailed, "eglMakeCurrent failed.");
            return false;
        }

        if(!createFBO (m_displayWidth, m_displayHeight)){
            CPLError(CE_Failure, CPLE_OpenFailed, "createFBO failed.");
            return false;
        }
    }
    else {
        CPLError(CE_Failure, CPLE_OpenFailed, "eglCreatePbufferSurface failed.");
        return false;
    }

    return true;
}

void GlOffScreenView::destroyFBO()
{
    ngsCheckGLEerror(glDeleteFramebuffers(1, &m_defaultFramebuffer));
    ngsCheckGLEerror(glDeleteRenderbuffers(ARRAY_SIZE(m_renderbuffers),
                                           m_renderbuffers));
}

bool GlOffScreenView::createFBO(int width, int height)
{
    //Create the FrameBuffer and binds it
    ngsCheckGLEerror(glGenFramebuffers(1, &m_defaultFramebuffer));
    ngsCheckGLEerror(glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFramebuffer));

    ngsCheckGLEerror(glGenRenderbuffers(ARRAY_SIZE(m_renderbuffers),
                                        m_renderbuffers));

    //Create the RenderBuffer for offscreen rendering // Color
    ngsCheckGLEerror(glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffers[0]));
    ngsCheckGLEerror(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, width,
                                           height));

    //Create the RenderBuffer for offscreen rendering // Depth
    ngsCheckGLEerror(glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffers[1]));
    ngsCheckGLEerror(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                           width, height));

    //Create the RenderBuffer for offscreen rendering // Stencil
    /*glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffers[2]);
    checkGLError("glBindRenderbuffer stencil");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
    checkGLError("glRenderbufferStorage stencil");*/

    // bind renderbuffers to framebuffer object
    ngsCheckGLEerror(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffers[1]));
    ngsCheckGLEerror(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_renderbuffers[0]));
    /*glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffers[2]);
    checkGLError("glFramebufferRenderbuffer stencil");*/

    //Test for FrameBuffer completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    checkGLError("glCheckFramebufferStatus");
    const char* msg = nullptr;
    bool res = false;
    switch (status) {
        case GL_FRAMEBUFFER_COMPLETE:
            msg = "FBO complete  GL_FRAMEBUFFER_COMPLETE";
            res = true;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            msg = "FBO GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            msg = "FBO FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
            msg = "FBO FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            msg = "FBO GL_FRAMEBUFFER_UNSUPPORTED";
            break;
        default:
            msg = "Failed to make complete framebuffer object";
        }
#ifdef _DEBUG
    cout << msg << " " << status << endl;
#endif //_DEBUG

    return res;
}

#endif // OFFSCREEN_GL

//------------------------------------------------------------------------------
// GlProgram
//------------------------------------------------------------------------------

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
    if( !vertexShaderId ) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Load vertex shader failed.");
        return false;
    }

    GLuint fragmentShaderId = loadShader(GL_FRAGMENT_SHADER, fragmentShader);
    if( !fragmentShaderId ) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Load fragment shader failed.");
        return false;
    }

    GLuint programId = glCreateProgram();
    if ( !programId ) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Create program failed.");
        return false;
    }

    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glLinkProgram(programId);

    if( !checkLinkStatus(programId) ){
        CPLError(CE_Failure, CPLE_OpenFailed, "Link program failed.");
        return false;
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
    shader = glCreateShader ( type );

    if ( !shader )
     return 0;

    // Load the shader source
    glShaderSource ( shader, 1, &shaderSrc, nullptr );

    // Compile the shader
    glCompileShader ( shader );

    // Check the compile status
    glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

    if (!checkShaderCompileStatus(shader) ) {
       glDeleteShader ( shader );
       return 0;
    }

    return shader;
}

GLuint GlProgram::id() const
{
    return m_id;
}

void GlProgram::use() const
{
    ngsCheckGLEerror(glUseProgram(m_id));
}

//------------------------------------------------------------------------------
// GlFuctions
//------------------------------------------------------------------------------

GlFuctions::GlFuctions() : m_extensionLoad(false), m_pBkChanged(true)
{
}

GlFuctions::~GlFuctions()
{
    // TODO: unload extensions
}

bool GlFuctions::init()
{
    if(!loadExtensions ())
        return false;
    return true;
}

bool GlFuctions::isOk() const
{
    return m_extensionLoad;
}

void GlFuctions::setBackgroundColor(const ngsRGBA &color)
{
    m_bkColor.r = float(color.R) / 255;
    m_bkColor.g = float(color.G) / 255;
    m_bkColor.b = float(color.B) / 255;
    m_bkColor.a = float(color.A) / 255;

    m_pBkChanged = true;
}

// NOTE: Should be run on current context
void GlFuctions::clearBackground()
{
    if(m_pBkChanged) {
        ngsCheckGLEerror(glClearColor(m_bkColor.r, m_bkColor.g, m_bkColor.b,
                                  m_bkColor.a));
        m_pBkChanged = false;
    }
    ngsCheckGLEerror(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void GlFuctions::testDrawPreserved(int colorId) const
{
    static bool isBuffersFilled = false;
    static GLuint    buffers[2];
    if(!isBuffersFilled) {
        isBuffersFilled = true;
        ngsCheckGLEerror(glGenBuffers(2, buffers)); // TODO: glDeleteBuffers

        ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));

        GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                               -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                                4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                              };

        ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices), vVertices, GL_STATIC_DRAW));

        ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[1]));

        GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                               -2236992.0f, 3972353.0f, 0.5f,
                                5187591.0f, 4509961.0f, 0.5f
                              };

        ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices2), vVertices2, GL_STATIC_DRAW));
    }

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));

    ngsCheckGLEerror(glUniform4f(colorId, 1.0f, 0.0f, 0.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));

    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[1]));

    ngsCheckGLEerror(glUniform4f(colorId, 0.0f, 0.0f, 1.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));

    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

}

void GlFuctions::testDraw(int colorId) const
{
    GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                          };

    ngsCheckGLEerror(glUniform4f(colorId, 1.0f, 0.0f, 0.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                           -2236992.0f, 3972353.0f, 0.5f,
                            5187591.0f, 4509961.0f, 0.5f
                          };

    ngsCheckGLEerror(glUniform4f(colorId, 0.0f, 0.0f, 1.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices2 ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));
}

void GlFuctions::drawPolygons(const vector<GLfloat> &vertices,
                              const vector<GLushort> &indices) const
{
    if(vertices.empty() || indices.empty ())
        return;
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));
    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                            vertices.data ()));
    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES, indices.size (),
                                    GL_UNSIGNED_SHORT, indices.data ()));
}

bool GlFuctions::loadExtensions()
{
    m_extensionLoad = true;
    return true;
}

//------------------------------------------------------------------------------
// GlBuffer
//------------------------------------------------------------------------------

// static
std::atomic_int_fast32_t GlBuffer::m_globalVertexBufferSize;
// static
std::atomic_int_fast32_t GlBuffer::m_globalIndexBufferSize;
// static
std::atomic_int_fast32_t GlBuffer::m_globalBorderIndexBufferSize;
// static
std::atomic_int_fast32_t GlBuffer::m_globalHardBuffersCount;

GlBuffer::GlBuffer()
        : m_bound(false)
        , m_finalVertexBufferSize(0)
        , m_finalIndexBufferSize(0)
        , m_finalBorderIndexBufferSize(0)
        , m_glHardBufferIds{
                  GL_BUFFER_UNKNOWN, GL_BUFFER_UNKNOWN, GL_BUFFER_UNKNOWN}
{
    m_vertices.reserve(MAX_VERTEX_BUFFER_SIZE);
    m_indices.reserve(MAX_INDEX_BUFFER_SIZE);
    m_borderIndices.reserve(MAX_INDEX_BUFFER_SIZE);
}

GlBuffer::GlBuffer(GlBuffer&& other)
{
    *this = std::move(other);
}

GlBuffer::~GlBuffer()
{
    m_globalVertexBufferSize.fetch_sub(getVertexBufferSize());
    m_globalIndexBufferSize.fetch_sub(getIndexBufferSize());
    m_globalBorderIndexBufferSize.fetch_sub(
            getIndexBufferSize(BF_BORDER_INDICES));

    if (m_bound) {
        glDeleteBuffers(GL_BUFFERS_COUNT, m_glHardBufferIds);
        m_globalHardBuffersCount.fetch_sub(GL_BUFFERS_COUNT);
    }
}

GlBuffer& GlBuffer::operator=(GlBuffer&& other)
{
    if (this == &other) {
        return *this;
    }

    m_globalVertexBufferSize.fetch_add(other.getVertexBufferSize());
    m_globalIndexBufferSize.fetch_add(other.getIndexBufferSize());
    m_globalBorderIndexBufferSize.fetch_add(
            other.getIndexBufferSize(BF_BORDER_INDICES));

    m_bound = other.m_bound;
    m_finalVertexBufferSize = other.m_finalVertexBufferSize;
    m_finalIndexBufferSize = other.m_finalIndexBufferSize;
    m_finalBorderIndexBufferSize = other.m_finalBorderIndexBufferSize;
    m_glHardBufferIds[0] = other.m_glHardBufferIds[0];
    m_glHardBufferIds[1] = other.m_glHardBufferIds[1];
    m_glHardBufferIds[2] = other.m_glHardBufferIds[2];

    other.m_bound = false;
    other.m_finalVertexBufferSize = 0;
    other.m_finalIndexBufferSize = 0;
    other.m_finalBorderIndexBufferSize = 0;
    other.m_glHardBufferIds[0] = GL_BUFFER_UNKNOWN;
    other.m_glHardBufferIds[1] = GL_BUFFER_UNKNOWN;
    other.m_glHardBufferIds[2] = GL_BUFFER_UNKNOWN;

    m_vertices.clear();
    m_vertices.shrink_to_fit();
    m_vertices = other.m_vertices;
    other.m_vertices.clear();
    other.m_vertices.shrink_to_fit();

    m_indices.clear();
    m_indices.shrink_to_fit();
    m_indices = other.m_indices;
    other.m_indices.clear();
    other.m_indices.shrink_to_fit();

    m_borderIndices.clear();
    m_borderIndices.shrink_to_fit();
    m_borderIndices = other.m_borderIndices;
    other.m_borderIndices.clear();
    other.m_borderIndices.shrink_to_fit();

    return *this;
}

void GlBuffer::bind()
{
    if (m_bound || m_vertices.empty() || m_indices.empty())
        return;

    m_globalHardBuffersCount.fetch_add(GL_BUFFERS_COUNT);
    ngsCheckGLEerror(glGenBuffers(GL_BUFFERS_COUNT, m_glHardBufferIds));

    m_finalVertexBufferSize = m_vertices.size();
    ngsCheckGLEerror(
            glBindBuffer(GL_ARRAY_BUFFER, getGlHardBufferId(BF_VERTICES)));
    ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER,
            sizeof(GLfloat) * m_finalVertexBufferSize, m_vertices.data(),
            GL_STATIC_DRAW));
    m_vertices.clear();

    m_finalIndexBufferSize = m_indices.size();
    ngsCheckGLEerror(glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER, getGlHardBufferId(BF_INDICES)));
    ngsCheckGLEerror(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            sizeof(GLushort) * m_finalIndexBufferSize, m_indices.data(),
            GL_STATIC_DRAW));
    m_indices.clear();

    if (!m_borderIndices.empty()) {
        m_finalBorderIndexBufferSize = m_borderIndices.size();
        ngsCheckGLEerror(glBindBuffer(
                GL_ELEMENT_ARRAY_BUFFER, getGlHardBufferId(BF_BORDER_INDICES)));
        ngsCheckGLEerror(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                sizeof(GLushort) * m_finalBorderIndexBufferSize,
                m_borderIndices.data(), GL_STATIC_DRAW));
        m_borderIndices.clear();
    }

    m_bound = true;
}

bool GlBuffer::bound() const
{
    return m_bound;
}

// static
bool GlBuffer::canGlobalStoreVertices(size_t amount, bool withNormals)
{
    return (m_globalVertexBufferSize.load()
                   + amount * (withNormals ? VERTEX_WITH_NORMAL_SIZE
                                           : VERTEX_SIZE))
            < MAX_GLOBAL_VERTEX_BUFFER_SIZE;
}

// static
bool GlBuffer::canGlobalStoreIndices(
        size_t amount, enum ngsBufferType indexType)
{
    int_fast32_t size;
    switch (indexType) {
        case BF_INDICES:
            size = m_globalIndexBufferSize.load();
            break;
        case BF_BORDER_INDICES:
            size = m_globalBorderIndexBufferSize.load();
            break;
        default:
            return false;
    }

    return (size + amount) < MAX_GLOBAL_INDEX_BUFFER_SIZE;
}

bool GlBuffer::canStoreVertices(size_t amount, bool withNormals) const
{
    return ((m_vertices.size()
                    + amount * (withNormals ? VERTEX_WITH_NORMAL_SIZE
                                            : VERTEX_SIZE))
                   < MAX_VERTEX_BUFFER_SIZE)
            && canGlobalStoreVertices(amount, withNormals);
}

bool GlBuffer::canStoreIndices(
        size_t amount, enum ngsBufferType indexType) const
{
    int_fast32_t size;
    switch (indexType) {
        case BF_INDICES:
            size = m_indices.size();
            break;
        case BF_BORDER_INDICES:
            size = m_borderIndices.size();
            break;
        default:
            return false;
    }

    return ((size + amount) < MAX_INDEX_BUFFER_SIZE)
            && canGlobalStoreIndices(amount);
}

void GlBuffer::addVertex(
        float x, float y, float z, bool withNormals, float nX, float nY)
{
    if (!canStoreVertices(1, withNormals)) {
        return;
    }
    m_vertices.emplace_back(x);
    m_vertices.emplace_back(y);
    m_vertices.emplace_back(z);

    if (withNormals) {
        m_vertices.emplace_back(nX);
        m_vertices.emplace_back(nY);
    }

    m_globalVertexBufferSize.fetch_add(
            withNormals ? VERTEX_WITH_NORMAL_SIZE : VERTEX_SIZE);
}

void GlBuffer::addIndex(unsigned short index, enum ngsBufferType indexType)
{
    if (!canStoreIndices(1, indexType)) {
        return;
    }

    std::vector<GLushort>* pBuffer;
    std::atomic_int_fast32_t* pSize;

    switch (indexType) {
        case BF_INDICES:
            pBuffer = &m_indices;
            pSize = &m_globalIndexBufferSize;
            return;
        case BF_BORDER_INDICES:
            pBuffer = &m_borderIndices;
            pSize = &m_globalBorderIndexBufferSize;
            return;
        default:
            return;
    }

    pBuffer->emplace_back(index);
    pSize->fetch_add(1);
}

void GlBuffer::addTriangleIndices(unsigned short one,
        unsigned short two,
        unsigned short three,
        enum ngsBufferType indexType)
{
    if (!canStoreIndices(3, indexType)) {
        return;
    }

    std::vector<GLushort>* pBuffer;
    std::atomic_int_fast32_t* pSize;

    switch (indexType) {
        case BF_INDICES:
            pBuffer = &m_indices;
            pSize = &m_globalIndexBufferSize;
            break;
        case BF_BORDER_INDICES:
            pBuffer = &m_borderIndices;
            pSize = &m_globalBorderIndexBufferSize;
            break;
        default:
            return;
    }

    pBuffer->emplace_back(one);
    pBuffer->emplace_back(two);
    pBuffer->emplace_back(three);
    pSize->fetch_add(3);
}

size_t GlBuffer::getVertexBufferSize() const
{
    return m_bound ? m_finalVertexBufferSize : m_vertices.size();
}

size_t GlBuffer::getIndexBufferSize(enum ngsBufferType indexType) const
{
    switch (indexType) {
        case BF_INDICES:
            return m_bound ? m_finalIndexBufferSize : m_indices.size();
        case BF_BORDER_INDICES:
            return m_bound ? m_finalBorderIndexBufferSize
                           : m_borderIndices.size();
        default:
            return 0;
    }
}

//static
std::int_fast32_t GlBuffer::getGlobalVertexBufferSize()
{
    return m_globalVertexBufferSize.load();
}

//static
std::int_fast32_t GlBuffer::getGlobalIndexBufferSize(
        enum ngsBufferType indexType)
{
    switch (indexType) {
        case BF_INDICES:
            return m_globalIndexBufferSize.load();
        case BF_BORDER_INDICES:
            return m_globalBorderIndexBufferSize.load();
        default:
            return 0;
    }
}

//static
std::int_fast32_t GlBuffer::getGlobalHardBuffersCount()
{
    return m_globalHardBuffersCount.load();
}

GLuint GlBuffer::getGlHardBufferId(ngsBufferType type) const
{
    switch (type) {
        case BF_VERTICES:
            return m_glHardBufferIds[0];
        case BF_INDICES:
            return m_glHardBufferIds[1];
        case BF_BORDER_INDICES:
            return m_glHardBufferIds[2];
    }
    return GL_BUFFER_UNKNOWN;
}

//------------------------------------------------------------------------------
// GlBufferBucket
//------------------------------------------------------------------------------

GlBufferBucket::GlBufferBucket(int x,
        int y,
        unsigned char z,
        const OGREnvelope& env,
        char crossExtent)
        : m_X(x)
        , m_Y(y)
        , m_zoom(z)
        , m_extent(env)
        , m_filled(false)
        , m_crossExtent(crossExtent)
        , m_e1(-1)
        , m_e2(-1)
        , m_e3(-1)
{
    m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
}

GlBufferBucket::GlBufferBucket(GlBufferBucket&& other)
{
    *this = std::move(other);
}

GlBufferBucket::~GlBufferBucket()
{
    m_buffers.clear();
    m_buffers.shrink_to_fit();
}

GlBufferBucket& GlBufferBucket::operator=(GlBufferBucket&& other)
{
    if (this == &other) {
        return *this;
    }

    m_X = other.m_X;
    m_Y = other.m_Y;
    m_zoom = other.m_zoom;
    m_extent = other.m_extent;
    m_filled = other.m_filled;
    m_crossExtent = other.m_crossExtent;

    other.m_X = 0;
    other.m_Y = 0;
    other.m_zoom = 0;
    other.m_extent = OGREnvelope();
    other.m_filled = false;
    other.m_crossExtent = 0;

    m_buffers.clear();
    m_buffers.shrink_to_fit();
    m_buffers = std::move(other.m_buffers);

    m_fids.clear();
    m_fids = other.m_fids;
    other.m_fids.clear();

    return *this;
}

void GlBufferBucket::bind()
{
    for (const GlBufferSharedPtr& buff : m_buffers) {
        buff->bind();
    }
}

bool GlBufferBucket::bound() const
{
    for (const GlBufferSharedPtr& buff : m_buffers) {
        if (!buff->bound())
            return false;
    }
    return true;
}

bool GlBufferBucket::filled() const
{
    return m_filled;
}

void GlBufferBucket::setFilled(bool filled)
{
    m_filled = filled;
}

void GlBufferBucket::fill(GIntBig fid, OGRGeometry* geom, float level)
{
    if (nullptr == geom)
        return;
    std::int_fast32_t size = GlBuffer::getGlobalVertexBufferSize();
    fill(geom, level);
    if (0 < GlBuffer::getGlobalVertexBufferSize() - size) {
        m_fids.insert(fid);
    }
}

// TODO: add flags to specify how to fill buffer
void GlBufferBucket::fill(const OGRGeometry* geom, float level)
{
    switch (OGR_GT_Flatten(geom->getGeometryType())) {
        case wkbPoint: {
            const OGRPoint* pt = static_cast<const OGRPoint*>(geom);
            fillPoint(pt, level);
        } break;
        case wkbLineString: {
            const OGRLineString* line = static_cast<const OGRLineString*>(geom);
            fillLineString(line, level);
        } break;
        case wkbPolygon: {
            const OGRPolygon* polygon = static_cast<const OGRPolygon*>(geom);
//            fillPolygon(polygon, level);
            fillBorderedPolygon(polygon, level);
        } break;
        case wkbMultiPoint: {
            const OGRMultiPoint* mpt = static_cast<const OGRMultiPoint*>(geom);
            for (int i = 0; i < mpt->getNumGeometries(); ++i) {
                const OGRPoint* pt =
                        static_cast<const OGRPoint*>(mpt->getGeometryRef(i));
                fillPoint(pt, level);
            }
        } break;
        case wkbMultiLineString: {
            const OGRMultiLineString* mln =
                    static_cast<const OGRMultiLineString*>(geom);
            for (int i = 0; i < mln->getNumGeometries(); ++i) {
                const OGRLineString* line = static_cast<const OGRLineString*>(
                        mln->getGeometryRef(i));
                fillLineString(line, level);
            }
        } break;
        case wkbMultiPolygon: {
            const OGRMultiPolygon* mplg =
                    static_cast<const OGRMultiPolygon*>(geom);
            for (int i = 0; i < mplg->getNumGeometries(); ++i) {
                const OGRPolygon* polygon =
                        static_cast<const OGRPolygon*>(mplg->getGeometryRef(i));
//                fillPolygon(polygon, level);
                fillBorderedPolygon(polygon, level);
            }
        } break;
        case wkbGeometryCollection: {
            const OGRGeometryCollection* coll =
                    static_cast<const OGRGeometryCollection*>(geom);
            for (int i = 0; i < coll->getNumGeometries(); ++i) {
                fill(coll->getGeometryRef(i), level);
            }
        } break;
            /* TODO:
        case wkbCircularString:
            return "cir";
        case wkbCompoundCurve:
            return "ccrv";
        case wkbCurvePolygon:
            return "crvplg";
        case wkbMultiCurve:
            return "mcrv";
        case wkbMultiSurface:
            return "msurf";
        case wkbCurve:
            return "crv";
        case wkbSurface:
            return "surf";*/
    }
}

void GlBufferBucket::fillPoint(const OGRPoint* point, float level)
{
    if (!m_buffers.back()->canStoreVertices(1)
            || !m_buffers.back()->canStoreIndices(1)) {
        if (!GlBuffer::canGlobalStoreVertices(1)
                || !GlBuffer::canGlobalStoreIndices(1)) {
            return;
        }
        m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
    }

    GlBufferSharedPtr currBuffer = m_buffers.back();
    unsigned short startIndex = currBuffer->getIndexBufferSize();
    // TODO: add getZ + level
    currBuffer->addVertex(
            static_cast<float>(point->getX() + m_crossExtent * DEFAULT_MAX_X2),
            static_cast<float>(point->getY()), level);

    currBuffer->addIndex(startIndex);
}

void GlBufferBucket::fillLineString(const OGRLineString* line, float level)
{
    int numPoints = line->getNumPoints();
    if (numPoints < 2)
        return;

    // TODO: cut line by x or y direction or
    // tesselate to fill into array max size
    if (numPoints > 21000) {
        CPLDebug("GlBufferBucket", "Too many points - %d, need to divide",
                numPoints);
        return;
    }

    if (!m_buffers.back()->canStoreVertices(2 * numPoints, true)
            || !m_buffers.back()->canStoreIndices(6 * (numPoints - 1))) {
        if (!GlBuffer::canGlobalStoreVertices(2 * numPoints, true)
                || !GlBuffer::canGlobalStoreIndices(6 * (numPoints - 1))) {
            return;
        }
        m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
    }

    GlBufferSharedPtr currBuffer = m_buffers.back();
    LineStringFiller lf(line, level, m_crossExtent, LineCapType::Butt,
            LineJoinType::Bevel, currBuffer);

    for (int i = 0; i < numPoints; ++i) {
        lf.insertVertex(i);
    }
}

void GlBufferBucket::fillPolygon(const OGRPolygon* polygon, float level)
{
    PolygonTriangulator tr;
    tr.triangulate(polygon);

    if (!m_buffers.back()->canStoreVertices(tr.getNumVertices())
            || !m_buffers.back()->canStoreIndices(3 * tr.getNumTriangles())) {
        if (!GlBuffer::canGlobalStoreVertices(tr.getNumVertices())
                || !GlBuffer::canGlobalStoreIndices(3 * tr.getNumTriangles())) {
            return;
        }
        m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
    }

    GlBufferSharedPtr currBuffer = m_buffers.back();
    tr.insertVerticesToBuffer(level, m_crossExtent, currBuffer);
}

void GlBufferBucket::fillBorderedPolygon(const OGRPolygon* polygon, float level)
{
    PolygonTriangulator tr;
    size_t polygonPointIndex = 0;
    const int intRingCnt = polygon->getNumInteriorRings();
    for (int i = -1; i < intRingCnt; ++i) {
        const OGRLinearRing* ring;
        if (-1 == i) {
            ring = polygon->getExteriorRing();
        } else {
            ring = polygon->getInteriorRing(i);
        }

        int numPoints = ring->getNumPoints();
        if (numPoints < 3) {
            if (-1 == i)
                return;
            else
                continue;
        }

        // TODO: cut line by x or y direction or
        // tesselate to fill into array max size
        if (numPoints > 21000) {
            CPLDebug("GlBufferBucket", "Too many points - %d, need to divide",
                    numPoints);
            return;
        }

        polygonPointIndex = tr.insertConstraint(ring, polygonPointIndex);
    }

    // Mark facets that are inside the domain bounded by the polygon.
    tr.markDomains();

    int maxVerticesCnt = std::max(2 * polygonPointIndex, tr.getNumVertices());
    int maxIndicesCnt =
            std::max(6 * (polygonPointIndex - 1), 3 * tr.getNumTriangles());

    if (!m_buffers.back()->canStoreVertices(maxVerticesCnt, true)
            || !m_buffers.back()->canStoreIndices(maxIndicesCnt)) {
        if (!GlBuffer::canGlobalStoreVertices(maxVerticesCnt, true)
                || !GlBuffer::canGlobalStoreIndices(maxIndicesCnt)) {
            return;
        }
        m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
    }

    GlBufferSharedPtr currBuffer = m_buffers.back();
    std::vector<size_t> borderIndices(polygonPointIndex);
    for (int i = -1; i < intRingCnt; ++i) {
        const OGRLinearRing* ring;
        if (-1 == i) {
            ring = polygon->getExteriorRing();
        } else {
            ring = polygon->getInteriorRing(i);
        }

        int numPoints = ring->getNumPoints();
        if (numPoints < 3) {
            if (-1 == i)
                return;
            else
                continue;
        }

        LineStringFiller lf(ring, level, m_crossExtent, LineCapType::Butt,
                LineJoinType::Bevel, currBuffer);
        for (int i = 0; i < numPoints; ++i) {
            int vertexIndex = lf.insertVertex(i, BF_BORDER_INDICES);
            if (-1 == vertexIndex) {
                // TODO:
            }
            borderIndices.push_back(vertexIndex);
        }
    }

    tr.insertVerticesToBuffer(level, m_crossExtent, currBuffer, &borderIndices);
}

char GlBufferBucket::crossExtent() const
{
    return m_crossExtent;
}

void GlBufferBucket::draw(const Style& style)
{
    for (const GlBufferSharedPtr& buffer : m_buffers) {
        if(!buffer->bound())
            buffer->bind();
        style.draw(*buffer);
    }
}

int GlBufferBucket::X() const
{
    return m_X;
}

int GlBufferBucket::Y() const
{
    return m_Y;
}

unsigned char GlBufferBucket::zoom() const
{
    return m_zoom;
}

void GlBufferBucket::free()
{
    m_buffers.clear ();
    m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
}

bool GlBufferBucket::hasFid(GIntBig fid) const
{
    return m_fids.find(fid) != m_fids.end();
}

int GlBufferBucket::getFidCount() const
{
    return m_fids.size();
}

OGREnvelope GlBufferBucket::extent() const
{
    return m_extent;
}

bool GlBufferBucket::intersects(const GlBufferBucket &other) const
{
    return m_extent.Intersects (other.m_extent) == TRUE;
}

bool GlBufferBucket::intersects(const OGREnvelope &ext) const
{
    return m_extent.Intersects (ext) == TRUE;
}

size_t GlBufferBucket::getVertexBufferSize() const
{
    size_t count = 0;
    for(const GlBufferSharedPtr& buff : m_buffers) {
        count += buff->getVertexBufferSize();
    }
    return count;
}

size_t GlBufferBucket::getIndexBufferSize() const
{
    size_t count = 0;
    for(const GlBufferSharedPtr& buff : m_buffers) {
        count += buff->getIndexBufferSize();
    }
    return count;
}
