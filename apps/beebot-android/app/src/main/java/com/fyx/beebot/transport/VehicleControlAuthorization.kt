package com.fyx.beebot.transport

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/** 主页与蓝牙智能小车页共用的车辆控制授权状态。 */
object VehicleControlAuthorization {
    var locked by mutableStateOf(true)
        private set

    fun lock() { locked = true }
    fun unlock() { locked = false }
}
