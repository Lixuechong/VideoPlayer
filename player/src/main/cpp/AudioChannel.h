#ifndef VIDEOPLAYER_AUDIOCHANNEL_H
#define VIDEOPLAYER_AUDIOCHANNEL_H

#include "BaseChannel.h"

class AudioChannel: public BaseChannel {

public:
    AudioChannel(int, AVCodecContext *);

    virtual ~AudioChannel();
};

#endif //VIDEOPLAYER_AUDIOCHANNEL_H
