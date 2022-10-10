

#include "VideoChannel.h"

/**
 * 这里的解码包不需要考虑I帧的问题。
 * @param q
 */
void task_drop_frame(queue<AVFrame *> &q) {
    if (!q.empty()) {
        AVFrame *frame = q.front();
        BaseChannel::releaseAVFrame(&frame);
        q.pop();

    }
}

void task_drop_packet(queue<AVPacket *> &q) {
    while (!q.empty()) {
        AVPacket *packet = q.front();
        if (packet->flags != AV_PKT_FLAG_KEY) { // 如果不是I帧
            BaseChannel::releaseAVPacket(&packet);
            q.pop();
        } else {
            break;
        }
    }
}

VideoChannel::VideoChannel(int stream_index, AVCodecContext *codecContext,
                           AVRational time_base, int fps)
        : BaseChannel(stream_index, codecContext, time_base) {
    this->fps = fps;
    packets.setSyncCallback(task_drop_packet);
    frames.setSyncCallback(task_drop_frame);
}

VideoChannel::~VideoChannel() {

}

void VideoChannel::stop() {
    //todo
}

void *task_video_decode(void *args) {
    auto *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_decode();
    return 0;
}

/**
 * 运行在子线程,解码
 */
void VideoChannel::video_decode() {
    AVPacket *packet = 0;

    while (is_playing) {
        bool is_limit = false;
        if (is_playing) {
            beyondLimitsWithFrames(&is_limit);
        }
        if (is_limit) {
            av_usleep(2 * 1000); // 单位微秒
            continue;
        }

        int result = packets.popQueueAndDel(packet); // 阻塞式队列

        if (!is_playing) { // 用户停止播放,跳出循环并释放资源。
            break;
        }

        if (!result) {
            continue;
        }

        // 把packet解码成frame，需要把packet给ffmpeg内的缓冲区，再获取frame。
        result = avcodec_send_packet(codecContext, packet);

        //avcodec_send_packet 会把packet进行深拷贝，所以可以直接在这里释放。
//        releaseAVPacket(&packet);

        if (result != 0) {
            //这里各种异常
            break;
        }

        AVFrame *frame = av_frame_alloc();
        result = avcodec_receive_frame(codecContext, frame);

        // 进行异常判断
        if (result == AVERROR(EAGAIN)) {
            // IBP的理论???
            // 当不是关键帧时，无法通过单独的一帧解码。所以可以继续，参考下一帧进行解码。
            continue;
        } else if (result != 0) { // 失败
            if (frame) {
                av_frame_unref(frame);
                releaseAVFrame(&frame);
            }
            break;
        }

        frames.insertToQueue(frame);

        // 此时把packet完全释放。释放packet对象，和packet成员指向的空间。 先释放内部成员，再释放本身。
        av_packet_unref(packet); // AVPacket对象成员中，也存在开辟堆空间的指针，所以需要用api把对象成员的堆空间释放。
        releaseAVPacket(&packet);
    }

    is_playing = false;
    av_packet_unref(packet);
    releaseAVPacket(&packet);
}

void *task_video_play(void *args) {
    auto *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_play();
    return 0;
}

/**
 * 运行在子线程,播放
 *
 * 需要对解码包的数据格式进行转换。
 * 解码包的数据格式为YUV（视频），而android屏幕使用的是RGBA，所以需要先把解码包进行格式转换后再进行播放。
 * 使用libswscale
 */
