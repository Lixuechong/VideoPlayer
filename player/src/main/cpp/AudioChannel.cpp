#include "AudioChannel.h"


/**
 * 音频三要素
 * 采样率、位声/采样格式、声道数
 *
 * 音频压缩数据包格式AAC，大部分是44100、32位、双声道。
 *
 */
AudioChannel::AudioChannel(int stream_index, AVCodecContext *codecContext, AVRational time_base)
        : BaseChannel(stream_index, codecContext, time_base) {
    // 缓冲区大小怎么定义？答：涉及到声音三要素。
    // 手机大部分的位声是16bit，2声道，44100.
    // 音频压缩包大部分是32bit，2声道，44100. 32bit的算法运算效率高。（浮点型为什么运算效率高？？？）
    // 所以需要对音频数据进行重采样。

    // 计算设备上双声道类型（AV_CH_LAYOUT_STEREO）的声道数。
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    // 每个采样点的大小为16bit 2byte。
    out_sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    // 声音的采样率。
    out_sample_rate = 44100;

    // 计算最终缓冲区的大小。声道数 * 采样格式 * 采样率。
    out_buffers_size = out_channels * out_sample_size * out_sample_rate;

    // 堆区申请空间，作为缓冲区。
    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));

    // 使用ffmpeg音频重采样。
    swr_ctx = swr_alloc_set_opts(0, // 目前没有上下文，可以传0，也可以传self
            // 下面是输出环节
                                 AV_CH_LAYOUT_STEREO,  // 声道布局类型 双声道
                                 AV_SAMPLE_FMT_S16,  // 采样大小 16bit
                                 out_sample_rate, // 采样率  44100

            // 下面是输入环节
                                 codecContext->channel_layout, // 声道布局类型
                                 codecContext->sample_fmt, // 采样大小
                                 codecContext->sample_rate,  // 采样率
                                 0, 0);
    // 初始化重采样上下文
    swr_init(swr_ctx);
}

AudioChannel::~AudioChannel() {
    if (out_buffers) {
        delete out_buffers;
        out_buffers = 0;
    }
}

void AudioChannel::stop() {

}

void *task_audio_decode(void *args) {
    auto *audio_channel = static_cast<AudioChannel *>(args);
    audio_channel->audio_decode();
    return 0;
}

void AudioChannel::audio_decode() {
    AVPacket *packet = nullptr;

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
//        releaseAVPacket(&packet); // 这里的释放是释放对象本身。


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
        } else if (result != 0) {// 失败
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

void *task_audio_play(void *args) {
    auto *audio_channel = static_cast<AudioChannel *>(args);
    audio_channel->audio_play();
    return 0;
}

/**
 * 回调函数
 *
 * @param bq 缓冲池队列接口
 * @param args 给回调函数的参数
 */
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *args) {
    auto *audio_channel = static_cast<AudioChannel *>(args);

    int pcm_size = 0;
    audio_channel->getPcm(&pcm_size);

    // 添加数据到缓冲区
    (*bq)->Enqueue(bq,
                   audio_channel->out_buffers, // PCM数据(重采样后的数据)
                   pcm_size// PCM数据对应的大小(重采样后的缓冲区大小)
    );
}

/**
 * 音频播放可以使用OpenSL
 * 可以直接在C++层进行播放。
 * 进行了硬件加速。
 */
