
#include "VideoPlayer.h"

VideoPlayer::VideoPlayer(const char *data_source, JNICallbackHelper *helper) {
    // 这里的赋值在栈中，当jni函数弹栈后，会被回收。this->data_source指针会悬空。所以需要深拷贝。
//    this->data_source = data_source;

    this->data_source = new char[strlen(data_source) + 1];
    strcpy(this->data_source, data_source);

    this->helper = helper;
}

VideoPlayer::~VideoPlayer() {
    if (this->data_source) {
        delete this->data_source;
        this->data_source = nullptr;
    }
    if (this->helper) {
        delete this->helper;
        this->helper = nullptr;
    }
}

/**
 * 子线程回调的函数
 */
void *task_prepare(void *args) {

    auto *player = static_cast<VideoPlayer *>(args);
    player->prepare_();
    return nullptr; // 必须返回数据
}

/**
 * 该函数在子线程中执行
 *
 * ffmpeg大量使用了上下文context，因为它是C语言，没有对象的概念，所有的内容全部都存在于上下文中，保证自始至终使用的都是同一个成员。
 */
void VideoPlayer::prepare_() {
    // 第一步，打开媒体地址（文件路径，rtmp地址）
    formatContext = avformat_alloc_context(); // 使用自带的api开辟上下文。
    AVDictionary *dictionary = nullptr;
    av_dict_set(&dictionary, "timeout", "5000000", 0);
    int result = avformat_open_input(&formatContext, this->data_source, nullptr, &dictionary);
    av_dict_free(&dictionary); // 释放字典,自我理解是把字典中的内容赋值到上下文中，所以此处不需要字典了。
    if (result) {
        LOGD("第一步异常\n")
        char *error_info = av_err2str(result);
        this->helper->onError(error_info, THREAD_CHILD);
        return;
    }

    // 第二步，查询媒体中的音视频流信息
    result = avformat_find_stream_info(formatContext, nullptr);
    if (result < 0) {
        LOGD("第二步异常\n")
        char *error_info = av_err2str(result);
        this->helper->onError(error_info, THREAD_CHILD);
        return;
    }

    // 此时说明流是一个合格的流媒体。

    // 第三步，根据流信息，流的个数，用循环来找音频流和视频流。
    int stream_index = 0;
    for (; stream_index < this->formatContext->nb_streams; stream_index++) {

        // 第四步，获取媒体流（音频流/视频流/字幕流)
        AVStream *stream = this->formatContext->streams[stream_index];

        // 第五步，从流中获取编码解码的参数(包含了所有的视频、音频、字幕参数)
        AVCodecParameters *parameters = stream->codecpar;

        // 第六步，获取解码器（根据上面的参数）,解码播放，编码封包
        AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
        if (!codec) {
            LOGD("第六步异常\n")
            char *error_info = av_err2str(result);
            this->helper->onError(error_info, THREAD_CHILD);
        }

        // 第七步，通过上下文，协助解码器进行播放
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            LOGD("第七步异常\n")
            char *error_info = av_err2str(result);
            this->helper->onError(error_info, THREAD_CHILD);
            return;
        }

        // 第八步，把流的参数赋值给解码器的上下文。（第七步的codecContext只是创建指针对象，并没有实际内容）
        result = avcodec_parameters_to_context(codecContext, parameters);
        if (result < 0) {
            LOGD("第八步异常\n")
            char *error_info = av_err2str(result);
            this->helper->onError(error_info, THREAD_CHILD);
            return;
        }

        LOGD("流类型 %d \n", parameters->codec_type)

        if (parameters->codec_type != AVMediaType::AVMEDIA_TYPE_AUDIO
            && parameters->codec_type != AVMediaType::AVMEDIA_TYPE_VIDEO) {
            continue;
        }

        // 第九步，打开解码器
        result = avcodec_open2(codecContext, codec, nullptr);
        if (result) {
            LOGD("第九步异常码:%d,流类型:%d,音频流编码:%d,视频流编码:%d\n", result, parameters->codec_type,
                 AVMediaType::AVMEDIA_TYPE_AUDIO, AVMediaType::AVMEDIA_TYPE_VIDEO)
            char *error_info = av_err2str(result);
            this->helper->onError(error_info, THREAD_CHILD);
            return;
        }

        // 第十步，从编解码器参数中，获取流的类型
        // 注意：this->audio_channel == nullptr此处判空，主要用于防止重复创建。媒体流的类型可能重复。
        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO
            && this->audio_channel == nullptr) { // 音频流
            this->audio_channel = new AudioChannel(stream_index, codecContext);
        }
        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO
            && this->video_channel == nullptr) { // 视频流
            this->video_channel = new VideoChannel(stream_index, codecContext);
            this->video_channel->setRenderCallback(this->renderCallback);
        }
    }

    // 第十一步，如果流中没有音频也没有视频。（对流进行再次校验）
    if (!this->audio_channel && !this->video_channel) {
        LOGD("第十一步异常\n")
        char *error_info = av_err2str(result);
        this->helper->onError(error_info, THREAD_CHILD);
        return;
    }

    // 第十二步，prepare完成。通知Java层。
    if (this->helper) {
        LOGD("prepare完成\n")
        this->helper->onPrepared(THREAD_CHILD);
    }
}

