/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2018 NextGIS, <info@nextgis.com>
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

#include <jni.h>
#include <vector>
#include <cpl_json.h>

#include "ngstore/api.h"
#include "cpl_string.h"

#define NGS_JNI_FUNC(type, name) extern "C" JNIEXPORT type JNICALL Java_com_nextgis_maplib_API_ ## name


constexpr jboolean NGS_JNI_TRUE = 1;
constexpr jboolean NGS_JNI_FALSE = 0;
static JavaVM *g_vm;

class jniString
{
public:
    jniString(JNIEnv *env, jstring str) : m_env(env), m_original(str) {
        jboolean iscopy;
        m_native = env->GetStringUTFChars(str, &iscopy);
    }

    ~jniString() {
        m_env->ReleaseStringUTFChars(m_original, m_native);
    }

    const char *c_str() const { return m_native; }

private:
    JNIEnv *m_env;
    const char *m_native;
    jstring m_original;
};

static jclass g_APIClass;
static jmethodID g_NotifyMid;
static jmethodID g_ProgressMid;
static jclass g_StringClass;
static jclass g_EnvelopeClass;
static jmethodID g_EnvelopeInitMid;
static jclass g_PointClass;
static jmethodID g_PointInitMid;
static jclass g_CatalogObjectInfoClass;
static jmethodID g_CatalogObjectInfoInitMid;
static jclass g_FieldClass;
static jmethodID g_FieldInitMid;
static jclass g_DateComponentsClass;
static jmethodID g_DateComponentsInitMid;
static jclass g_EditOperationClass;
static jmethodID g_EditOperationInitMid;
static jclass g_RequestResultClass;
static jmethodID g_RequestResultInitMid;
static jclass g_RequestResultJsonClass;
static jmethodID g_RequestResultJsonInitMid;
static jclass g_RequestResultRawClass;
static jmethodID g_RequestResultRawInitMid;
static jclass g_AttachmentClass;
static jmethodID g_AttachmentInitMid;

//static jclass g_Class;
//static jmethodID g_InitMid;

static void notifyProxyFunc(const char *uri, enum ngsChangeCode operation)
{
    JNIEnv * g_env;
    // double check it's all ok
    int getEnvStat = g_vm->GetEnv((void **)&g_env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&g_env, nullptr) != 0) {
            return;
        }
    } else if (getEnvStat == JNI_EVERSION) {
        return;
    }

    jstring stringUri = g_env->NewStringUTF(uri);
    g_env->CallStaticVoidMethod(g_APIClass, g_NotifyMid, stringUri, operation);
    g_env->DeleteLocalRef(stringUri);

    if (g_env->ExceptionCheck()) {
        g_env->ExceptionDescribe();
    }

    g_vm->DetachCurrentThread();
}

static int progressProxyFunc(enum ngsCode status, double complete,
                             const char *message, void *progressArguments)
{
    JNIEnv * g_env;
    // double check it's all ok
    int getEnvStat = g_vm->GetEnv((void **)&g_env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&g_env, nullptr) != 0) {
            return 1;
        }
    } else if (getEnvStat == JNI_EVERSION) {
        return 1;
    }

    jstring stringMessage = g_env->NewStringUTF(message);
    long long progressArgumentsDigit = reinterpret_cast<long long>(progressArguments);
    jint res = g_env->CallStaticIntMethod(g_APIClass, g_ProgressMid, status, complete, message,
                                          static_cast<jint>(progressArgumentsDigit));
    g_env->DeleteLocalRef(stringMessage);

    if (g_env->ExceptionCheck()) {
        g_env->ExceptionDescribe();
    }

    g_vm->DetachCurrentThread();

    return res;
}

static char **toOptions(JNIEnv *env, jobjectArray optionsArray)
{
    int count = env->GetArrayLength(optionsArray);
    char **nativeOptions = nullptr;
    for (int i = 0; i < count; ++i) {
        jstring option = (jstring) (env->GetObjectArrayElement(optionsArray, i));
        nativeOptions = CSLAddString(nativeOptions, jniString(env, option).c_str());
    }

    return nativeOptions;
}

static jobjectArray fromOptions(JNIEnv *env, CSLConstList options)
{
    int count = CSLCount(options);
    jobjectArray result = env->NewObjectArray(count, g_StringClass, env->NewStringUTF(""));
    for(int i = 0; i < count; ++i) {
        env->SetObjectArrayElement(result, i, env->NewStringUTF(options[i]));
    }
    return result;
}

NGS_JNI_FUNC(jint, getVersion)(JNIEnv *env, jobject /* this */, jstring request)
{
    return ngsGetVersion(jniString(env, request).c_str());
}

NGS_JNI_FUNC(jstring, getVersionString)(JNIEnv *env, jobject /* this */, jstring request)
{
    const char *result = ngsGetVersionString(jniString(env, request).c_str());
    return env->NewStringUTF(result);
}

NGS_JNI_FUNC(void, unInit)(JNIEnv *env, jobject /* this */)
{
    ngsUnInit();

    env->DeleteGlobalRef(g_APIClass);
    env->DeleteGlobalRef(g_StringClass);
    env->DeleteGlobalRef(g_PointClass);
    env->DeleteGlobalRef(g_EnvelopeClass);
    env->DeleteGlobalRef(g_CatalogObjectInfoClass);
    env->DeleteGlobalRef(g_FieldClass);
    env->DeleteGlobalRef(g_EditOperationClass);
    env->DeleteGlobalRef(g_RequestResultClass);
    env->DeleteGlobalRef(g_RequestResultJsonClass);
    env->DeleteGlobalRef(g_RequestResultRawClass);
    env->DeleteGlobalRef(g_AttachmentClass);
}

static bool getClassInitMethod(JNIEnv *env, const char *className, const char *signature,
                               jclass &classVar, jmethodID &methodVar)
{
    jclass clazz = env->FindClass(className);
    classVar = static_cast<jclass>(env->NewGlobalRef(clazz));
    methodVar = env->GetMethodID(classVar, "<init>", signature);

    return methodVar != nullptr;
}

