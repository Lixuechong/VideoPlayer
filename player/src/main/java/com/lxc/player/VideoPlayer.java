package com.lxc.player;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewParent;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.nio.file.NoSuchFileException;

/**
 * Created by XC.Li
 * desc:
 */
public class VideoPlayer extends LinearLayout implements SurfaceHolder.Callback, SeekBar.OnSeekBarChangeListener {

    private final String TAG = VideoPlayer.class.getSimpleName();

    private final int HANDLE_STATUS_PREPARED = 1; // 视频已准备
    private final int HANDLE_STATUS_PROGRESS = 10; // 当前进度更新

    static {
        System.loadLibrary("native-lib");
    }

    private SurfaceHolder surfaceHolder;

    private OnPreparedListener onPreparedListener;
    private OnErrorListener onErrorListener;

    private Handler handler;
    private HandleMessage message;

    /**
     * 媒体来源
     */
    private String dataSource;

    private View seekBox; // 拖动条的容器
    private SeekBar seekBar; // 拖动条
    private TextView timeView; // 显示播放时间
    private boolean dragged; // 是否拖拽了拖动条

    private int duration; // 播放总时长

    public VideoPlayer(Context context) {
        this(context, null);
    }

    public VideoPlayer(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public VideoPlayer(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        initView(context);
    }

    private void initView(Context context) {
        View content = LayoutInflater.from(context).inflate(R.layout.view_video_player, null);
        setVerticalGravity(LinearLayout.VERTICAL);
        ViewParent parent = content.getParent();
        if (parent != null) {
            removeAllViews();
        }
        addView(content);

        SurfaceView surfaceView = content.findViewById(R.id.surfaceView);
        seekBox = content.findViewById(R.id.process_box);
        seekBar = content.findViewById(R.id.seekBar);
        timeView = content.findViewById(R.id.tv_time);
        seekBar.setOnSeekBarChangeListener(this);

        dragged = false;
        surfaceHolder = surfaceView.getHolder();
        message = new HandleMessage();
        handler = new Handler(Looper.getMainLooper(), message);
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
        bindSurfaceHolder();
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
        message = null;
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
    private void bindSurfaceHolder() {
        if (this.surfaceHolder != null) {
            this.surfaceHolder.removeCallback(this);
        }
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
        duration = fetchDurationNative();
        Log.d(TAG, "时长= " + duration);
        sendMessage(HANDLE_STATUS_PREPARED);
    }

    private void jni_progress(int progress) {
        if (!dragged) {
            sendMessage(HANDLE_STATUS_PROGRESS, progress);
        }
    }

    private String getMinutes(int duration) { // 给我一个duration，转换成xxx分钟
        int minutes = duration / 60;
        if (minutes <= 9) {
            return "0" + minutes;
        }
        return "" + minutes;
    }

    // 119 ---> 60 59
    private String getSeconds(int duration) { // 给我一个duration，转换成xxx秒
        int seconds = duration % 60;
        if (seconds <= 9) {
            return "0" + seconds;
        }
        return "" + seconds;
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

    /**
     * 拖动条进度发生改变触发
     *
     * @param seekBar  组件
     * @param progress 当前拖拽的进度
     * @param fromUser 是否为用户拖拽导致的改变
     */
    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            sendMessage(HANDLE_STATUS_PROGRESS, progress);
        }
    }

    /**
     * 按下时触发
     */
    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        dragged = true;
    }

    /**
     * 抬起时触发
     */
    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        dragged = false;
        int audioTime = seekBar.getProgress(); // 获取抬起时的起始播放位置（时间戳）
        Log.d(TAG, "onStopTrackingTouch audioTime = " + audioTime);
        seekNative(audioTime);
    }

    private void sendMessage(int what) {
        sendMessage(what, null);
    }

    private void sendMessage(int what, Object obj) {
        Message message = Message.obtain();
        message.what = what;
        message.obj = obj;
        handler.sendMessage(message);
    }

    private class HandleMessage implements Handler.Callback {

        @Override
        public boolean handleMessage(@NonNull Message msg) {
            int what = msg.what;
            if (what == HANDLE_STATUS_PREPARED) {
                int visible = View.GONE;
                if (duration > 0) {
                    visible = View.VISIBLE;
                }
                seekBox.setVisibility(visible);
                if (duration > 0) {
                    String prepareTime = "00:00/" + getMinutes(duration) + ":" + getSeconds(duration);
                    timeView.setText(prepareTime);
                    seekBar.setMax(duration);
                }
            } else if (what == HANDLE_STATUS_PROGRESS) {
                Object progressObj = msg.obj;
                if (progressObj instanceof Integer) {
                    int audioTime = (int) progressObj;
                    String prepareTime = getMinutes(audioTime) + ":" + getSeconds(audioTime)
                            + "/" + getMinutes(duration) + ":" + getSeconds(duration);
                    timeView.setText(prepareTime);
                    seekBar.setProgress(audioTime, true);
                }
            }
            return false;
        }
    }

    private native void prepareNative(String dataSource);

    private native void startNative();

    private native void stopNative();

    private native void releaseNative();

    private native void setSurfaceNative(Surface surface);

    private native int fetchDurationNative();

    private native void seekNative(int audioTime);
}
