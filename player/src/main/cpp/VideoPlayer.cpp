

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
    }
    if (this->helper) {
        delete this->helper;
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
        this->helper->onError("第一步异常", THREAD_CHILD);
        return;
    }

    // 第二步，查询媒体中的音视频流信息
    result = avformat_find_stream_info(formatContext, nullptr);
    if (result < 0) {
        LOGD("第二步异常\n")
        this->helper->onError("第二步异常", THREAD_CHILD);
        return;
    }

    // 此时说明流是一个合格的流媒体。

    // 第三步，根据流信息，流的个数，用循环来找音频流和视频流。
    int i = 0;
    for (; i < this->formatContext->nb_streams; i++) {

        // 第四步，获取媒体流（音频流/视频流/字幕流)
        AVStream *stream = this->formatContext->streams[i];

        // 第五步，从流中获取编码解码的参数(包含了所有的视频、音频、字幕参数)
        AVCodecParameters *parameters = stream->codecpar;

        // 第六步，获取解码器（根据上面的参数）,解码播放，编码封包
        AVCodec *codec = avcodec_find_decoder(parameters->codec_id);

        // 第七步，通过上下文，协助解码器进行播放
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            LOGD("第七步异常\n")
            this->helper->onError("第七步异常", THREAD_CHILD);
            return;
        }

        // 第八步，把流的参数赋值给解码器的上下文。（第七步的codecContext只是创建指针对象，并没有实际内容）
        result = avcodec_parameters_to_context(codecContext, parameters);
        if (result < 0) {
            LOGD("第八步异常\n")
            this->helper->onError("第八步异常", THREAD_CHILD);
            return;
        }

        // 第九步，打开解码器
        result = avcodec_open2(codecContext, codec, 0);
        if (result) {
            LOGD("第九步异常\n")
            this->helper->onError("第九步异常", THREAD_CHILD);
            return;
        }

        // 第十步，从编解码器参数中，获取流的类型
        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) { // 音频流
            this->audio_channel = new AudioChannel();
        }
        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) { // 视频流
            this->video_channel = new VideoChannel();
        }
    }

    // 第十一步，如果流中没有音频也没有视频。（对流进行再次校验）
    if (!this->audio_channel && !this->video_channel) {
        LOGD("第十一步异常\n")
        this->helper->onError("第十一步异常", THREAD_CHILD);
        return;
    }

    // 第十二步，prepare完成。通知Java层。
    if(this->helper) {
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