NGS_JNI_FUNC(jboolean, init)(JNIEnv *env, jobject /* this */, jobjectArray optionsArray)
{
    env->GetJavaVM(&g_vm);

    char **nativeOptions = toOptions(env, optionsArray);
    int result = ngsInit(nativeOptions);
    CSLDestroy(nativeOptions);

    jclass clazz = env->FindClass("com/nextgis/maplib/API");
    g_APIClass = static_cast<jclass>(env->NewGlobalRef(clazz));

    clazz = env->FindClass("java/lang/String");
    g_StringClass = static_cast<jclass>(env->NewGlobalRef(clazz));

    // register callback function: notify
    g_NotifyMid = env->GetStaticMethodID(g_APIClass, "notifyBridgeFunction", "(Ljava/lang/String;I)V");
    if(g_NotifyMid == nullptr) {
        return NGS_JNI_FALSE;
    }

    // register callback function: progress
    g_ProgressMid = env->GetStaticMethodID(g_APIClass, "progressBridgeFunction", "(IDLjava/lang/String;I)I");
    if(g_ProgressMid == nullptr) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/Point", "(DD)V", g_PointClass, g_PointInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/Envelope", "(DDDD)V", g_EnvelopeClass, g_EnvelopeInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/CatalogObjectInfo", "(Ljava/lang/String;IJ)V", g_CatalogObjectInfoClass, g_CatalogObjectInfoInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/Field", "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V", g_FieldClass, g_FieldInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/DateComponents", "(IIIIIII)V", g_DateComponentsClass, g_DateComponentsInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/EditOperation", "(JJJJI)V", g_EditOperationClass, g_EditOperationInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/RequestResult", "(ILjava/lang/String;)V", g_RequestResultClass, g_RequestResultInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/RequestResultJsonInt", "(IJ)V", g_RequestResultJsonClass, g_RequestResultJsonInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/RequestResultRaw", "(I[B)V", g_RequestResultRawClass, g_RequestResultRawInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/Attachment", "(JJLjava/lang/String;Ljava/lang/String;Ljava/lang/String;JJ)V", g_AttachmentClass, g_AttachmentInitMid)) {
        return NGS_JNI_FALSE;
    }

    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(void, freeResources)(JNIEnv /* *env */, jobject /* this */, jboolean full)
{
    ngsFreeResources(static_cast<char>(full ? 1 : 0));
}

NGS_JNI_FUNC(jstring, getLastErrorMessage)(JNIEnv *env, jobject /* this */)
{
    return env->NewStringUTF(ngsGetLastErrorMessage());
}

NGS_JNI_FUNC(jstring, settingsGetString)(JNIEnv *env, jobject /* this */, jstring key, jstring defaultVal)
{
    const char *result = ngsSettingsGetString(jniString(env, key).c_str(),
                                              jniString(env, defaultVal).c_str());
    return env->NewStringUTF(result);
}

NGS_JNI_FUNC(void, settingsSetString)(JNIEnv *env, jobject /* this */, jstring key, jstring value)
{
    ngsSettingsSetString(jniString(env, key).c_str(), jniString(env, value).c_str());
}

/**
 * Proxy to GDAL functions
 */

NGS_JNI_FUNC(jstring, getCurrentDirectory)(JNIEnv *env, jobject /* this */)
{
    return env->NewStringUTF(ngsGetCurrentDirectory());
}

//NGS_EXTERNC char **ngsListAddNameValue(char **list, const char *name,
//                                   const char *value);
//NGS_EXTERNC void ngsListFree(char **list);
NGS_JNI_FUNC(jstring, formFileName)(JNIEnv *env, jobject /* this */, jstring path, jstring name, jstring extension)
{
    return env->NewStringUTF(ngsFormFileName(jniString(env, path).c_str(), jniString(env, name).c_str(),
                             jniString(env, extension).c_str()));
}

NGS_JNI_FUNC(void, free)(JNIEnv /* *env */, jobject /* this */, jlong pointer)
{
    ngsFree(reinterpret_cast<void *>(pointer));
}

/**
 * Miscellaneous functions
 */

NGS_JNI_FUNC(jobject, URLRequest)(JNIEnv *env, jobject /* this */, jint type, jstring url, jobjectArray options)
{
    ngsURLRequestResult *result = ngsURLRequest(static_cast<ngsURLRequestType>(type),
                                                jniString(env, url).c_str(), toOptions(env, options));
    int status = result->status;
    std::string value(reinterpret_cast<const char*>(result->data),
                      static_cast<unsigned long>(result->dataLen));
    jstring outStr = env->NewStringUTF(value.c_str());
    ngsURLRequestResultFree(result);

    jvalue args[2];
    args[0].i = status;
    args[1].l = outStr;
    return env->NewObjectA(g_RequestResultClass, g_RequestResultInitMid, args);
}

NGS_JNI_FUNC(jobject, URLRequestJson)(JNIEnv *env, jobject /* this */, jint type, jstring url, jobjectArray options)
{
    ngsURLRequestResult *result = ngsURLRequest(static_cast<ngsURLRequestType>(type),
                                                jniString(env, url).c_str(), toOptions(env, options));
    int status = result->status;
    CPLJSONDocument doc;
    long handle = 0;
    if(doc.LoadMemory(result->data, result->dataLen)) {
        handle = reinterpret_cast<long>(new CPLJSONObject(doc.GetRoot()));
    }
    ngsURLRequestResultFree(result);

    jvalue args[2];
    args[0].i = status;
    args[1].j = handle;
    return env->NewObjectA(g_RequestResultJsonClass, g_RequestResultJsonInitMid, args);
}

NGS_JNI_FUNC(jobject, URLRequestRaw)(JNIEnv *env, jobject /* this */, jint type, jstring url, jobjectArray options)
{
    ngsURLRequestResult *result = ngsURLRequest(static_cast<ngsURLRequestType>(type),
                                                jniString(env, url).c_str(), toOptions(env, options));
    int status = result->status;
    jbyteArray barray = env->NewByteArray(result->dataLen);
    env->SetByteArrayRegion(barray, 0, result->dataLen,
                            reinterpret_cast<const jbyte *>(result->data));
    ngsURLRequestResultFree(result);

    jvalue args[2];
    args[0].i = status;
    args[1].l = barray;
    return env->NewObjectA(g_RequestResultRawClass, g_RequestResultRawInitMid, args);
}

NGS_JNI_FUNC(jobject, URLUploadFile)(JNIEnv *env, jobject /* this */, jstring path, jstring url,
                                     jobjectArray options, jint callbackId)
{
    ngsURLRequestResult *result;

    if(callbackId == 0) {
        result = ngsURLUploadFile(jniString(env, path).c_str(), jniString(env, url).c_str(),
                                  toOptions(env, options), nullptr, nullptr);
    }
    else {
        result = ngsURLUploadFile(jniString(env, path).c_str(), jniString(env, url).c_str(),
                                  toOptions(env, options), progressProxyFunc, reinterpret_cast<void *>(callbackId));
    }

    CPLJSONDocument doc;
    long handle = 0;
    int status = result->status;
    if(doc.LoadMemory(result->data, result->dataLen)) {
        handle = reinterpret_cast<long>(new CPLJSONObject(doc.GetRoot()));
    }
    ngsURLRequestResultFree(result);

    jvalue args[2];
    args[0].i = status;
    args[1].j = handle;
    return env->NewObjectA(g_RequestResultJsonClass, g_RequestResultJsonInitMid, args);
}

