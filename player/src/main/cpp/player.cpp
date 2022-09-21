#include <jni.h>
#include <string>
#include "VideoPlayer.h"
#include "JNICallbackHelper.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_lxc_player_NativeLib_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

JavaVM *vm = 0;

/**
 * 该函数在java层调用loadLibrary函数时会触发执行.
 * @param vm JavaVM居右跨线程的能力。
 * @return
 */
jint JNI_OnLoad(JavaVM *vm, void *args) {
    ::vm = vm;
    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_prepareNative(JNIEnv *env, jobject thiz, jstring data_source) {
    //使用new关键字创建的对象，是在堆中申请的空间。
    auto *helper = new JNICallbackHelper(vm, env, thiz);
    const char *data_source_ = env->GetStringUTFChars(data_source, 0);
    auto *player = new VideoPlayer(data_source_, helper);
    player->prepare();
    env->ReleaseStringUTFChars(data_source, data_source_);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_startNative(JNIEnv *env, jobject thiz) {
}

extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_stopNative(JNIEnv *env, jobject thiz) {
}

extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_releaseNative(JNIEnv *env, jobject thiz) {
}