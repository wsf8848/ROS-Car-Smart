package com.fyx.beebot.transport

import com.fyx.beebot.input.GamepadState

/**
 * 机器人通讯层接口（预留拓展点）。
 *
 * 后续接入机器狗 / 机器车时，只需实现本接口（蓝牙 / WiFi-UDP / 串口 等），
 * 把 [GamepadState] 编码成对应的控制协议帧下发即可，界面与采集层无需改动。
 *
 * 例如：
 *  - BleRobotTransport   —— 低功耗蓝牙
 *  - UdpRobotTransport   —— WiFi 图传/控制
 *  - SerialRobotTransport —— USB / TTL 串口
 */
interface RobotTransport {

    val name: String

    val isConnected: Boolean

    /** 建立连接。 */
    fun connect()

    /** 断开连接。 */
    fun disconnect()

    /**
     * 下发一帧控制指令。实现方负责把手柄状态映射为具体机器人的运动协议
     * （如底盘线速度/角速度、云台角度、灯光、模式切换等）。
     */
    fun sendControl(state: GamepadState)
}