NGS_JNI_FUNC(jboolean, URLAuthAdd)(JNIEnv *env, jobject /* this */, jstring url, jobjectArray options)
{
    return ngsURLAuthAdd(jniString(env, url).c_str(), toOptions(env, options)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobject, URLAuthGet)(JNIEnv *env, jobject /* this */, jstring url)
{
    char **properties = ngsURLAuthGet(jniString(env, url).c_str());
    jobject ret = fromOptions(env, reinterpret_cast<CSLConstList>(properties));
    ngsListFree(properties);
    return ret;
}

NGS_JNI_FUNC(jboolean, URLAuthDelete)(JNIEnv *env, jobject /* this */, jstring url)
{
    return ngsURLAuthDelete(jniString(env, url).c_str()) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jstring, md5)(JNIEnv *env, jobject /* this */, jstring value)
{
    return env->NewStringUTF(ngsMD5(jniString(env, value).c_str()));
}

NGS_JNI_FUNC(jlong, jsonDocumentCreate)(JNIEnv /* *env */, jobject /* this */)
{
    return reinterpret_cast<jlong>(ngsJsonDocumentCreate());
}

NGS_JNI_FUNC(void, jsonDocumentFree)(JNIEnv /* *env */, jobject /* this */, jlong document)
{
    ngsJsonDocumentFree(reinterpret_cast<JsonDocumentH>(document));
}

NGS_JNI_FUNC(jboolean, jsonDocumentLoadUrl)(JNIEnv *env, jobject /* this */, jlong document,
                                            jstring url, jobjectArray options, jint callbackId)
{
    char **nativeOptions = toOptions(env, options);
    int result;
    if(callbackId == 0) {
        result = ngsJsonDocumentLoadUrl(reinterpret_cast<JsonDocumentH>(document),
                                        jniString(env, url).c_str(), nativeOptions, nullptr, nullptr);
    }
    else {
        result = ngsJsonDocumentLoadUrl(reinterpret_cast<JsonDocumentH>(document),
                                        jniString(env, url).c_str(), nativeOptions,
                                        progressProxyFunc, reinterpret_cast<void *>(callbackId));
    }
    CSLDestroy(nativeOptions);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, jsonDocumentRoot)(JNIEnv /* *env */, jobject /* this */, jlong document)
{
    return reinterpret_cast<jlong>(ngsJsonDocumentRoot(reinterpret_cast<JsonDocumentH>(document)));
}

NGS_JNI_FUNC(void, jsonObjectFree)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    ngsJsonObjectFree(reinterpret_cast<JsonObjectH>(object));
}

NGS_JNI_FUNC(jint, jsonObjectType)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return ngsJsonObjectType(reinterpret_cast<JsonObjectH>(object));
}

NGS_JNI_FUNC(jboolean, jsonObjectValid)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return ngsJsonObjectValid(reinterpret_cast<JsonObjectH>(object)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jstring, jsonObjectName)(JNIEnv *env, jobject /* this */, jlong object)
{
    return env->NewStringUTF(ngsJsonObjectName(reinterpret_cast<JsonObjectH>(object)));
}

NGS_JNI_FUNC(jlongArray, jsonObjectChildren)(JNIEnv *env, jobject /* this */, jlong object)
{
    jlongArray result;
    JsonObjectH *out = ngsJsonObjectChildren(reinterpret_cast<JsonObjectH>(object));
    std::vector<long> obArray;
    int counter = 0;
    while(out[counter] != nullptr) {
        obArray.push_back(reinterpret_cast<long>(out[counter]));
        counter++;
    }
    ngsJsonObjectChildrenListFree(out);

    result = env->NewLongArray(static_cast<jsize>(obArray.size()));
    if(result) {
        env->SetLongArrayRegion(result, 0, static_cast<jsize>(obArray.size()),
                                reinterpret_cast<const jlong*>(obArray.data()));
    }

    return result;
}

NGS_JNI_FUNC(jstring, jsonObjectGetString)(JNIEnv *env, jobject /* this */, jlong object,
                                            jstring defaultValue)
{
    return env->NewStringUTF(ngsJsonObjectGetString(reinterpret_cast<JsonObjectH>(object),
                                  jniString(env, defaultValue).c_str()));
}

NGS_JNI_FUNC(jdouble, jsonObjectGetDouble)(JNIEnv /* *env */, jobject /* this */, jlong object,
                                            jdouble defaultValue)
{
    return ngsJsonObjectGetDouble(reinterpret_cast<JsonObjectH>(object),
                                   defaultValue);
}

NGS_JNI_FUNC(jint, jsonObjectGetInteger)(JNIEnv /* *env */, jobject /* this */, jlong object,
                                          jint defaultValue)
{
    return ngsJsonObjectGetInteger(reinterpret_cast<JsonObjectH>(object),
                                defaultValue);
}

NGS_JNI_FUNC(jlong, jsonObjectGetLong)(JNIEnv /* *env */, jobject /* this */, jlong object,
                                          jlong defaultValue)
{
    return ngsJsonObjectGetLong(reinterpret_cast<JsonObjectH>(object),
                                defaultValue);
}

NGS_JNI_FUNC(jboolean, jsonObjectGetBool)(JNIEnv /* *env */, jobject /* this */, jlong object,
                                                jboolean defaultValue)
{
    return ngsJsonObjectGetBool(reinterpret_cast<JsonObjectH>(object),
                                      defaultValue) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, jsonObjectGetArray)(JNIEnv *env, jobject /* this */, jlong object, jstring name)
{
    return reinterpret_cast<jlong>(ngsJsonObjectGetArray(
            reinterpret_cast<JsonObjectH>(object), jniString(env, name).c_str()));
}

NGS_JNI_FUNC(jlong, jsonObjectGetObject)(JNIEnv *env, jobject /* this */, jlong object, jstring name)
{
    return reinterpret_cast<jlong>(ngsJsonObjectGetObject(
            reinterpret_cast<JsonObjectH>(object), jniString(env, name).c_str()));
}

NGS_JNI_FUNC(jint, jsonArraySize)(JNIEnv /* *env*/, jobject /* this */, jlong object)
{
    return ngsJsonArraySize(reinterpret_cast<JsonObjectH>(object));
}

NGS_JNI_FUNC(jlong, jsonArrayItem)(JNIEnv /* *env*/, jobject /* this */, jlong object, jint index)
{
    return reinterpret_cast<jlong>(ngsJsonArrayItem(reinterpret_cast<JsonObjectH>(object), index));
}

NGS_JNI_FUNC(jstring, jsonObjectGetStringForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                                 jstring name, jstring defaultValue)
{
    return env->NewStringUTF( ngsJsonObjectGetStringForKey(
            reinterpret_cast<JsonObjectH>(object), jniString(env, name).c_str(),
            jniString(env, defaultValue).c_str()) );
}

NGS_JNI_FUNC(jdouble, jsonObjectGetDoubleForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                                  jstring name, jdouble defaultValue)
{
    return ngsJsonObjectGetDoubleForKey(reinterpret_cast<JsonObjectH>(object),
                                         jniString(env, name).c_str(), defaultValue);
}

NGS_JNI_FUNC(jint, jsonObjectGetIntegerForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                               jstring name, jint defaultValue)
{
    return ngsJsonObjectGetIntegerForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), defaultValue);
}

NGS_JNI_FUNC(jlong, jsonObjectGetLongForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                             jstring name, jlong defaultValue)
{
    return ngsJsonObjectGetLongForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), defaultValue);
}

