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

%module(directors="1") Api

%feature("director") ProgressCallback;
%feature("director") NotificationCallback;

%typemap(in) ( ngsProgressFunc callback = NULL, void* callbackData = NULL)
{
    if ( $input != 0 ) {
        $1 = JavaProgressProxy;
        $2 = (void *) $input;
    }
    else
    {
        $1 = NULL;
        $2 = NULL;
    }
}

%typemap(jni) (ngsProgressFunc callback = NULL, void* callbackData = NULL)  "jlong"
%typemap(jtype) (ngsProgressFunc callback = NULL, void* callbackData = NULL)  "long"
%typemap(jstype) (ngsProgressFunc callback = NULL, void* callbackData = NULL)  "ProgressCallback"
%typemap(javain) (ngsProgressFunc callback = NULL, void* callbackData = NULL)  "ProgressCallback.getCPtr($javainput)"
%typemap(javaout) (ngsProgressFunc callback = NULL, void* callbackData = NULL)
{
    return $jnicall;
}

// test with java directors
//%typemap(javadirectorin) (double complete) "$jniinput"
//%typemap(javadirectorout) (int) {
//    return $javacall;
//}
//%typemap(directorin,descriptor="Lcom/nextgis/store/ProgressCallback;") (double complete) {
//    $input = (jobject) $2;
//}
//%typemap(directorout) (int) {
//    $result = (int)$input;
//}


%typemap(in) ( ngsNotifyFunc callback = NULL )
{
    if ( $input != 0 ) {
// TODO
//        $1 = JavaNotificationProxy;
//        $2 = (void *) $input;
    }
    else
    {
// TODO
//        $1 = NULL;
//        $2 = NULL;
    }
}

%typemap(jni) (ngsNotifyFunc callback = NULL)  "jlong"
%typemap(jtype) (ngsNotifyFunc callback = NULL)  "long"
%typemap(jstype) (ngsNotifyFunc callback = NULL)  "NotificationCallback"
%typemap(javain) (ngsNotifyFunc callback = NULL)  "NotificationCallback.getCPtr($javainput)"
%typemap(javaout) (ngsNotifyFunc callback = NULL)
{
    return $jnicall;
}