void AudioChannel::audio_play() {
    SLresult result; // 执行成功或失败的返回值

    // 第一步，创建引擎并获取引擎接口
    // 1.1 创建引擎对象：SLObjectItf
    result = slCreateEngine(&engineObject, 0, 0, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        LOGD("创建sl引擎engineObject失败.")
        return;
    }
    // 1.2 初始化引擎
    result = (*engineObject)->Realize(engineObject, // sl引擎
                                      false); // 延迟等待，等创建成功。（初始化是否异步）
    if (SL_RESULT_SUCCESS != result) {
        LOGD("初始化sl引擎engineObject失败.")
        return;
    }
    // 1.3 获取引擎接口
    result = (*engineObject)->GetInterface(engineObject, // sl引擎
                                           SL_IID_ENGINE, // 引擎id
                                           &engineInterface // 引擎接口
    );
    if (SL_RESULT_SUCCESS != result) {
        LOGD("获取sl引擎接口engineInterface失败.")
        return;
    }
    if (!engineInterface) {
        LOGD("sl引擎接口engineInterface为NULL.")
        return;
    }
    LOGD("获取sl引擎接口engineInterface成功.")

    // 第二步，设置混音器
    // 2.1 创建混音器对象
    result = (*engineInterface)->CreateOutputMix(engineInterface, // 引擎接口
                                                 &outputMixObject, // 混音器对象
                                                 0, // 环境特效
                                                 0, // 混响特效
                                                 0); // 其他什么特效
    if (SL_RESULT_SUCCESS != result) {
        LOGD("创建混音器outputMixObject失败.")
        return;
    }
    // 2.2 初始化混音器对象
    result = (*outputMixObject)->Realize(outputMixObject,  // 混音器对象
                                         false); // 是否异步初始化
    if (SL_RESULT_SUCCESS != result) {
        LOGD("初始化混音器outputMixObject失败.")
        return;
    }

    // 不启用混响可以不用获取混音器接口 【声音的效果】
    // 获得混音器接口
    /*
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                             &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
    // 设置混响 ： 默认。
    SL_I3DL2_ENVIRONMENT_PRESET_ROOM: 室内
    SL_I3DL2_ENVIRONMENT_PRESET_AUDITORIUM : 礼堂 等
    const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
           outputMixEnvironmentalReverb, &settings);
    }
    */
    LOGD("2、设置混音器 Success");

    // 第三步，创建播放器
    // 播放器的声音卡顿和画面卡顿的本质一致。60帧的速率，16ms会从缓冲区获取一次声音数据，如果没有数据那么就会产生卡顿。
    // 3.1 创建缓冲队列buffer。
    SLDataLocator_AndroidSimpleBufferQueue loc_buf = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            10// 10个大小
    };
    // 注，PCM无法直接播放，因为它不包含数据参数（采样率、采样格式等等）。
    // 注，另外需要把声音转换为扬声器支持的格式，所以需要重采样。

    // pcm数据格式 == PCM是不能直接播放，mp3可以直接播放(参数集)，人家不知道PCM的参数
    // SL_DATAFORMAT_PCM：数据格式为pcm格式
    // 2：双声道
    // SL_SAMPLINGRATE_44_1：采样率为44100 （每秒钟采样44100个点，采样率越高失真越小）
    // SL_PCMSAMPLEFORMAT_FIXED_16：采样格式为16bit （每个采样点为16bit，大小）
    // SL_PCMSAMPLEFORMAT_FIXED_16：数据大小为16bit
    // SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT：左右声道（双声道）
    // SL_BYTEORDER_LITTLEENDIAN：小端模式
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, // PCM数据格式
                                   2, // 声道数
                                   SL_SAMPLINGRATE_44_1, // 采样率（每秒44100个点）
                                   SL_PCMSAMPLEFORMAT_FIXED_16, // 每秒采样样本 存放大小 16bit
                                   SL_PCMSAMPLEFORMAT_FIXED_16, // 每个样本位数 16bit
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, // 前左声道  前右声道
                                   SL_BYTEORDER_LITTLEENDIAN}; // 字节序(小端) 例如：int类型四个字节（到底是 高位在前 还是 低位在前 的排序方式，一般我们都是小端）

    // 数据源 将上述配置信息放到这个数据源中
    // audioSrc最终配置音频信息的成果，给后面代码使用
    SLDataSource audio_src = {&loc_buf, &format_pcm};
    // 3.2 配置音轨（输出）
    // 设置混音器
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX,
                                          outputMixObject}; // SL_DATALOCATOR_OUTPUTMIX:输出混音器类型
    SLDataSink audioSnk = {&loc_outmix, NULL}; // outmix最终混音器的成果，给后面代码使用
    // 需要的接口 操作队列的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    // 3.3 创建播放器 SLObjectItf bqPlayerObject
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, // 参数1：引擎接口
                                                   &bqPlayerObject, // 参数2：播放器
                                                   &audio_src, // 参数3：音频配置信息
                                                   &audioSnk, // 参数4：混音器

            // 下面代码都是 打开队列的工作
                                                   1, // 参数5：开放的参数的个数
                                                   ids,  // 参数6：代表我们需要 Buff
                                                   req // 参数7：代表我们上面的Buff 需要开放出去
    );
    if (SL_RESULT_SUCCESS != result) {
        LOGD("创建播放器bqPlayerObject失败.");
        return;
    }
    // 3.4 初始化播放器
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGD("初始化播放器bqPlayerObject失败.");
        return;
    }
    // 3.5 获取播放器接口 【以后播放全部使用 播放器接口（核心）】
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY,
                                             &bqPlayerPlay); // SL_IID_PLAY:播放接口 == iplayer
    if (SL_RESULT_SUCCESS != result) {
        LOGD("获取播放接口 GetInterface SL_IID_PLAY failed!");
        return;
    }

    // 第四步，把播放器队列和声卡缓冲池队列进行绑定。方便我们向播放器队列添加数据时，缓冲池可以通过回调获取播放器队列中的数据。

    // 4.1 获取播放器队列接口对象：SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue  // 播放需要的队列
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    if (result != SL_RESULT_SUCCESS) {
        LOGD("绑定播放队列 GetInterface SL_IID_BUFFERQUEUE failed!");
        return;
    }
    // 4.2 给播放器队列接口对象设置回调函数。
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue,  // 传入刚刚设置好的队列
                                             bqPlayerCallback,  // 回调函数
                                             this); // 给回调函数的参数

    // 第五步，设置播放器状态为播放状态。
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);

    // 第六步，手动激活回调函数 (需要手动激活才可以让声卡驱动转起来。)
    bqPlayerCallback(bqPlayerBufferQueue, this);
}