NGS_JNI_FUNC(jboolean, jsonObjectGetBoolForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                                jstring name, jboolean defaultValue)
{
    return ngsJsonObjectGetBoolForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), defaultValue) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetStringForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                                  jstring name, jstring value)
{
    return ngsJsonObjectSetStringForKey(reinterpret_cast<JsonObjectH>(object),
                                        jniString(env, name).c_str(),
                                        jniString(env, value).c_str()) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetDoubleForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                                  jstring name, jdouble value)
{
    return ngsJsonObjectSetDoubleForKey(reinterpret_cast<JsonObjectH>(object),
                                         jniString(env, name).c_str(), value) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetIntegerForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                                   jstring name, jint value)
{
    return ngsJsonObjectSetIntegerForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), value) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetLongForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                                jstring name, jlong value)
{
    return ngsJsonObjectSetLongForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), value) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetBoolForKey)(JNIEnv *env, jobject /* this */, jlong object,
                                                jstring name, jboolean value)
{
    return ngsJsonObjectSetBoolForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), value) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

/**
 * Catalog functions
 */

NGS_JNI_FUNC(jstring, catalogPathFromSystem)(JNIEnv *env, jobject /* this */, jstring path)
{
    return env->NewStringUTF(ngsCatalogPathFromSystem(jniString(env, path).c_str()));
}

NGS_JNI_FUNC(jlong, catalogObjectGet)(JNIEnv *env, jobject /* this */, jstring path)
{
    return reinterpret_cast<jlong>(ngsCatalogObjectGet(jniString(env, path).c_str()));
}

static jobjectArray catalogObjectQueryToJobjectArray(JNIEnv *env, ngsCatalogObjectInfo *info)
{
    if(nullptr == info) {
        return env->NewObjectArray(0, g_CatalogObjectInfoClass, 0);
    }

    std::vector<jobject> infoArray;
    int counter = 0;
    while(info[counter].name != nullptr) {
        jvalue args[3];
        args[0].l = env->NewStringUTF(info[counter].name);
        args[1].i = info[counter].type;
        args[2].j = reinterpret_cast<jlong>(info[counter].object);

        infoArray.push_back(env->NewObjectA(g_CatalogObjectInfoClass, g_CatalogObjectInfoInitMid, args));
        counter++;
    }

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(infoArray.size()), g_CatalogObjectInfoClass, 0);
    for(int i = 0; i < infoArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, infoArray[i]);
    }

    return array;
}

NGS_JNI_FUNC(jobjectArray, catalogObjectQuery)(JNIEnv *env, jobject /* this */, jlong object, jint filter)
{
    ngsCatalogObjectInfo *info = ngsCatalogObjectQuery(reinterpret_cast<CatalogObjectH>(object), filter);
    jobjectArray out = catalogObjectQueryToJobjectArray(env, info);

    if(nullptr != info) {
        ngsFree(info);
    }

    return out;
}

NGS_JNI_FUNC(jobjectArray, catalogObjectQueryMultiFilter)(JNIEnv *env, jobject /* this */, jlong object, jintArray filters)
{
    int size = env->GetArrayLength(filters);
    jboolean iscopy;
    jint *filtersArray = env->GetIntArrayElements(filters, &iscopy);
    ngsCatalogObjectInfo *info = ngsCatalogObjectQueryMultiFilter(
            reinterpret_cast<CatalogObjectH>(object), filtersArray, size);
    jobjectArray out = catalogObjectQueryToJobjectArray(env, info);
    if(nullptr != info) {
        ngsFree(info);
    }
    return out;
}

NGS_JNI_FUNC(jboolean, catalogObjectDelete)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return ngsCatalogObjectDelete(reinterpret_cast<CatalogObjectH>(object)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, catalogObjectCreate)(JNIEnv *env, jobject /* this */, jlong object, jstring name, jobjectArray options)
{
    char **nativeOptions = toOptions(env, options);
    int res = ngsCatalogObjectCreate(reinterpret_cast<CatalogObjectH>(object), jniString(env, name).c_str(), nativeOptions);
    CSLDestroy(nativeOptions);
    return res == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, catalogObjectCopy)(JNIEnv *env, jobject /* this */, jlong srcObject,
                                          jlong dstObjectContainer, jobjectArray options, jint callbackId)
{
    char **nativeOptions = toOptions(env, options);
    int result;
    if(callbackId == 0) {
        result = ngsCatalogObjectCopy(reinterpret_cast<CatalogObjectH>(srcObject),
                                      reinterpret_cast<CatalogObjectH>(dstObjectContainer),
                                      nativeOptions, nullptr, nullptr);
    }
    else {
        result = ngsCatalogObjectCopy(reinterpret_cast<CatalogObjectH>(srcObject),
                             reinterpret_cast<CatalogObjectH>(dstObjectContainer),
                             nativeOptions, progressProxyFunc,
                             reinterpret_cast<void *>(callbackId));
    }
    CSLDestroy(nativeOptions);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, catalogObjectRename)(JNIEnv *env, jobject /* this */, jlong object,
                                            jstring newName)
{
    return ngsCatalogObjectRename(reinterpret_cast<CatalogObjectH>(object),
                                  jniString(env, newName).c_str()) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jstring, catalogObjectOptions)(JNIEnv *env, jobject /* this */, jlong object, jint optionType)
{
    return env->NewStringUTF(ngsCatalogObjectOptions(reinterpret_cast<CatalogObjectH>(object), optionType));
}

NGS_JNI_FUNC(jint, catalogObjectType)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return ngsCatalogObjectType(reinterpret_cast<CatalogObjectH>(object));
}

NGS_JNI_FUNC(jstring, catalogObjectName)(JNIEnv *env, jobject /* this */, jlong object)
{
    return env->NewStringUTF(ngsCatalogObjectName(reinterpret_cast<CatalogObjectH>(object)));
}

NGS_JNI_FUNC(jobject, catalogObjectProperties)(JNIEnv *env, jobject /* this */, jlong object, jstring domain)
{
    char **properties = ngsCatalogObjectProperties(reinterpret_cast<CatalogObjectH>(object),
                                                   jniString(env, domain).c_str());
    jobject ret = fromOptions(env, reinterpret_cast<CSLConstList>(properties));
    ngsListFree(properties);
    return ret;
}

NGS_JNI_FUNC(jboolean , catalogObjectSetProperty)(JNIEnv *env, jobject /* this */, jlong object,
                                                  jstring name, jstring value, jstring domain)
{
    int ret = ngsCatalogObjectSetProperty(reinterpret_cast<CatalogObjectH>(object),
                                          jniString(env, name).c_str(), jniString(env, value).c_str(),
                                          jniString(env, domain).c_str());
    return ret == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(void, catalogObjectRefresh)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    ngsCatalogObjectRefresh(reinterpret_cast<CatalogObjectH>(object));
}

/**
 * Feature class
 */

