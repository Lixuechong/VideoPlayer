//
// Created by lixuechong on 2022/9/22.
//

#ifndef VIDEOPLAYER_LOG_H
#define VIDEOPLAYER_LOG_H

#include <android/log.h>

#define TAG "Lxc_log" // __VA_ARGS__ 代表...的可变参数
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__);

#endif //VIDEOPLAYER_LOG_H
