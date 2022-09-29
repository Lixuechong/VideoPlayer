package com.lxc.player;

import android.text.TextUtils;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

import java.nio.file.NoSuchFileException;

/**
 * Created by XC.Li
 * desc:
 */
public class VideoPlayer implements SurfaceHolder.Callback {

    private final String TAG = VideoPlayer.class.getSimpleName();

    static {
        System.loadLibrary("native-lib");
    }

    private SurfaceHolder surfaceHolder;

    private OnPreparedListener onPreparedListener;
    private OnErrorListener onErrorListener;

    /**
     * 媒体来源
     */
    private String dataSource;

    public VideoPlayer() {
    }

    /**
     * 设置媒体来源
     *
     * @param dataSource 文件路径/媒体流地址
     */
    public void setDataSource(String dataSource) throws NoSuchFileException {
        if (TextUtils.isEmpty(dataSource)) {
            throw new NoSuchFileException("dataSource is must not null.");
        }
        this.dataSource = dataSource;
    }

    /**
     * 播放准备资源
     */
    public void prepare() {
        prepareNative(dataSource);
    }

    /**
     * 开始播放
     */
    public void start() {
        startNative();
    }

    /**
     * 停止播放
     */
    public void stop() {
        stopNative();
    }

    /**
     * 释放资源
     */
    public void release() {
        releaseNative();
    }

    /**
     * 设置准备监听，只有准备完成才可以开始播放
     */
    public void setOnPreparedListener(OnPreparedListener onPreparedListener) {
        this.onPreparedListener = onPreparedListener;
    }

    /**
     * 设置异常监听
     */
    public void setOnErrorListener(OnErrorListener onErrorListener) {
        this.onErrorListener = onErrorListener;
    }

    /**
     * 与surfaceView绑定
     */
    public void bindSurfaceView(SurfaceView surfaceView) {
        if (this.surfaceHolder != null) {
            this.surfaceHolder.removeCallback(this);
        }
        this.surfaceHolder = surfaceView.getHolder();
        this.surfaceHolder.addCallback(this);
    }

    /**
     * 由Jni通过反射调用，通知Java资源已准备
     */
    private void jni_prepared() {
        Log.d(TAG, "_jni_prepared");
        if (onPreparedListener != null) {
            onPreparedListener.onPrepared();
        }
    }

    /**
     * 异常时调用
     *
     * @param error 异常信息
     */
    private void jni_error(String error) {
        Log.d(TAG, "_jni_error msg is " + error);
        if (onErrorListener != null) {
            onErrorListener.onError(error);
        }
    }

    public interface OnPreparedListener {
        void onPrepared();
    }

    public interface OnErrorListener {
        void onError(String msg);
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder holder) {

    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
        setSurfaceNative(holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {

    }

    private native void prepareNative(String dataSource);

    private native void startNative();

    private native void stopNative();

    private native void releaseNative();

    private native void setSurfaceNative(Surface surface);
}