NGS_JNI_FUNC(jboolean, datasetOpen)(JNIEnv *env, jobject /* this */, jlong object,
                                    jint openFlags, jobjectArray openOptions)
{
    char** openOptionsNative = toOptions(env, openOptions);
    int result = ngsDatasetOpen(reinterpret_cast<CatalogObjectH>(object),
                                static_cast<unsigned int>(openFlags), openOptionsNative);
    CSLDestroy(openOptionsNative);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, datasetIsOpened)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return ngsDatasetIsOpened(reinterpret_cast<CatalogObjectH>(object)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, datasetClose)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return ngsDatasetClose(reinterpret_cast<CatalogObjectH>(object)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobjectArray, featureClassFields)(JNIEnv *env, jobject /* this */, jlong object)
{
    ngsField *fields = ngsFeatureClassFields(reinterpret_cast<CatalogObjectH>(object));
    if(nullptr == fields) {
        return env->NewObjectArray(0, g_CatalogObjectInfoClass, 0);
    }

    std::vector<jobject> fieldArray;
    int counter = 0;
    while(fields[counter].name != nullptr) {
        jvalue args[4];
        args[0].l = env->NewStringUTF(fields[counter].name);
        args[1].l = env->NewStringUTF(fields[counter].alias);
        args[2].i = fields[counter].type;
        args[3].l = nullptr;

        fieldArray.push_back(env->NewObjectA(g_FieldClass, g_FieldInitMid, args));
        counter++;
    }

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(fieldArray.size()), g_FieldClass, 0);
    for(int i = 0; i < fieldArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, fieldArray[i]);
    }

    return array;
}

NGS_JNI_FUNC(jint, featureClassGeometryType)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return ngsFeatureClassGeometryType(reinterpret_cast<CatalogObjectH>(object));
}

NGS_JNI_FUNC(jboolean, featureClassCreateOverviews)(JNIEnv *env, jobject /* this */, jlong object, jobjectArray options, jint callbackId)
{
    char **nativeOptions = toOptions(env, options);
    int result;
    if(callbackId == 0) {
        result = ngsFeatureClassCreateOverviews(reinterpret_cast<CatalogObjectH>(object),
                                                nativeOptions, nullptr, nullptr);
    }
    else {
        result = ngsFeatureClassCreateOverviews(reinterpret_cast<CatalogObjectH>(object),
                                                nativeOptions, progressProxyFunc,
                                                reinterpret_cast<void *>(callbackId));
    }
    CSLDestroy(nativeOptions);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, featureClassCreateFeature)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return reinterpret_cast<jlong>(ngsFeatureClassCreateFeature(reinterpret_cast<CatalogObjectH>(object)));
}

NGS_JNI_FUNC(void, featureClassBatchMode)(JNIEnv /* *env */, jobject /* this */, jlong object, jboolean enable)
{
    ngsFeatureClassBatchMode(reinterpret_cast<CatalogObjectH>(object), enable);
}

NGS_JNI_FUNC(jboolean, featureClassInsertFeature)(JNIEnv /* *env */, jobject /* this */, jlong object, jlong feature, jboolean logEdits)
{
    return ngsFeatureClassInsertFeature(reinterpret_cast<CatalogObjectH>(object),
                                        reinterpret_cast<FeatureH>(feature), logEdits) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassUpdateFeature)(JNIEnv /* *env */, jobject /* this */, jlong object, jlong feature, jboolean logEdits)
{
    return ngsFeatureClassUpdateFeature(reinterpret_cast<CatalogObjectH>(object),
                                        reinterpret_cast<FeatureH>(feature), logEdits) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassDeleteFeature)(JNIEnv /* *env */, jobject /* this */, jlong object, jlong id, jboolean logEdits)
{
    return ngsFeatureClassDeleteFeature(reinterpret_cast<CatalogObjectH>(object), id, logEdits) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassDeleteFeatures)(JNIEnv /* *env */, jobject /* this */, jlong object, jboolean logEdits)
{
    return ngsFeatureClassDeleteFeatures(reinterpret_cast<CatalogObjectH>(object), logEdits) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, featureClassCount)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return ngsFeatureClassCount(reinterpret_cast<CatalogObjectH>(object));
}

NGS_JNI_FUNC(void, featureClassResetReading)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    ngsFeatureClassResetReading(reinterpret_cast<CatalogObjectH>(object));
}

NGS_JNI_FUNC(jlong, featureClassNextFeature)(JNIEnv /* *env */, jobject /* this */, jlong object)
{
    return reinterpret_cast<jlong>(ngsFeatureClassNextFeature(reinterpret_cast<CatalogObjectH>(object)));
}

NGS_JNI_FUNC(jlong, featureClassGetFeature)(JNIEnv /* *env */, jobject /* this */, jlong object, jlong id)
{
    return reinterpret_cast<jlong>(ngsFeatureClassGetFeature(reinterpret_cast<CatalogObjectH>(object), id));
}

NGS_JNI_FUNC(jboolean, featureClassSetFilter)(JNIEnv *env, jobject /* this */, jlong object,
                                              jlong geometryFilter, jstring attributeFilter)
{
    return ngsFeatureClassSetFilter(reinterpret_cast<CatalogObjectH>(object),
                                    reinterpret_cast<GeometryH>(geometryFilter),
                                    jniString(env, attributeFilter).c_str()) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassSetSpatialFilter)(JNIEnv /* *env */, jobject /* this */, jlong object,
                                                        jdouble minX, jdouble minY, jdouble maxX, jdouble maxY)
{
    return ngsFeatureClassSetSpatialFilter(reinterpret_cast<CatalogObjectH>(object),
                                           minX, minY, maxX, maxY) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassDeleteEditOperation)(JNIEnv /* *env */, jobject /* this */, jlong object, jlong fid, jlong aid, jint code, jlong rid, jlong arid)
{
    ngsEditOperation operation = {fid, aid, static_cast<ngsChangeCode>(code), rid, arid};
    return ngsFeatureClassDeleteEditOperation(reinterpret_cast<CatalogObjectH>(object),
                                              operation) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobjectArray, featureClassGetEditOperations)(JNIEnv *env, jobject /* this */, jlong object)
{
    ngsEditOperation *out = ngsFeatureClassGetEditOperations(
            reinterpret_cast<CatalogObjectH>(object));
    int counter = 0;
    std::vector<jobject> obArray;
    while(out[counter].fid != -1) {
        jvalue args[5];
        args[0].j = out[counter].fid;
        args[1].j = out[counter].aid;
        args[2].i = out[counter].code;
        args[3].j = out[counter].rid;
        args[4].j = out[counter].arid;

        obArray.push_back(env->NewObjectA(g_EditOperationClass, g_EditOperationInitMid, args));
        counter++;
    }
    ngsFree(out);

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(obArray.size()), g_EditOperationClass, 0);
    for(int i = 0; i < obArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, obArray[i]);
    }

    return array;
}

NGS_JNI_FUNC(void, featureFree)(JNIEnv /* *env */, jobject /* this */, jlong feature)
{
    ngsFeatureFree(reinterpret_cast<FeatureH>(feature));
}

NGS_JNI_FUNC(jint, featureFieldCount)(JNIEnv /* *env */, jobject /* this */, jlong feature)
{
    return ngsFeatureFieldCount(reinterpret_cast<FeatureH>(feature));
}

NGS_JNI_FUNC(jboolean, featureIsFieldSet)(JNIEnv /* *env */, jobject /* this */, jlong feature, jint fieldIndex)
{
    return ngsFeatureIsFieldSet(reinterpret_cast<FeatureH>(feature), fieldIndex) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, featureGetId)(JNIEnv /* *env */, jobject /* this */, jlong feature)
{
    return ngsFeatureGetId(reinterpret_cast<FeatureH>(feature));
}

