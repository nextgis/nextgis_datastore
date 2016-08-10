/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
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

%module Api

%typemap(arginit, noblock=1) ( ngsProgressFunc callback = NULL, void* callbackData = NULL)
{
    JavaProgressData sProgressInfo;
    sProgressInfo.jenv = jenv;
    sProgressInfo.pJavaCallback = NULL;

}

%typemap(in) ( ngsProgressFunc callback = NULL, void* callbackData = NULL)
{
    if ( $input != 0 ) {
        sProgressInfo.pJavaCallback = $input;
        $1 = JavaProgressProxy;
        $2 = &sProgressInfo;
    }
    else
    {
        $1 = NULL;
        $2 = NULL;
    }
}


/* These 3 typemaps tell SWIG what JNI and Java types to use */
%typemap(jni) (ngsProgressFunc callback = NULL, void* callbackData = NULL)  "jobject"
%typemap(jtype) (ngsProgressFunc callback = NULL, void* callbackData = NULL)  "ProgressCallback"
%typemap(jstype) (ngsProgressFunc callback = NULL, void* callbackData = NULL)  "ProgressCallback"
%typemap(javain) (ngsProgressFunc callback = NULL, void* callbackData = NULL)  "$javainput"
%typemap(javaout) (ngsProgressFunc callback = NULL, void* callbackData = NULL) {
    return $jnicall;
  }


%typemap(arginit, noblock=1) ( ngsNotifyFunc callback = NULL, void* callbackData = NULL)
{
    JavaProgressData sProgressInfo;
    sProgressInfo.jenv = jenv;
    sProgressInfo.pJavaCallback = NULL;

}

%typemap(in) ( ngsNotifyFunc callback = NULL, void* callbackData = NULL)
{
    if ( $input != 0 ) {
        sProgressInfo.pJavaCallback = $input;
        $1 = JavaNotificationProxy;
        $2 = &sProgressInfo;
    }
    else
    {
        $1 = NULL;
        $2 = NULL;
    }
}


/* These 3 typemaps tell SWIG what JNI and Java types to use */
%typemap(jni) (ngsNotifyFunc callback = NULL)  "jobject"
%typemap(jtype) (ngsNotifyFunc callback = NULL)  "NotificationCallback"
%typemap(jstype) (ngsNotifyFunc callback = NULL)  "NotificationCallback"
%typemap(javain) (ngsNotifyFunc callback = NULL)  "$javainput"
%typemap(javaout) (ngsNotifyFunc callback = NULL) {
    return $jnicall;
  }
