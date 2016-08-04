/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 32 of the License, or
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
 
/* File : api.i */

%module Api
%{
#include "api.h"
%}

#ifdef SWIGJAVA
%include typemap_java.i
%include callback_java.i
#endif

#ifdef SWIGPYTHON

#endif

%inline %{
extern int ngsGetVersion(const char* request);
extern const char* ngsGetVersionString(const char* request);
extern int ngsInit(const char* dataPath, const char* cachePath);
extern void ngsUninit();
extern void ngsOnLowMemory();
extern void ngsOnPause();
extern void ngsSetNotifyFunction(ngsNotifyFunc callback);
extern int ngsInitDataStore(const char* path);
extern int ngsDestroyDataStore(const char* path, const char* cachePath);
extern int ngsCreateMap(const char* name, const char* description,
                             unsigned short epsg, double minX, double minY,
                             double maxX, double maxY);
extern int ngsOpenMap(const char* path);
extern int ngsSaveMap(unsigned int mapId, const char* path);
extern int ngsInitMap(unsigned int mapId, void * nioBuffer, int width,
                      int height, int isYAxisInverted);
extern int ngsDrawMap(unsigned int mapId, ngsProgressFunc callback,
                      void* callbackData);
extern int ngsSetMapBackgroundColor(unsigned int mapId, unsigned char R,
                                    unsigned char B, unsigned char G,
                                    unsigned char A);
extern ngsRGBA ngsGetMapBackgroundColor(unsigned int mapId);

extern int ngsSetMapDisplayCenter(unsigned int mapId, int x, int y);
extern int ngsGetMapDisplayCenter(unsigned int mapId, int &x, int &y);
extern int ngsSetMapScale(unsigned int mapId, double scale);
extern int ngsGetMapScale(unsigned int mapId, double &scale);
%}

%clear const char *request, const char *name, const char* path,
const char* cachePath, const char* dataPath, const char* description;

#ifdef SWIGJAVA

// exampe of adding import declaration
//%pragma(java) jniclassimports=%{
//import com.nextgis.store.NotificationCallback;
//%}

// exampe of adding import declaration
//%pragma(java) moduleimports=%{
//import com.nextgis.store.NotificationCallback;
//%}

%pragma(java) jniclasscode=%{
  static {
    try {
        System.loadLibrary("ngstore");
        System.loadLibrary("ngstoreapi");
    } catch (UnsatisfiedLinkError e) {
      System.err.println("Native code library failed to load. \n" + e);
      System.exit(1);
    }
  }
%}

// exampe of adding fuction to Api class
%pragma(java) modulecode=%{
  public static int ngsInitMap2(String name, java.nio.ByteBuffer nioBuffer, int width, int height) {
    if(null == nioBuffer)
       throw new NullPointerException("Needed not null buffer");
    if(width * height > nioBuffer.capacity())
        throw new IllegalArgumentException("Output buffer too small. Should be greater than width * height");
    return ngsInitMap(name, nioBuffer, width, height);
  }
%}

#endif
