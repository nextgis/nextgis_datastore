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

%header %{
typedef struct {
    JNIEnv *jenv;
    jobject pJavaCallback;
} JavaProgressData;
%}

%inline %{
class ProgressCallback
{
public:
    virtual ~ProgressCallback() {  }
    virtual int run(double complete, const char* message)
    {
        return 1;
    }
};

class NotificationCallback
{
public:
    virtual ~NotificationCallback() {  }
    virtual void run(enum ngsSourceCodes src, const char* table, long row, enum ngsChangeCodes operation)
    {
    }
};
%}

%{
int
JavaProgressProxy( double complete, const char *message, void *progressArguments )
{
    JavaProgressData* psProgressInfo = (JavaProgressData*)progressArguments;
    JNIEnv *jenv = psProgressInfo->jenv;
    int ret;
    const jclass progressCallbackClass = jenv->FindClass("com/nextgis/store/ProgressCallback");
    const jmethodID runMethod = jenv->GetMethodID(progressCallbackClass, "run", "(DLjava/lang/String;)I");
    jstring tempString = jenv->NewStringUTF(message);
    ret = jenv->CallIntMethod(psProgressInfo->pJavaCallback, runMethod, complete, tempString);
    jenv->DeleteLocalRef(tempString);
    return ret;
}

void
JavaNotificationProxy(enum ngsSourceCodes src, const char* table, long row, enum ngsChangeCodes operation, void *progressArguments)
{
    JavaProgressData* psProgressInfo = (JavaProgressData*)progressArguments;
    JNIEnv *jenv = psProgressInfo->jenv;
    int ret;
    const jclass notificationCallbackClass = jenv->FindClass("com/nextgis/store/NotificationCallback");
    const jmethodID runMethod = jenv->GetMethodID(notificationCallbackClass, "run", "(DLjava/lang/String;)I");
    jstring tempString = jenv->NewStringUTF(table);
    jenv->CallVoidMethod(psProgressInfo->pJavaCallback, runMethod, src, tempString, row, operation);
    jenv->DeleteLocalRef(tempString);
}
%}
