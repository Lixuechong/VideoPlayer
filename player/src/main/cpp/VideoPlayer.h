#ifndef VIDEOPLAYER_VIDEOPLAYER_H
#define VIDEOPLAYER_VIDEOPLAYER_H

#include <cstring>
#include <pthread.h>
#include "AudioChannel.h"
#include "VideoChannel.h"
#include "JNICallbackHelper.h"
#include "util.h"

extern "C" {
#include <libavformat/avformat.h>
}

class VideoPlayer {

private:
    char *data_source = 0; // 指针初始化请赋值，否则该指针是野指针。
    pthread_t pid_prepare;
    AudioChannel *audio_channel = 0;
    VideoChannel *video_channel = 0;
    JNICallbackHelper *helper = 0;

public:
    AVFormatContext *formatContext = 0;

    VideoPlayer(const char *data_source, JNICallbackHelper *pHelper);

    ~VideoPlayer();

    void prepare();
    void prepare_();
};


#endif //VIDEOPLAYER_VIDEOPLAYER_H
