#include "AudioChannel.h"

AudioChannel::AudioChannel(int stream_index, AVCodecContext *codecContext)
        : BaseChannel(stream_index, codecContext) {

}

AudioChannel::~AudioChannel() {

}