void VideoChannel::video_play() {
    AVFrame *frame = 0;
    uint8_t *dst_data[4];//转换后的像素数据。二维数组，每个像素点有四个元素，数据分别为RGBA，范围为0-255
    int dst_line_size[4];//转换后的像素大小。二维数组.

    // 在堆中申请空间
    av_image_alloc(dst_data, dst_line_size,
                   codecContext->width, codecContext->height,
                   AV_PIX_FMT_RGBA,
                   1);

    SwsContext *sws_context = sws_getContext(
            codecContext->width, // 视频输入的宽
            codecContext->height, // 视频输入的高
            codecContext->pix_fmt, // 视频输入的像素格式，80%的视频格式是 AV_PIX_FMT_YUV420P
            codecContext->width, // 视频输出的宽
            codecContext->height, // 视频输出的高
            AV_PIX_FMT_RGBA, // 视频输出的格式
            SWS_BILINEAR, // 格式转换的算法，各种类型，需要学习.
            NULL, NULL, NULL
    );
    while (is_playing) {
        int result = frames.popQueueAndDel(frame);
        if (!is_playing) { // 用户停止播放,跳出循环并释放资源。
            break;
        }

        if (!result) {
            continue;
        }

        // 格式转换
        sws_scale(sws_context,
                  frame->data, // 输入渲染一行的数据
                  frame->linesize, // 输入渲染一行的大小
                  0, // 输入渲染一行的宽度，一般为0
                  codecContext->height, // 输入渲染一行的高度
                  dst_data, // 输出渲染的数据
                  dst_line_size // 输出渲染的大小
        );

        // 把rgba渲染到屏幕上。
        // 如何渲染一帧图像？
        // 答：需要 宽/高/数据

        // 把渲染数据传递给player.cpp
        // 在回调渲染之前，对视频帧进行数据进度矫正。
        // 加入FPS间隔时间。
        // 额外延时时间（在之前编码时，帧之间的延时时间）
        double extra_delay = frame->repeat_pict / (2 * fps); // 可能获取不到(编码时没有加入额外延时)。
        // fps延时时间 (计算每一帧的延时时间)
        double fps_delay = 1.0 / fps;
        // 当前帧的延时时间
        double real_delay = fps_delay + extra_delay;
        // 当前帧间隔. 这里只是根据视频的fps，进行间隔，与音频并不同步。
//        av_usleep(real_delay * 1000000);

        // 与音频同步

        // 获取音视频的当前帧时间戳
        double video_time = frame->best_effort_timestamp * av_q2d(time_base);
        double audio_time = audio_channel->audio_time;
        // 定义差值
        double time_diff = video_time - audio_time;
        // 判断两个时间差值
        if (time_diff > 0) { // 视频播放相对音频较快
            if (time_diff > 1) { // 视频播放速度比音频播放速度间隔大于1s(差距大)
                av_usleep((real_delay * 2) * 1000000);
            } else { // 视频播放速度比音频播放速度间隔小于1s(差距小)
                av_usleep((real_delay + time_diff) * 1000000);
            }
        } else if (time_diff < 0) { // 音频播放相对视频较快
            // 采用丢弃视频帧的方式，追赶音频的播放进度。
            // 注意：不能随意丢弃，如果丢弃的是I帧，那么就会出现花屏。
            // 当需要丢弃的进度非常小时，可以丢弃。因为这几个需要被丢弃的帧和当前显示的帧内容非常相近。
            // 那么这个进度是多少呢？经验值为0.05个进度。
            if (fabs(time_diff) <= 0.05) { // fabs()把数值取绝对值。
                // 此处丢包时，涉及到多线程。

                frames.sync();

                continue;
            }
        } else { // 音视频播放完全同步

        }

        renderCallback(dst_data[0],  // 数组被传递会退化成指针
                       codecContext->width,
                       codecContext->height,
                       dst_line_size[0]);

        av_frame_unref(frame);
        releaseAVFrame(&frame); // 此处不考虑回退，所以渲染完成后可以直接被释放。
    }

    av_frame_unref(frame);
    releaseAVFrame(&frame);
    sws_freeContext(sws_context);
    is_playing = false;
    av_free(dst_data);
    av_free(dst_line_size);
}

void VideoChannel::start() {
    is_playing = true;

    // 队列开始工作
    packets.working(true);
    frames.working(true);

    // 该线程用于从packet队列取出压缩包，进行解码。解码后再次放入frame队列。(yuv格式)
    pthread_create(&pid_video_decode, 0, task_video_decode, this);

    // 该线程用于从frame队列中取出解码包，播放。
    pthread_create(&pid_video_play, 0, task_video_play, this);
}

void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

void VideoChannel::setAudioChannel(AudioChannel *channel) {
    this->audio_channel = channel;
}
