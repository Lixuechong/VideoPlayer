#ifndef VIDEOPLAYER_BASECHANNEL_H
#define VIDEOPLAYER_BASECHANNEL_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
};

#include "SafeQueue.h"

#define MAX_SIZE_QUEUE 100

class BaseChannel {

private:
    int zero_count = 0; // 视频队列数量为0的循环次数.
public:
    // VideoPlayer.cpp prepare的第三步.formatContext->nb_streams
    int stream_index; // 音/视频的下标 ，在使用for循环时获取的数据流的类型。是这个流的下标，并不是一帧的下标。
    SafeQueue<AVPacket *> packets; // 压缩包队列
    SafeQueue<AVFrame *> frames; // 解压包队列
    bool is_playing;
    AVCodecContext *codecContext = 0; // 音/视频解码器的上下文

    AVRational time_base; // 时间基

    BaseChannel(int streamIndex, AVCodecContext *codecContext, AVRational time_base) :
            stream_index(streamIndex),
            codecContext(codecContext),
            time_base(time_base) {

        packets.setReleaseCallback(releaseAVPacket);
        frames.setReleaseCallback(releaseAVFrame);
    }

    virtual ~BaseChannel() {
        packets.clear();
        frames.clear();
    }

    /**
     * 视频压缩包是否采集完成
     */
    void isEmptyVideoQueue(bool *finished) {
        if (zero_count >= MAX_SIZE_QUEUE) {
            *finished = true;
        } else {
            if (packets.size() == 0) {
                zero_count++;
            }
            *finished = false;
        }
    }

    /**
     * 压缩包队列是否超过阈值
     */
    void beyondLimitsWithPackets(bool *limit) {
        if (packets.size() >= MAX_SIZE_QUEUE) {
            *limit = true;
        } else {
            *limit = false;
        }
    }

    /**
     * 解码包队列是否超过阈值
     */
    void beyondLimitsWithFrames(bool *limit) {
        if (frames.size() >= MAX_SIZE_QUEUE) {
            *limit = true;
        } else {
            *limit = false;
        }
    }

    /**
     * 释放队列中的AVPacket
     * @param p
     */
    static void releaseAVPacket(AVPacket **p) {
        if (p) {
            av_packet_free(p); // 释放队列中的T
            *p = 0;
        }
    }

    /**
     * 释放队列中的AVPacket
     * @param p
     */
    static void releaseAVFrame(AVFrame **p) {
        if (p) {
            av_frame_free(p); // 释放队列中的T
            *p = 0;
        }
    }
};

#endif //VIDEOPLAYER_BASECHANNEL_H
