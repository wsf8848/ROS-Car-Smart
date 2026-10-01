package com.fyx.beebot.ui

import android.app.Application
import android.os.Handler
import android.os.Looper
import android.os.Message
import android.view.SurfaceHolder
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.fyx.beebot.BuildConfig
import com.videogo.errorlayer.ErrorInfo
import com.videogo.openapi.EZConstants.EZRealPlayConstants
import com.videogo.openapi.EZOpenSDK
import com.videogo.openapi.EZPlayer

internal enum class EzvizPreviewStatus(val label: String) {
    CONFIGURATION_REQUIRED("萤石配置未完成"),
    CONNECTING("萤石云画面连接中"),
    PLAYING("萤石云画面已接入"),
    ERROR("萤石云画面断开")
}

/**
 * 将萤石 SDK 生命周期收拢在一个播放器中，避免视频错误影响车辆 MQTT/蓝牙控制。
 */
internal object EzvizPreviewPlayer {
    var status by mutableStateOf(EzvizPreviewStatus.CONFIGURATION_REQUIRED)
        private set
    var errorCode by mutableStateOf<Int?>(null)
        private set

    private var sdkInitialized = false
    private var player: EZPlayer? = null
    private var surfaceHolder: SurfaceHolder? = null
    private var playing = false

    private val messageHandler = Handler(Looper.getMainLooper()) { message: Message ->
        when (message.what) {
            EZRealPlayConstants.MSG_REALPLAY_PLAY_SUCCESS -> {
                playing = true
                errorCode = null
                status = EzvizPreviewStatus.PLAYING
            }
            EZRealPlayConstants.MSG_REALPLAY_PLAY_FAIL -> {
                playing = false
                errorCode = (message.obj as? ErrorInfo)?.errorCode
                status = EzvizPreviewStatus.ERROR
            }
        }
        true
    }

    fun attach(application: Application, holder: SurfaceHolder) {
        surfaceHolder = holder
        if (!hasConfiguration()) {
            errorCode = null
            status = EzvizPreviewStatus.CONFIGURATION_REQUIRED
            return
        }
        if (!initializeSdk(application)) {
            status = EzvizPreviewStatus.ERROR
            return
        }
        startPlayback()
    }

    fun pause() {
        if (playing) player?.stopRealPlay()
        playing = false
    }

    fun resume() {
        if (surfaceHolder != null && hasConfiguration() && sdkInitialized) startPlayback()
    }

    fun onSurfaceDestroyed(holder: SurfaceHolder) {
        if (surfaceHolder === holder) {
            pause()
            player?.setSurfaceHold(null)
            surfaceHolder = null
        }
    }

    fun release() {
        pause()
        player?.setSurfaceHold(null)
        player?.release()
        player = null
        surfaceHolder = null
        playing = false
    }

    private fun startPlayback() {
        val holder = surfaceHolder ?: return
        val previewPlayer = player ?: EZOpenSDK.getInstance().createPlayer(
            BuildConfig.EZVIZ_DEVICE_SERIAL,
            BuildConfig.EZVIZ_CHANNEL_NO.toIntOrNull() ?: 1
        ).also {
            it.setHandler(messageHandler)
            if (BuildConfig.EZVIZ_VERIFY_CODE.isNotBlank()) it.setPlayVerifyCode(BuildConfig.EZVIZ_VERIFY_CODE)
            it.closeSound()
            player = it
        }
        previewPlayer.setSurfaceHold(holder)
        if (!playing) {
            errorCode = null
            status = EzvizPreviewStatus.CONNECTING
            if (!previewPlayer.startRealPlay()) status = EzvizPreviewStatus.ERROR
        }
    }

    private fun initializeSdk(application: Application): Boolean {
        if (sdkInitialized) return true
        EZOpenSDK.showSDKLog(BuildConfig.DEBUG)
        EZOpenSDK.setDebugStreamEnable(false)
        sdkInitialized = EZOpenSDK.initLib(application, BuildConfig.EZVIZ_APP_KEY)
        if (sdkInitialized) EZOpenSDK.getInstance().setAccessToken(BuildConfig.EZVIZ_ACCESS_TOKEN)
        return sdkInitialized
    }

    private fun hasConfiguration(): Boolean =
        BuildConfig.EZVIZ_APP_KEY.isNotBlank() &&
            BuildConfig.EZVIZ_ACCESS_TOKEN.isNotBlank() &&
            BuildConfig.EZVIZ_DEVICE_SERIAL.isNotBlank()
}
