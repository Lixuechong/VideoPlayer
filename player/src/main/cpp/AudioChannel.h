#ifndef VIDEOPLAYER_AUDIOCHANNEL_H
#define VIDEOPLAYER_AUDIOCHANNEL_H

#include "BaseChannel.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "Log.h"

extern "C" {
#include <libswresample/swresample.h> // 对pcm数据进行转换（重采样）？？？
};

class AudioChannel : public BaseChannel {

private:
    pthread_t pid_audio_decode;
    pthread_t pid_audio_play;

public:
    int out_channels;
    int out_sample_size;
    int out_sample_rate;
    int out_buffers_size;
    uint8_t *out_buffers = 0; // 无符号int类型

    SwrContext * swr_ctx = 0;

    double audio_time; // 音频时间戳,当前播放的时间戳。

public:
    //引擎
    SLObjectItf engineObject = 0;
    // 引擎接口
    SLEngineItf engineInterface = 0;
    // 混音器
    SLObjectItf outputMixObject = 0;
    // 播放器
    SLObjectItf bqPlayerObject = 0;
    // 播放器接口
    SLPlayItf bqPlayerPlay = 0;

    // 播放器队列接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = 0;

public:
    AudioChannel(int, AVCodecContext *, AVRational);

    virtual ~AudioChannel();

    void stop();

    void start();


    void audio_decode();

    void audio_play();

    void getPcm(int *);
};

#endif //VIDEOPLAYER_AUDIOCHANNEL_H
