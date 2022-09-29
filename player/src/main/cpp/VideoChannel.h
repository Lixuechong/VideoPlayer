//
// Created by lixuechong on 2022/9/21.
//

#ifndef VIDEOPLAYER_VIDEOCHANNEL_H
#define VIDEOPLAYER_VIDEOCHANNEL_H


#include "BaseChannel.h"

typedef void(*RenderCallback)(uint8_t *, int, int, int);// 定义函数指针，用于渲染视频时返回必须参数。

class VideoChannel : public BaseChannel {

private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback renderCallback;

public:
    VideoChannel(int, AVCodecContext *);

    virtual ~VideoChannel();

    void stop();

    void start();

    void video_decode();

    void video_play();

    void setRenderCallback(RenderCallback renderCallback);
};


#endif //VIDEOPLAYER_VIDEOCHANNEL_H