/**
 * 该函数是在主线程中执行，对资源进行解封装需要开辟子线程。
 */
void VideoPlayer::prepare() {
    // data_source 是一个文件io流，或者rtmp流，必须使用子线程。
    pthread_create(&pid_prepare, nullptr, task_prepare, this);
}


/**
 * 子线程回调的函数
 */
void *task_start(void *args) {

    auto *player = static_cast<VideoPlayer *>(args);
    player->start_();
    return nullptr; // 必须返回数据
}

void VideoPlayer::start_() {

    // 第一步，把媒体压缩包保存到对应的数据队列中.
    // 注意：如果音频采样率较高（单通道采样数为1024），视频帧率较低时，此时音频包的生产速度大于视频包生产速度。
    while (is_playing) {

        // 判断如果生产视频编码包太快，超过阈值，则让生产队列等待。
        bool is_limit = false;
        if (video_channel) {
            video_channel->beyondLimitsWithPackets(&is_limit);
        }

        if (is_limit) {
            av_usleep(2 * 1000); // 单位微秒
            continue;
        }

        if (audio_channel) {
            audio_channel->beyondLimitsWithPackets(&is_limit);
        }
        if (is_limit) {
            av_usleep(2 * 1000); // 单位微秒
            continue;
        }

        LOGD("audio_channel size %d, is limit %d, video_channel size %d\n",
             audio_channel->packets.size(), is_limit, video_channel->packets.size())

        // AVPacket 是压缩包的类型(音频和视频的帧数据，都在这个包中)
        AVPacket *packet = av_packet_alloc();
        // 此时，formatContext中存在了流媒体的数据源，可以直接读取。
        int result = av_read_frame(this->formatContext, packet); // 从媒体中读取音/视频包.
        if (!result) { // if(result) 表示 if(result != null)

            // 把AVPacket假如队列，提前区分音频和视频，加入不同的数据队列

            // if条件表示为视频
            if (video_channel->stream_index == packet->stream_index) {
                video_channel->packets.insertToQueue(packet);
            }

            // if条件表示为音频
            if (audio_channel->stream_index == packet->stream_index) {
                audio_channel->packets.insertToQueue(packet);
            }
        } else if (result == AVERROR_EOF) { // 表示流媒体读取完毕
            // 但并不代表播放完成。
            // 此处文件读取到末尾时，判断编码包队列中是否还存在数据，如果没有数据表示视频已经播放完毕。此时退出while循环。
            if (video_channel->frames.empty() && audio_channel->frames.empty()) {
                break;
            }
        } else {
            break; //av_read_frame出现异常。
        }
    }
    is_playing = false;
    if (video_channel) {
        video_channel->stop();
    }
    if (audio_channel) {
        audio_channel->stop();
    }

}

void VideoPlayer::start() {

    is_playing = true;

    // 第二步，开启播放。
    if (video_channel) {
        video_channel->start();
    }

    if (audio_channel) {
        audio_channel->start();
    }

    // 开启线程把压缩包放入压缩队列
    pthread_create(&pid_start, 0, task_start, this);

}

void VideoPlayer::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}


