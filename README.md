# VideoPlayer
视频播放器客户端。（可以播放网络流数据<rtmp>，也可以播放本地文件）

## 采用 C++ + FFmpeg 完成。

### 音视频渲染：
1，新增一个数据队列，存放编码包和解码包。
    两个线程处理，一个线程存放编码包；一个线程取出编码包解码，再放回队列。
2，使用第三个线程播放解码包。