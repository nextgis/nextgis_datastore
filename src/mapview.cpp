/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
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
#include "mapview.h"
#include "api.h"

#include <iostream>
#include <limits>

#include <GLES2/gl2.h>
#include "EGL/egl.h"

using namespace ngs;
using namespace std;

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

#define THREAD_LOOP_SLEEP 0.1

const GLuint maxUint = numeric_limits<GLuint>::max();

void reportGlStatus(GLuint obj) {
    GLint length;
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
    GLchar *pLog = new GLchar[length];
    glGetShaderInfoLog(obj, length, &length, pLog);
    cerr << pLog << endl;
    delete [] pLog;
}

bool checkShaderCompileStatus(GLuint obj) {
    GLint status;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

bool checkProgramLinkStatus(GLuint obj) {
    GLint status;
    glGetProgramiv(obj, GL_LINK_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

GLuint prepareProgram() {

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
        return maxUint;

    const GLchar * const fragmentShaderSourcePtr = ""
          "#version 330 core\n"
          "out vec3 color;\n"
          "void main() { color = vec3(0,0,1); }\n";

    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderId, 1, &fragmentShaderSourcePtr, nullptr);
    glCompileShader(fragmentShaderId);

    if (!checkShaderCompileStatus(fragmentShaderId) )
        return maxUint;

    GLuint programId = glCreateProgram();
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);
    glLinkProgram(programId);

    if( !checkProgramLinkStatus(programId) )
        return maxUint;

    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);

    return programId;
}

void RenderingThread(void * view)
{
    MapView* pMapView = static_cast<MapView*>(view);
    EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        pMapView->setErrorCode(ngsErrorCodes::GL_GET_DISPLAY_FAILED);
        return;
    }

    EGLint major, minor;
    if (!eglInitialize(eglDisplay, &major, &minor)) {
        pMapView->setErrorCode(ngsErrorCodes::INIT_FAILED);
        return;
    }

    if ((major <= 1) && (minor < 1)) {
        pMapView->setErrorCode(ngsErrorCodes::UNSUPPORTED);
        return;
    }

#ifdef _DEBUG
    cout << "Vendor: " << eglQueryString(eglDisplay, EGL_VENDOR) << endl;
    cout << "Version: " << eglQueryString(eglDisplay, EGL_VERSION) << endl;
    cout << "Client APIs: " << eglQueryString(eglDisplay, EGL_CLIENT_APIS) << endl;
    cout << "Client Extensions: " << eglQueryString(eglDisplay, EGL_EXTENSIONS) << endl;
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
    EGLConfig eglConf;
    if(!eglChooseConfig(eglDisplay, confAttr, &eglConf, 1, &numConfigs)){
        pMapView->setErrorCode(ngsErrorCodes::INIT_FAILED);
        return;
    }

    EGLContext eglCtx = eglCreateContext(eglDisplay, eglConf, EGL_NO_CONTEXT, ctxAttr);
    if (eglCtx == EGL_NO_CONTEXT) {
        pMapView->setErrorCode(ngsErrorCodes::INIT_FAILED);
        return;
    }

    EGLSurface eglSurface = EGL_NO_SURFACE;

    // load shaders
    // TODO: need special class to load/undload and manage shaders
    GLuint progId = prepareProgram();
    if(maxUint == progId) {
        pMapView->setErrorCode(ngsErrorCodes::INIT_FAILED);
        return;
    }

    pMapView->setErrorCode(ngsErrorCodes::SUCCESS);
    pMapView->setDisplayInit (true);

    // start rendering loop here
    while(!pMapView->cancel ()){
        if(pMapView->isSizeChanged ()){
#ifdef _DEBUG
            cout << "Size changed" << endl;
#endif // _DEBUG
            eglDestroySurface(eglDisplay, eglSurface);

            // create a pixelbuffer surface
            // surface attributes
            // the surface size is set to the input frame size
            const EGLint surfaceAttr[] = {
                 EGL_WIDTH, pMapView->getDisplayWidht (),
                 EGL_HEIGHT, pMapView->getDisplayHeight (),
                 EGL_NONE
            };

            eglSurface = eglCreatePbufferSurface(eglDisplay, eglConf, surfaceAttr);
            if(EGL_NO_SURFACE != eglSurface){
                eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglCtx);
            }
            pMapView->setSizeChanged (false);
        }

        if(EGL_NO_SURFACE == eglSurface){
#ifdef _DEBUG
            cerr << "No surface to draw" << endl;
#endif //_DEBUG
            CPLSleep(THREAD_LOOP_SLEEP);
            continue;
        }

        switch(pMapView->getDrawStage ()){
        case MapView::DrawStage::Stop:
#ifdef _DEBUG
            cout << "MapView::DrawStage::Stop" << endl;
#endif // _DEBUG
            // stop extract data threads
            pMapView->setDrawStage (MapView::DrawStage::Start);
            break;
        case MapView::DrawStage::Process:
#ifdef _DEBUG
            cout << "MapView::DrawStage::Process" << endl;
#endif // _DEBUG
            // get ready portion of geodata to draw
            // fill buffer with pixels glReadPixels
            // and notify listeners

            // if no more geodata finish
            pMapView->setDrawStage (MapView::DrawStage::Done);
            // if notify return false - set stage to done
            pMapView->notify (1.0, nullptr);
            break;
        case MapView::DrawStage::Start:
#ifdef _DEBUG
            cout << "MapView::DrawStage::Start" << endl;
#endif // _DEBUG

            pMapView->setDrawStage (MapView::DrawStage::Process);
            {
            // clear
            ngsRGBA color = pMapView->getBackgroundColor();
            float dfR = float(color.R) / 255;
            float dfG = float(color.G) / 255;
            float dfB = float(color.B) / 255;
            float dfA = float(color.A) / 255;
            glClearColor(dfR, dfG, dfB, dfA);
            glClear(GL_COLOR_BUFFER_BIT);
            }
            pMapView->fillBuffer(eglDisplay, eglSurface);
            break;

        case MapView::DrawStage::Done:
            // if not tasks sleep 100ms
            CPLSleep(THREAD_LOOP_SLEEP);
        }
    }

