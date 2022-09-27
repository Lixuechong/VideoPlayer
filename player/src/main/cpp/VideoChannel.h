//
// Created by lixuechong on 2022/9/21.
//

#ifndef VIDEOPLAYER_VIDEOCHANNEL_H
#define VIDEOPLAYER_VIDEOCHANNEL_H


#include "BaseChannel.h"

class VideoChannel : public BaseChannel {

private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;

public:
    VideoChannel(int, AVCodecContext *);

    virtual ~VideoChannel();

    void stop();

    void start();

    void video_decode();

    void video_play();
};


#endif //VIDEOPLAYER_VIDEOCHANNEL_H
