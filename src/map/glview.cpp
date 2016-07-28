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
#include "constants.h"

#include <iostream>
#include "cpl_error.h"
#include "cpl_string.h"

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

using namespace ngs;

GlView::GlView() : m_eglDisplay(EGL_NO_DISPLAY), m_eglCtx(EGL_NO_CONTEXT),
    m_eglSurface(EGL_NO_SURFACE), m_programId(0), m_displayWidth(100),
    m_displayHeight(100), m_extensionLoad(false), m_programLoad(false),
    m_defaultFramebuffer(0)
{
    memset(m_renderbuffers, 0, sizeof(m_renderbuffers));
}

GlView::~GlView()
{
    // FIXME: egl display and config should be common for all maps
    //eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if(m_programId)
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

    // EGL context attributes
    const EGLint ctxAttr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,              // very important!
        EGL_NONE
    };

    m_eglCtx = eglCreateContext(m_eglDisplay, m_eglConf, EGL_NO_CONTEXT, ctxAttr);
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

#ifdef _DEBUG
    cout << "Size changed" << endl;
#endif // _DEBUG

    if(m_eglSurface != EGL_NO_SURFACE)
        eglDestroySurface( m_eglDisplay, m_eglSurface );

    destroyFBO ();

    // create a pixelbuffer surface
    // surface attributes
    // the surface size is set to the input frame size
    const EGLint surfaceAttr[] = {
         EGL_WIDTH, width,
         EGL_HEIGHT, height,
         EGL_LARGEST_PBUFFER, EGL_TRUE,
         EGL_NONE
    };

    // NOTE: need to create both pbuffer and FBO to draw into offscreen buffer
    // see: http://stackoverflow.com/q/28817777/2901140

    m_eglSurface = eglCreatePbufferSurface( m_eglDisplay, m_eglConf, surfaceAttr);
    if(EGL_NO_SURFACE != m_eglSurface){

#ifdef _DEBUG
        checkEGLError("eglCreatePbufferSurface");
#endif // _DEBUG

        if(!eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglCtx)) {
            CPLError(CE_Failure, CPLE_OpenFailed, "eglMakeCurrent failed.");
            return;
        }

        m_displayWidth = width;
        m_displayHeight = height;

#ifdef _DEBUG
        EGLint w, h;
        eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_WIDTH, &w);
        checkEGLError ("eglQuerySurface");
        cout << "EGL_WIDTH: " << w << endl;
        eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_HEIGHT, &h);
        checkEGLError ("eglQuerySurface");
        cout << "EGL_HEIGHT: " << h << endl;
#endif // _DEBUG

        if(!createFBO (width, height)){
            CPLError(CE_Failure, CPLE_OpenFailed, "createFBO failed.");
            return;
        }

/*        if(!m_extensionLoad){
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

        if(!m_programLoad) {
            // TODO: need special class to load/undload and manage shaders
            m_programId = prepareProgram();
            if(!m_programId) {
                CPLError(CE_Failure, CPLE_OpenFailed, "Prepare program (shaders) failed.");
                return;
            }
            m_programLoad = true;
        }
        ngsCheckGLEerror(glClearColor(m_bkColor.r, m_bkColor.g, m_bkColor.b,
                                       m_bkColor.a));
    }
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
    ngsCheckEGLEerror(eglSwapBuffers(m_eglDisplay, m_eglSurface));
    ngsCheckGLEerror(glReadPixels(0, 0, m_displayWidth, m_displayHeight,
                                   GL_RGBA, GL_UNSIGNED_BYTE, buffer));
}

void GlView::clearBackground()
{    
    ngsCheckGLEerror(glClear(GL_COLOR_BUFFER_BIT));
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
    GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                          };

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices ));
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

bool GlView::checkEGLError(const char *cmd) const {
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

bool GlView::checkGLError(const char *cmd) const {
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

void GlView::reportGlStatus(GLuint obj) const {
    GLint length;
    ngsCheckGLEerror(glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length));
    GLchar *log = new GLchar[length];
    ngsCheckGLEerror(glGetShaderInfoLog(obj, length, &length, log));
    CPLError(CE_Failure, CPLE_AppDefined, "%s", log);
    delete [] log;
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
            "precision mediump float;\n"
            "void main()                                  \n"
            "{                                            \n"
            "  gl_FragColor = vec4 ( 1.0, 0.0, 0.0, 1.0 );\n"
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

void GlView::destroyFBO()
{
    ngsCheckGLEerror(glDeleteFramebuffers(1, &m_defaultFramebuffer));
    ngsCheckGLEerror(glDeleteRenderbuffers(ARRAY_SIZE(m_renderbuffers),
                                           m_renderbuffers));
}

bool GlView::createFBO(int width, int height)
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
