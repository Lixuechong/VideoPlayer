#ifndef VIDEOPLAYER_VIDEOPLAYER_H
#define VIDEOPLAYER_VIDEOPLAYER_H

#include <cstring>
#include <pthread.h>
#include "AudioChannel.h"
#include "VideoChannel.h"
#include "JNICallbackHelper.h"
#include "util.h"
#include "Log.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}


class VideoPlayer {

private:
    char *data_source = 0; // 指针初始化请赋值，否则该指针是野指针。
    pthread_t pid_prepare;
    pthread_t pid_start;
    AudioChannel *audio_channel = 0;
    VideoChannel *video_channel = 0;
    JNICallbackHelper *helper = 0;
    bool is_playing = false; // 是否播放
    RenderCallback renderCallback;
    int duration; // 视频总时长

    pthread_mutex_t seek_mutex; // 改变进度的锁

public:
    AVFormatContext *formatContext = 0;

    VideoPlayer(const char *data_source, JNICallbackHelper *pHelper);

    ~VideoPlayer();

    void prepare();

    void prepare_();

    void start();

    void start_();

    void setRenderCallback(RenderCallback renderCallback);

    int fetch_duration();

    void seek(int);
};


#endif //VIDEOPLAYER_VIDEOPLAYER_H
