package com.lxc.player;

public class NativeLib {

    // Used to load the 'player' library on application startup.
    static {
        System.loadLibrary("player");
    }

    /**
     * A native method that is implemented by the 'player' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
}