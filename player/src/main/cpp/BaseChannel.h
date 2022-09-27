#ifndef VIDEOPLAYER_BASECHANNEL_H
#define VIDEOPLAYER_BASECHANNEL_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
};

#include "SafeQueue.h"

class BaseChannel {
public:
    // VideoPlayer.cpp prepare的第三步.formatContext->nb_streams
    int stream_index; // 音/视频的下标 ，在使用for循环时获取的数据流的类型。是这个流的下标，并不是一帧的下标。
    SafeQueue<AVPacket *> packets; // 压缩包队列
    SafeQueue<AVFrame *> frames; // 解压包队列
    bool is_playing;
    AVCodecContext *codecContext = 0; // 音/视频解码器的上下文

    BaseChannel(int streamIndex, AVCodecContext *codecContext) : stream_index(streamIndex),
                                                                 codecContext(codecContext) {

        packets.setReleaseCallback(releaseAVPacket);
        frames.setReleaseCallback(releaseAVFrame);
    }

    virtual ~BaseChannel() {
        packets.clear();
        frames.clear();
    }

    /**
     * 释放队列中的AVPacket
     * @param p
     */
    static void releaseAVPacket(AVPacket **p) {
        if(p) {
            av_packet_free(p); // 释放队列中的T
            *p = 0;
        }
    }

    /**
     * 释放队列中的AVPacket
     * @param p
     */
    static void releaseAVFrame(AVFrame **p) {
        if(p) {
            av_frame_free(p); // 释放队列中的T
            *p = 0;
        }
    }
};

#endif //VIDEOPLAYER_BASECHANNEL_H
