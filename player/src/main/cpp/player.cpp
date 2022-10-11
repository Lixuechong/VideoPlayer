#include <jni.h>
#include <string>
#include "VideoPlayer.h"
#include "JNICallbackHelper.h"
#include <android/native_window_jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_lxc_player_NativeLib_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

VideoPlayer *player = 0;
JavaVM *vm = 0;
ANativeWindow *window = 0;

pthread_mutex_t static_mutex = PTHREAD_MUTEX_INITIALIZER;// 静态初始化互斥锁

/**
 * 该函数在java层调用loadLibrary函数时会触发执行.
 * @param vm JavaVM居右跨线程的能力。
 * @return
 */
jint JNI_OnLoad(JavaVM *vm, void *args) {
    ::vm = vm;
    return JNI_VERSION_1_6;
}

/**
 * 渲染画面
 * @param src_data 渲染数据
 * @param width 数据宽
 * @param height 数据宽
 * @param src_line_size 数据大小
 */
void renderFrame(uint8_t * src_data, int width, int height, int src_lineSize) {
    pthread_mutex_lock(&static_mutex);
    if (window) {
        // 设置窗口的大小，各个属性
        ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);

        // 他自己有个缓冲区 buffer
        ANativeWindow_Buffer window_buffer; // 目前他是指针吗？

        // 如果我在渲染的时候，是被锁住的，那我就无法渲染，我需要释放 ，防止出现死锁
        if (ANativeWindow_lock(window, &window_buffer, 0)) {
            ANativeWindow_release(window);
            window = 0;

            pthread_mutex_unlock(&static_mutex); // 解锁，怕出现死锁
            return;
        }

        // 填充window_buffer  画面就出来了
        auto *dst_data = static_cast<uint8_t *>(window_buffer.bits);
        int dst_line_size = window_buffer.stride * 4;

        for (int i = 0; i < window_buffer.height; ++i) { // 图：一行一行显示 [高度不用管，用循环了，遍历高度]
            memcpy(dst_data + i * dst_line_size, src_data + i * src_lineSize, dst_line_size); // OK的
        }

        ANativeWindow_unlockAndPost(window); // 解锁后 并且刷新 window_buffer的数据显示画面
    }

    pthread_mutex_unlock(&static_mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_prepareNative(JNIEnv *env, jobject thiz, jstring data_source) {
    //使用new关键字创建的对象，是在堆中申请的空间。
    auto *helper = new JNICallbackHelper(vm, env, thiz);
    const char *data_source_ = env->GetStringUTFChars(data_source, 0);
    player = new VideoPlayer(data_source_, helper);
    player->setRenderCallback(renderFrame);
    player->prepare();
    env->ReleaseStringUTFChars(data_source, data_source_);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_startNative(JNIEnv *env, jobject thiz) {

    if (player) {
        player->start();
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_stopNative(JNIEnv *env, jobject thiz) {
}

extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_releaseNative(JNIEnv *env, jobject thiz) {
}

/**
 * 实例化ANativeWindow对象
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_setSurfaceNative(JNIEnv *env, jobject thiz, jobject surface) {
    pthread_mutex_lock(&static_mutex);
    // 需要检测上次的surface是否存在，存在需要清除之前的surface窗口。
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    // 创建新的窗口
    window = ANativeWindow_fromSurface(env, surface);
    pthread_mutex_unlock(&static_mutex);
}

/**
 * 获取视频的总时长
 */
extern "C"
JNIEXPORT jint JNICALL
Java_com_lxc_player_VideoPlayer_fetchDurationNative(JNIEnv *env, jobject thiz) {
    if(player) {
        return player->fetch_duration();
    }
    return 0;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_lxc_player_VideoPlayer_seekNative(JNIEnv *env, jobject thiz, jint audio_time) {
    if(player) {
        player->seek(audio_time);
    }
}