NGS_JNI_FUNC(jlong, featureGetGeometry)(JNIEnv /* *env */, jobject /* this */, jlong feature)
{
    return reinterpret_cast<jlong>(ngsFeatureGetGeometry(reinterpret_cast<FeatureH>(feature)));
}

NGS_JNI_FUNC(jint, featureGetFieldAsInteger)(JNIEnv /* *env */, jobject /* this */, jlong feature, jint field)
{
    return ngsFeatureGetFieldAsInteger(reinterpret_cast<FeatureH>(feature), field);
}

NGS_JNI_FUNC(jdouble, featureGetFieldAsDouble)(JNIEnv /* *env */, jobject /* this */, jlong feature, jint field)
{
    return ngsFeatureGetFieldAsDouble(reinterpret_cast<FeatureH>(feature), field);
}

NGS_JNI_FUNC(jstring, featureGetFieldAsString)(JNIEnv *env, jobject /* this */, jlong feature, jint field)
{
    return env->NewStringUTF(ngsFeatureGetFieldAsString(reinterpret_cast<FeatureH>(feature), field));
}

NGS_JNI_FUNC(jobject, featureGetFieldAsDateTime)(JNIEnv *env, jobject /* this */, jlong feature, jint field)
{
    int year(0), month(0), day(0), hour(0), minute(0), tzflag(0);
    float second(0.0f);
    ngsFeatureGetFieldAsDateTime(reinterpret_cast<FeatureH>(feature), field, &year, &month, &day, &hour, &minute, &second, &tzflag);
    if( tzflag > 1 ) {
        tzflag = (tzflag - 100) / 4 * 3600000; // milliseconds
    }
    else {
        tzflag = 0; // UTC
    }
    jvalue args[7];
    args[0].i = year;
    args[1].i = month;
    args[2].i = day;
    args[3].i = hour;
    args[4].i = minute;
    args[5].i = static_cast<jint>(second);
    args[6].i = tzflag;
    return env->NewObjectA(g_DateComponentsClass, g_DateComponentsInitMid, args);
}

NGS_JNI_FUNC(void, featureSetGeometry)(JNIEnv /* *env */, jobject /* this */, jlong feature, jlong geometry)
{
    ngsFeatureSetGeometry(reinterpret_cast<FeatureH>(feature), reinterpret_cast<GeometryH>(geometry));
}

NGS_JNI_FUNC(void, featureSetFieldInteger)(JNIEnv /* *env */, jobject /* this */, jlong feature, jint field, jint value)
{
    ngsFeatureSetFieldInteger(reinterpret_cast<FeatureH>(feature), field, value);
}

NGS_JNI_FUNC(void, featureSetFieldDouble)(JNIEnv /* *env */, jobject /* this */, jlong feature, jint field, jdouble value)
{
    ngsFeatureSetFieldDouble(reinterpret_cast<FeatureH>(feature), field, value);
}

NGS_JNI_FUNC(void, featureSetFieldString)(JNIEnv *env, jobject /* this */, jlong feature, jint field, jstring value)
{
    ngsFeatureSetFieldString(reinterpret_cast<FeatureH>(feature), field, jniString(env, value).c_str());
}

NGS_JNI_FUNC(void, featureSetFieldDateTime)(JNIEnv /* *env */, jobject /* this */, jlong feature, jint field,
                                            jint year, jint month, jint day, jint hour, jint minute,
                                            jint second)
{
    ngsFeatureSetFieldDateTime(reinterpret_cast<FeatureH>(feature), field, year, month, day, hour, minute, second, 100); // 100 is UTC
}

NGS_JNI_FUNC(jlong, storeFeatureClassGetFeatureByRemoteId)(JNIEnv /* *env */, jobject /* this */, jlong object, jlong rid)
{
    return reinterpret_cast<jlong>(ngsStoreFeatureClassGetFeatureByRemoteId(
            reinterpret_cast<CatalogObjectH>(object), rid));
}

NGS_JNI_FUNC(jlong, storeFeatureGetRemoteId)(JNIEnv /* *env */, jobject /* this */, jlong feature)
{
    return ngsStoreFeatureGetRemoteId(reinterpret_cast<FeatureH>(feature));
}

NGS_JNI_FUNC(void, storeFeatureSetRemoteId)(JNIEnv /* *env */, jobject /* this */, jlong feature, jlong rid)
{
    ngsStoreFeatureSetRemoteId(reinterpret_cast<FeatureH>(feature), rid);
}

NGS_JNI_FUNC(jlong, featureCreateGeometry)(JNIEnv /* *env */, jobject /* this */, jlong feature)
{
    return reinterpret_cast<jlong>(ngsFeatureCreateGeometry(reinterpret_cast<FeatureH>(feature)));
}

NGS_JNI_FUNC(jlong, featureCreateGeometryFromJson)(JNIEnv /* *env */, jobject /* this */, jlong geometry)
{
    return reinterpret_cast<jlong>(ngsFeatureCreateGeometryFromJson(reinterpret_cast<JsonObjectH>(geometry)));
}

NGS_JNI_FUNC(void, geometryFree)(JNIEnv /* *env */, jobject /* this */, jlong geometry)
{
    ngsGeometryFree(reinterpret_cast<GeometryH>(geometry));
}

NGS_JNI_FUNC(void, geometrySetPoint)(JNIEnv /* *env */, jobject /* this */, jlong geometry, jint point,
                                     jdouble x, jdouble y, jdouble z, jdouble m)
{
    ngsGeometrySetPoint(reinterpret_cast<GeometryH>(geometry), point, x, y, z, m);
}

NGS_JNI_FUNC(jobject, geometryGetEnvelope)(JNIEnv *env, jobject /* this */, jlong geometry)
{
    ngsExtent ext = ngsGeometryGetEnvelope(reinterpret_cast<GeometryH>(geometry));
    jvalue args[4];
    args[0].d = ext.minX;
    args[1].d = ext.maxX;
    args[2].d = ext.minY;
    args[3].d = ext.maxY;
    return env->NewObjectA(g_EnvelopeClass, g_EnvelopeInitMid, args);
}

