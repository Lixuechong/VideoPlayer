#ifndef VIDEOPLAYER_JNICALLBACKHELPER_H
#define VIDEOPLAYER_JNICALLBACKHELPER_H

#include <jni.h>
#include "util.h"

class JNICallbackHelper {
private:
    JavaVM *vm = 0;
    JNIEnv *env = 0;
    jobject job = 0;
    jmethodID jmd_prepared = 0;
    jmethodID jmd_error = 0;

public:
    JNICallbackHelper(JavaVM *, JNIEnv *, jobject);

    virtual ~JNICallbackHelper();

    void onPrepared(int);

    void onError(char*, int);
};


#endif //VIDEOPLAYER_JNICALLBACKHELPER_H
