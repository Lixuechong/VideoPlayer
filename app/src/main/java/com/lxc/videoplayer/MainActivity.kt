package com.lxc.videoplayer

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.SurfaceView
import android.widget.Toast
import com.lxc.player.VideoPlayer
import java.io.File

class MainActivity : AppCompatActivity() {

    private val TAG = MainActivity::class.java.simpleName

    private var player: VideoPlayer? = null

    private var surfaceView: SurfaceView? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        surfaceView = findViewById(R.id.surfaceView)

        player = VideoPlayer()
        val file =
            File(Environment.getExternalStorageDirectory().absolutePath, "/big_buck_bunny.mp4");
        val path = file.path
        Log.d(TAG, path)
        player?.setDataSource(path)
        player?.bindSurfaceView(surfaceView)

        player?.setOnPreparedListener {
            runOnUiThread {
                Toast.makeText(this@MainActivity, "准备成功", Toast.LENGTH_SHORT).show()
            }
            player?.start()
        }

        player?.setOnErrorListener {
            runOnUiThread {
                Toast.makeText(this@MainActivity, "异常 $it", Toast.LENGTH_SHORT).show()
            }
        }
    }

    override fun onResume() {
        super.onResume()
        player?.prepare()
    }

    override fun onStop() {
        super.onStop()
        player?.stop()
    }

    override fun onDestroy() {
        super.onDestroy()
        player?.release()
    }


}