#ifdef _DEBUG
    cout << "Exit draw thread" << endl;
#endif // _DEBUG

    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    glDeleteProgram(progId);
    eglDestroyContext(eglDisplay, eglCtx);
    eglDestroySurface(eglDisplay, eglSurface);
    eglTerminate ( eglDisplay );

    eglDisplay = EGL_NO_DISPLAY;
    eglSurface = EGL_NO_SURFACE;
    eglCtx = EGL_NO_CONTEXT;

    pMapView->setDisplayInit (false);
}

MapView::MapView(FeaturePtr feature, MapStore *mapstore) : Map(feature, mapstore),
    MapTransform(480, 640), m_displayInit(false), m_cancel(false),
    m_bufferData(nullptr), m_progressFunc(nullptr)
{
    initDisplay();
}

MapView::~MapView()
{
    m_cancel = true;
    // wait thread exit
    CPLJoinThread(m_hThread);
}

bool MapView::isDisplayInit() const
{
    return m_displayInit;
}

int MapView::initDisplay()
{
    m_hThread = CPLCreateJoinableThread(RenderingThread, this);
    return m_errorCode;
}

int MapView::errorCode() const
{
    return m_errorCode;
}

void MapView::setErrorCode(int errorCode)
{
    m_errorCode = errorCode;
}

void MapView::setDisplayInit(bool displayInit)
{
    m_displayInit = displayInit;
}

bool MapView::cancel() const
{
    return m_cancel;
}

void *MapView::getBufferData() const
{
    return m_bufferData;
}

int MapView::initBuffer(void *buffer, int width, int height)
{
    m_bufferData = buffer;

    setDisplaySize (width, height);

    return ngsErrorCodes::SUCCESS;
}

int MapView::draw(const ngsProgressFunc &progressFunc, void* progressArguments)
{
    m_progressFunc = progressFunc;
    m_progressArguments = progressArguments;

    if(m_drawStage == DrawStage::Process){
        m_drawStage = DrawStage::Stop;
    }
    else {
        m_drawStage = DrawStage::Start;
    }

    // 1.   Finish drawing if any and put scene to buffer
    // 1.1. Stop extract data threads
    // 1.2. Put current drawn scene to buffer
    // 1.3. Notify drawing progress
    // 2.   Start new drawing
    // 2.1. Start extract data for current extent
    // 2.2. Put partially or full scene to buffer
    // 2.3. Notify drawing progress

    return ngsErrorCodes::SUCCESS;
}

int MapView::notify(double complete, const char *message)
{
    if(nullptr != m_progressFunc) {
        return m_progressFunc(complete, message, m_progressArguments);
    }
    return TRUE;
}

void MapView::fillBuffer(void* pDisplay, void* pSurface)
{
    eglSwapBuffers(pDisplay, pSurface);
    glReadPixels(0, 0, getDisplayWidht (),
                 getDisplayHeight (), GL_RGBA,
                 GL_UNSIGNED_BYTE, m_bufferData);
}

enum MapView::DrawStage MapView::getDrawStage() const
{
    return m_drawStage;
}

void MapView::setDrawStage(const DrawStage &drawStage)
{
    m_drawStage = drawStage;
}



