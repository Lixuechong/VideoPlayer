#ifndef VIDEOPLAYER_SAFEQUEUE_H
#define VIDEOPLAYER_SAFEQUEUE_H

#include <queue>
#include <pthread.h>

using namespace std;

template<typename T> // 定义泛型
class SafeQueue {

private:
    typedef void (*ReleaseCallback)(T *);// 函数指针定义 作为回调
    typedef void (*SyncCallback)(queue<T> &);// 函数指针定义 作为回调 用来完成丢帧工作

private:
    queue<T> queue; // 队列
    pthread_mutex_t mutex; // 互斥锁，用于线程安全。
    pthread_cond_t cond; // 条件变量，用于控制线程睡眠和唤醒。
    bool work = false; // 标记队列是否工作
    ReleaseCallback releaseCallback;
    SyncCallback syncCallback;

public:

    SafeQueue() {
        pthread_mutex_init(&mutex, 0); // 初始化互斥锁
        pthread_cond_init(&cond, 0); // 初始化条件变量
    }

    virtual ~SafeQueue() {
        pthread_mutex_destroy(&mutex); // 释放互斥锁
        pthread_cond_destroy(&cond); // 释放条件变量
    }

    /**
     * 数据入队 [AVPacket 类型为压缩包] [AVFrame 类型为解压包]
     */
    void insertToQueue(T value) {
        pthread_mutex_lock(&mutex); // 锁住代码块

        if (work) {
            queue.push(value);
            pthread_cond_signal(&cond); // 当插入数据包 进入队列后，发出通知唤醒其他线程
        } else {
            // 没有工作时，释放value的空间，由于是T类型，类型不明确，所以由外界释放。
            if (releaseCallback) {
                releaseCallback(&value);
            }
        }

        pthread_mutex_unlock(&mutex); // 解锁代码块
    }

    /**
    * 数据出队 [AVPacket 类型为压缩包] [AVFrame 类型为解压包]
     *
     * @return 取数据是否成功
    */
    bool popQueueAndDel(T &value) {
        int ret = false;

        pthread_mutex_lock(&mutex); // 锁住代码块

        while (work && queue.empty()) {
            pthread_cond_wait(&cond, &mutex);// 没有数据的情况下，该线程让出锁并进入睡眠。
        }

        if (!queue.empty()) {
            value = queue.front(); // 获取队列中第一个数据
            queue.pop(); // 删除队列中第一个条数据
            ret = true;
        }

        pthread_mutex_unlock(&mutex); // 解锁代码块

        return ret;
    }

    /**
     * 设置工作状态，设置队列是否工作
     */
    void working(bool working) {
        pthread_mutex_lock(&mutex);
        this->work = working;
        pthread_cond_signal(&cond);// 唤醒其他线程开始工作。
        pthread_mutex_unlock(&mutex);
    }

    int empty() {
        return queue.empty();
    }

    int size() {
        return queue.size();
    }

    /**
     * 清除队列数据
     */
    void clear() {

        pthread_mutex_lock(&mutex);

        int i = 0;
        unsigned int size = queue.size();
        for (; i < size; i++) {
            // 循环释放队列中的数据
            T value = queue.front();
            if (releaseCallback) {
                releaseCallback(&value);
            }
            queue.pop();
        }
        pthread_mutex_unlock(&mutex);
    }

    void setReleaseCallback(ReleaseCallback releaseCallback) {
        this->releaseCallback = releaseCallback;
    }

    void setSyncCallback(SyncCallback callback) {
        this->syncCallback = callback;
    }

    /**
     * 同步操作 丢包
     */
     void sync() {
        pthread_mutex_lock(&mutex);

        if(syncCallback) {
            syncCallback(queue);
        }

        pthread_mutex_unlock(&mutex);
     }

};

#endif //VIDEOPLAYER_SAFEQUEUE_H