NGS_JNI_FUNC(jboolean, geometryTransformTo)(JNIEnv /* *env */, jobject /* this */, jlong geometry, jint  EPSG)
{
    return ngsGeometryTransformTo(reinterpret_cast<GeometryH>(geometry), EPSG) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, geometryTransform)(JNIEnv /* *env */, jobject /* this */, jlong geometry, jlong ct)
{
    return ngsGeometryTransform(reinterpret_cast<GeometryH>(geometry),
                                reinterpret_cast<CoordinateTransformationH>(ct)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, geometryIsEmpty)(JNIEnv /* *env */, jobject /* this */, jlong geometry)
{
    return ngsGeometryIsEmpty(reinterpret_cast<GeometryH>(geometry)) == 0 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jint, geometryGetType)(JNIEnv /* *env */, jobject /* this */, jlong geometry)
{
    return ngsGeometryGetType(reinterpret_cast<GeometryH>(geometry));
}

NGS_JNI_FUNC(jstring, geometryToJson)(JNIEnv *env, jobject /* this */, jlong geometry)
{
    return env->NewStringUTF(ngsGeometryToJson(reinterpret_cast<GeometryH>(geometry)));
}

NGS_JNI_FUNC(jlong, coordinateTransformationCreate)(JNIEnv /* *env */, jobject /* this */,
                                                    jint fromEPSG, jint toEPSG)
{
    return reinterpret_cast<jlong>(ngsCoordinateTransformationCreate(fromEPSG, toEPSG));
}

NGS_JNI_FUNC(void, coordinateTransformationFree)(JNIEnv /* *env */, jobject /* this */, jlong handle)
{
    ngsCoordinateTransformationFree(reinterpret_cast<CoordinateTransformationH>(handle));
}

NGS_JNI_FUNC(jobject, coordinateTransformationDo)(JNIEnv *env, jobject /* this */, jlong object,
                                                  jdouble x, jdouble y)
{
    ngsCoordinate coord = {x, y, 0.0};
    coord = ngsCoordinateTransformationDo(reinterpret_cast<CoordinateTransformationH>(object), coord);
    jvalue args[2];
    args[0].d = coord.X;
    args[1].d = coord.Y;
    return  env->NewObjectA(g_PointClass, g_PointInitMid, args);
}

NGS_JNI_FUNC(jlong, featureAttachmentAdd)(JNIEnv *env, jobject /* this */,
                                          jlong feature, jstring name, jstring description,
                                          jstring path, jobjectArray options, jboolean logEdits)
{
    return ngsFeatureAttachmentAdd(reinterpret_cast<FeatureH>(feature), jniString(env, name).c_str(),
        jniString(env, description).c_str(), jniString(env, path).c_str(), toOptions(env, options),
                                   logEdits);
}

NGS_JNI_FUNC(jboolean, featureAttachmentDelete)(JNIEnv /* *env */, jobject /* this */,
                                                jlong feature, jlong aid, jboolean logEdits)
{
    return ngsFeatureAttachmentDelete(reinterpret_cast<FeatureH>(feature), aid, logEdits) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureAttachmentDeleteAll)(JNIEnv /* *env */, jobject /* this */,
                                                   jlong feature, jboolean logEdits)
{
    return ngsFeatureAttachmentDeleteAll(reinterpret_cast<FeatureH>(feature), logEdits) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobject, featureAttachmentsGet)(JNIEnv *env, jobject /* this */, jlong feature)
{
    ngsFeatureAttachmentInfo *out = ngsFeatureAttachmentsGet(reinterpret_cast<FeatureH>(feature));
    int counter = 0;
    std::vector<jobject> obArray;
    while(out[counter].id != -1) {
        jvalue args[7];
        args[0].j = feature;
        args[1].j = out->id;
        args[2].l = env->NewStringUTF(out->name);
        args[3].l = env->NewStringUTF(out->description);
        args[4].l = env->NewStringUTF(out->path);
        args[5].j = out->size;
        args[6].j = out->rid;
        obArray.push_back(env->NewObjectA(g_EditOperationClass, g_EditOperationInitMid, args));
        counter++;
    }

    ngsFree(out);

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(obArray.size()), g_AttachmentClass, 0);
    for(int i = 0; i < obArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, obArray[i]);
    }

    return array;
}

