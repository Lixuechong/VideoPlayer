

#include "VideoChannel.h"

VideoChannel::VideoChannel(int stream_index, AVCodecContext *codecContext)
        : BaseChannel(stream_index, codecContext) {

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
        releaseAVPacket(&packet);
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
        } else if (result != 0) {
            break;
        }

        frames.insertToQueue(frame);
    }

    is_playing = false;

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
        renderCallback(dst_data[0],  // 数组被传递会退化成指针
                       codecContext->width,
                       codecContext->height,
                       dst_line_size[0]);

        releaseAVFrame(&frame); // 此处不考虑回退，所以渲染完成后可以直接被释放。
    }

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