void AudioChannel::start() {
    is_playing = true;

    packets.working(true);
    frames.working(true);

    // 该线程用于从packet队列取出压缩包，进行解码。解码后再次放入frame队列。(pcm格式)
    pthread_create(&pid_audio_decode, 0, task_audio_decode, this);

    // 该线程用于从frame队列中取出解码包，播放。
    pthread_create(&pid_audio_play, 0, task_audio_play, this);
}

/**
 * 计算重采样后的缓冲区大小, 并给out_buffers赋值。
 * @param p_int
 */
void AudioChannel::getPcm(int *p_int) {
    int pcm_data_size;
    // 从frames队列中获取PCM数据。此时数据为PCM格式，并未重采样。
    AVFrame *frame = nullptr;
    while (is_playing) {
        int ret = frames.popQueueAndDel(frame);
        if (!is_playing) {
            break; // 如果关闭了播放，跳出循环，releaseAVPacket(&pkt);
        }
        if (!ret) { // ret == 0
            continue; // 哪怕是没有成功，也要继续（假设：你生产太慢(原始包加入队列)，我消费就等一下你）
        }

        // 开始重采样

        // 来源：10个48000   ---->  目标:44100  11个44100
        // 获取单通道的采样点数（j即为一帧的采样点数） (计算目标样本数： ？ 10个48000 --->  48000/44100因为除不尽  11个44100)
        int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, frame->sample_rate) +
                                            frame->nb_samples, // 获取下一个输入样本相对于下一个输出样本将经历的延迟
                                            out_sample_rate, // 输出采样率
                                            frame->sample_rate, // 输入采样率
                                            AV_ROUND_UP); // 先上取 取去11个才能容纳的上

        // pcm的处理逻辑
        // 音频播放器的数据格式是我们自己在下面定义的
        // 而原始数据（待播放的音频pcm数据）
        // TODO 重采样工作
        // 返回的结果：每个通道输出的样本数(注意：是转换后的)    做一个简单的重采样实验(通道基本上都是:1024)
        int samples_per_channel = swr_convert(swr_ctx,
                // 下面是输出区域
                                              &out_buffers,  // 【成果的buff】  重采样后的
                                              dst_nb_samples, // 【成果的 单通道的样本数 无法与out_buffers对应，所以有下面的pcm_data_size计算】
                // 下面是输入区域
                                              (const uint8_t **) frame->data, // 队列的AVFrame * 拿的  PCM数据 未重采样的
                                              frame->nb_samples); // 输入的样本数

        // 由于out_buffers 和 dst_nb_samples 无法对应，所以需要重新计算
        pcm_data_size = samples_per_channel * out_sample_size *
                        out_channels; // 941通道样本数  *  2样本格式字节数  *  2声道数  =3764
        *p_int = pcm_data_size;

        audio_time = frame->best_effort_timestamp * av_q2d(time_base);

        break;
    }

    av_frame_unref(frame);
    releaseAVFrame(&frame);
}