NGS_JNI_FUNC(jboolean, featureAttachmentUpdate)(JNIEnv *env, jobject /* this */, jlong feature,
                                                jlong aid, jstring name, jstring description, jboolean logEdits)
{
    return ngsFeatureAttachmentUpdate(reinterpret_cast<FeatureH>(feature),
                                      aid, jniString(env, name).c_str(),
                                      jniString(env, description).c_str(), logEdits) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(void, storeFeatureSetAttachmentRemoteId)(JNIEnv /* *env */, jobject /* this */,
                                                      jlong feature, jlong aid, jlong rid)
{
    ngsStoreFeatureSetAttachmentRemoteId(reinterpret_cast<FeatureH>(feature), aid, rid);
}

/**
  * Raster
  */

NGS_JNI_FUNC(jboolean, rasterCacheArea)(JNIEnv *env, jobject /* this */, jlong object, jobjectArray options, jint callbackId)
{
    char **nativeOptions = toOptions(env, options);
    int result;
    if(callbackId == 0) {
        result = ngsRasterCacheArea(reinterpret_cast<CatalogObjectH>(object),
                                    nativeOptions, nullptr, nullptr);
    }
    else {
        result = ngsRasterCacheArea(reinterpret_cast<CatalogObjectH>(object),
                                    nativeOptions, progressProxyFunc,
                                    reinterpret_cast<void *>(callbackId));
    }
    CSLDestroy(nativeOptions);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

///**
// * Map functions
// *
// *  ngsCreateMap -> ngsInitMap -> ngsSaveMap [optional]
// *  ngsLoadMap -> ngsInitMap -> ngsSaveMap [optional]
// */
//
//typedef void *LayerH;
//NGS_EXTERNC unsigned char ngsMapCreate(const char *name, const char *description,
//                                       unsigned short epsg, double minX, double minY,
//                                       double maxX, double maxY);
//NGS_EXTERNC unsigned char ngsMapOpen(const char *path);
//NGS_EXTERNC int ngsMapSave(unsigned char mapId, const char *path);
//NGS_EXTERNC int ngsMapClose(unsigned char mapId);
//NGS_EXTERNC int ngsMapLayerCount(unsigned char mapId);
//NGS_EXTERNC int ngsMapCreateLayer(unsigned char mapId, const char *name,
//                                  const char *path);
//NGS_EXTERNC LayerH ngsMapLayerGet(unsigned char mapId, int layerId);
//NGS_EXTERNC int ngsMapLayerDelete(unsigned char mapId, LayerH layer);
//NGS_EXTERNC int ngsMapLayerReorder(unsigned char mapId, LayerH beforeLayer,
//                                   LayerH movedLayer);
//NGS_EXTERNC int ngsMapSetSize(unsigned char mapId, int width, int height,
//                              int YAxisInverted);
//NGS_EXTERNC int ngsMapDraw(unsigned char mapId, enum ngsDrawState state,
//                           ngsProgressFunc callback, void *callbackData);
//NGS_EXTERNC int ngsMapInvalidate(unsigned char mapId, ngsExtent bounds);
//NGS_EXTERNC int ngsMapSetBackgroundColor(unsigned char mapId, const ngsRGBA color);
//NGS_EXTERNC ngsRGBA ngsMapGetBackgroundColor(unsigned char mapId);
//NGS_EXTERNC int ngsMapSetCenter(unsigned char mapId, double x, double y);
//NGS_EXTERNC ngsCoordinate ngsMapGetCenter(unsigned char mapId);
//NGS_EXTERNC ngsCoordinate ngsMapGetCoordinate(unsigned char mapId, double x,
//                                              double y);
//NGS_EXTERNC ngsCoordinate ngsMapGetDistance(unsigned char mapId, double w,
//                                            double h);
//NGS_EXTERNC int ngsMapSetRotate(unsigned char mapId, enum ngsDirection dir,
//                                double rotate);
//NGS_EXTERNC double ngsMapGetRotate(unsigned char mapId, enum ngsDirection dir);
//NGS_EXTERNC int ngsMapSetScale(unsigned char mapId, double scale);
//NGS_EXTERNC double ngsMapGetScale(unsigned char mapId);
//
//NGS_EXTERNC int ngsMapSetOptions(unsigned char mapId, char **options);
//NGS_EXTERNC int ngsMapSetExtentLimits(unsigned char mapId,
//                                      double minX, double minY,
//                                      double maxX, double maxY);
//NGS_EXTERNC ngsExtent ngsMapGetExtent(unsigned char mapId, int epsg);
//NGS_EXTERNC int ngsMapSetExtent(unsigned char mapId, ngsExtent extent);
//
////NGS_EXTERNC void ngsMapSetLocation(unsigned char mapId, double x, double y, double azimuth);
//NGS_EXTERNC JsonObjectH ngsMapGetSelectionStyle(unsigned char mapId,
//                                                enum ngsStyleType styleType);
//NGS_EXTERNC int ngsMapSetSelectionsStyle(unsigned char mapId,
//                                         enum ngsStyleType styleType,
//                                         JsonObjectH style);
//NGS_EXTERNC const char *ngsMapGetSelectionStyleName(unsigned char mapId,
//                                                    enum ngsStyleType styleType);
//NGS_EXTERNC int ngsMapSetSelectionStyleName(unsigned char mapId,
//                                            enum ngsStyleType styleType,
//                                            const char *name);
//NGS_EXTERNC int ngsMapIconSetAdd(unsigned char mapId, const char *name,
//                                 const char *path, char ownByMap);
//NGS_EXTERNC int ngsMapIconSetRemove(unsigned char mapId, const char *name);
//NGS_EXTERNC char ngsMapIconSetExists(unsigned char mapId, const char *name);

/**
 * Layer functions
 */

//NGS_EXTERNC const char *ngsLayerGetName(LayerH layer);
//NGS_EXTERNC int ngsLayerSetName(LayerH layer, const char *name);
//NGS_EXTERNC char ngsLayerGetVisible(LayerH layer);
//NGS_EXTERNC int ngsLayerSetVisible(LayerH layer, char visible);
//NGS_EXTERNC CatalogObjectH ngsLayerGetDataSource(LayerH layer);
//NGS_EXTERNC JsonObjectH ngsLayerGetStyle(LayerH layer);
//NGS_EXTERNC int ngsLayerSetStyle(LayerH layer, JsonObjectH style);
//NGS_EXTERNC const char *ngsLayerGetStyleName(LayerH layer);
//NGS_EXTERNC int ngsLayerSetStyleName(LayerH layer, const char *name);
//NGS_EXTERNC int ngsLayerSetSelectionIds(LayerH layer, long long *ids, int size);
//NGS_EXTERNC int ngsLayerSetHideIds(LayerH layer, long long *ids, int size);

/**
 * Overlay functions
 */

//NGS_EXTERNC int ngsOverlaySetVisible(unsigned char mapId, int typeMask,
//                                     char visible);
//NGS_EXTERNC char ngsOverlayGetVisible(unsigned char mapId,
//                                      enum ngsMapOverlayType type);
//NGS_EXTERNC int ngsOverlaySetOptions(unsigned char mapId,
//                                     enum ngsMapOverlayType type, char **options);
//NGS_EXTERNC char **ngsOverlayGetOptions(unsigned char mapId,
//                                        enum ngsMapOverlayType type);
///* Edit */
//typedef struct _ngsPointId
//{
//    int pointId;
//    char isHole;
//} ngsPointId;
//
//NGS_EXTERNC ngsPointId ngsEditOverlayTouch(unsigned char mapId, double x,
//                                           double y, enum ngsMapTouchType type);
//NGS_EXTERNC char ngsEditOverlayUndo(unsigned char mapId);
//NGS_EXTERNC char ngsEditOverlayRedo(unsigned char mapId);
//NGS_EXTERNC char ngsEditOverlayCanUndo(unsigned char mapId);
//NGS_EXTERNC char ngsEditOverlayCanRedo(unsigned char mapId);
//NGS_EXTERNC FeatureH ngsEditOverlaySave(unsigned char mapId);
//NGS_EXTERNC int ngsEditOverlayCancel(unsigned char mapId);
//NGS_EXTERNC int ngsEditOverlayCreateGeometryInLayer(unsigned char mapId,
//                                                    LayerH layer, char empty);
//NGS_EXTERNC int ngsEditOverlayCreateGeometry(unsigned char mapId,
//                                             ngsGeometryType type);
//NGS_EXTERNC int ngsEditOverlayEditGeometry(unsigned char mapId, LayerH layer,
//                                           long long feateureId);
//NGS_EXTERNC int ngsEditOverlayDeleteGeometry(unsigned char mapId);
//NGS_EXTERNC int ngsEditOverlayAddPoint(unsigned char mapId);
//NGS_EXTERNC int ngsEditOverlayAddVertex(unsigned char mapId,
//                                        ngsCoordinate coordinates);
//NGS_EXTERNC enum ngsEditDeleteResult ngsEditOverlayDeletePoint(unsigned char mapId);
//NGS_EXTERNC int ngsEditOverlayAddHole(unsigned char mapId);
//NGS_EXTERNC enum ngsEditDeleteResult ngsEditOverlayDeleteHole(unsigned char mapId);
//NGS_EXTERNC int ngsEditOverlayAddGeometryPart(unsigned char mapId);
//NGS_EXTERNC enum ngsEditDeleteResult ngsEditOverlayDeleteGeometryPart(
//        unsigned char mapId);
//NGS_EXTERNC GeometryH ngsEditOverlayGetGeometry(unsigned char mapId);
//NGS_EXTERNC int ngsEditOverlaySetStyle(unsigned char mapId,
//                                       enum ngsEditStyleType type,
//                                       JsonObjectH style);
//NGS_EXTERNC int ngsEditOverlaySetStyleName(unsigned char mapId,
//                                           enum ngsEditStyleType type,
//                                           const char *name);
//NGS_EXTERNC JsonObjectH ngsEditOverlayGetStyle(unsigned char mapId,
//                                               enum ngsEditStyleType type);
//NGS_EXTERNC void ngsEditOverlaySetWalkingMode(unsigned char mapId, char enable);
//NGS_EXTERNC char ngsEditOverlayGetWalkingMode(unsigned char mapId);
//
/* Location */
//NGS_EXTERNC int ngsLocationOverlayUpdate(unsigned char mapId,
//                                         ngsCoordinate location,
//                                         float direction, float accuracy);
//NGS_EXTERNC int ngsLocationOverlaySetStyle(unsigned char mapId, JsonObjectH style);
//NGS_EXTERNC int ngsLocationOverlaySetStyleName(unsigned char mapId,
//                                               const char *name);
//NGS_EXTERNC JsonObjectH ngsLocationOverlayGetStyle(unsigned char mapId);