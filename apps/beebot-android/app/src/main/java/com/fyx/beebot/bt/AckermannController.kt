package com.fyx.beebot.bt

import android.os.SystemClock
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.fyx.beebot.input.GamepadButton
import com.fyx.beebot.input.GamepadState
import com.fyx.beebot.util.Haptics

/**
 * 阿克曼小车遥控（基于 WHEELTEC 底盘 USART2 蓝牙「APP 单字节协议」）。
 *
 * 控制（App→车）—— 按车端真正生效的 balance.c `Get_RC()`(Akm 分支) 映射：
 *  单字节 A~H(0x41~48) 设 Flag_Direction=1~8：Move_Z +PI/2=左 / -PI/2=右
 *    'A'(1)前直  'B'(2)前+右  'C'(3)停+右(原地右打舵)  'D'(4)后+右
 *    'E'(5)后直  'F'(6)后+左  'G'(7)停+左(原地左打舵)  'H'(8)前+左
 *  ⚠️ 停车+回中必须让 Flag_Direction=0(→default: Move_X=0,Move_Z=0)：发 'I'(0x49)
 *     （0x49 既落到 Flag_Direction=0，又置 Turn_Flag=0 强制方向接口，避免"停车"被当右转）。
 *  - 进入遥控：连发两个 'A'(0x41) 置 APP_ON_Flag=1，再补 'I' 停车回中。
 *  - 调速：'X'(0x58)=+100mm/s，'Y'(0x59)=-100mm/s（RC_Velocity 档位）。
 *  - 需固定周期(100ms)持续发，维持底盘失控保护(command_lost)不复位。
 *
 * 遥测（车→App，ASCII 帧 {..}$，printf 重定向到 USART2）：
 *  - {A<左轮*100>:<右轮*100>:<电量%>:<陀螺Z>}$   主页数据（本页解析显示）
 *  - {B..}$ 波形、{C<RC_Velocity>:<KP>:<KI>}$ 参数
 *
 * ⚠️ 极性已按固件"Move_Z +PI/2=左/-PI/2=右"并经真车"松手往右"症状印证；若某车相反，对调左右两组即可。
 * ⚠️ 底盘上电前 10s 忽略指令（CONTROL_DELAY）。
 */
object AckermannController {

    const val DEADZONE = 0.25f

    // 遥测
    var leftSpeed by mutableStateOf(0f); private set    // m/s
    var rightSpeed by mutableStateOf(0f); private set   // m/s
    var battery by mutableStateOf(0); private set       // %
    var gyroZ by mutableStateOf(0); private set
    var telemetryMs by mutableStateOf(-1); private set

    // 控制状态
    var remoting by mutableStateOf(false); private set
    var lastCmdLabel by mutableStateOf("停 车"); private set
    var lastCmdByte by mutableStateOf(0x49); private set
    var rcVelocity by mutableStateOf(300); private set  // 本地速度档镜像 mm/s

    private var lastTelemetryTime = 0L
    private var lastSpeedStepTime = 0L
    private val rxBuf = StringBuilder()

    fun startRemote() {
        remoting = true
        BluetoothRepository.sendRaw(byteArrayOf(0x41, 0x41, 0x49)) // 双A进APP模式 + I 停车回中
        lastCmdLabel = "停 车"; lastCmdByte = 0x49
    }

    /** 四驱智能小车安全启动：先停车再授权模式，不发送可能让车轮动作的旧阿克曼握手。 */
    fun startSmartCarRemote(modeCommand: Int) {
        remoting = true
        BluetoothRepository.sendRaw(byteArrayOf(0x49, modeCommand.toByte()))
        lastCmdLabel = "停 车"; lastCmdByte = 0x49
    }

    fun stopRemote(forceStop: Boolean = false) {
        if (remoting || forceStop) BluetoothRepository.sendRaw(byteArrayOf(0x49))
        remoting = false
        lastCmdLabel = "停 车"; lastCmdByte = 0x49
    }

    fun emergencyStop() {
        BluetoothRepository.sendRaw(byteArrayOf(0x49, 0x49, 0x49))
        remoting = false
        lastCmdLabel = "急 停"; lastCmdByte = 0x49
    }

    fun throttleOf(s: GamepadState): Float = -s.leftY      // 摇杆上为前（AXIS_Y 上为负）
    fun steeringOf(s: GamepadState): Float = s.rightX      // 右为正

    /** 每个发送周期调用（建议 100ms）。 */
    fun tick(s: GamepadState) {
        if (!remoting || !BluetoothRepository.connected) return

        // 调速：R2/R1 加速，L2/L1 减速（按住每 250ms 一档）
        val now = SystemClock.elapsedRealtime()
        if (now - lastSpeedStepTime > 250) {
            if (s.rightTrigger > 0.5f || s.isPressed(GamepadButton.R1)) {
                BluetoothRepository.sendRaw(byteArrayOf(0x58)); rcVelocity = (rcVelocity + 100).coerceAtMost(1500); lastSpeedStepTime = now; Haptics.tick()
            } else if (s.leftTrigger > 0.5f || s.isPressed(GamepadButton.L1)) {
                BluetoothRepository.sendRaw(byteArrayOf(0x59)); rcVelocity = (rcVelocity - 100).coerceAtLeast(0); lastSpeedStepTime = now; Haptics.tick()
            }
        }

        sendDriveCommand(s)
    }

    /** 智能小车页专用 tick：L2 发 0x6A 减速，R2 发 0x6B 加速。 */
    fun smartCarTick(s: GamepadState) {
        if (!remoting || !BluetoothRepository.connected) return

        val now = SystemClock.elapsedRealtime()
        if (now - lastSpeedStepTime > 250) {
            if (s.rightTrigger > 0.5f || s.isPressed(GamepadButton.R2)) {
                BluetoothRepository.sendRaw(byteArrayOf(0x6B))
                rcVelocity = (rcVelocity + 100).coerceAtMost(1500)
                lastSpeedStepTime = now
                Haptics.tick()
            } else if (s.leftTrigger > 0.5f || s.isPressed(GamepadButton.L2)) {
                BluetoothRepository.sendRaw(byteArrayOf(0x6A))
                rcVelocity = (rcVelocity - 100).coerceAtLeast(0)
                lastSpeedStepTime = now
                Haptics.tick()
            }
        }

        sendDriveCommand(s)
    }

    private fun sendDriveCommand(s: GamepadState) {
        val (ch, label) = mapCommand(s)
        lastCmdByte = ch.code; lastCmdLabel = label
        BluetoothRepository.sendRaw(byteArrayOf(ch.code.toByte()))
    }

    fun mapCommand(s: GamepadState): Pair<Char, String> = mapTS(throttleOf(s), steeringOf(s))

    /** 由油门(前+/后-)与转向(右+/左-)映射到 WHEELTEC APP 单字节指令。 */
    fun mapTS(thr: Float, steer: Float): Pair<Char, String> {
        val fwd = thr > DEADZONE; val back = thr < -DEADZONE
        val left = steer < -DEADZONE; val right = steer > DEADZONE
        // Move_Z +PI/2=左(H/G/F) / -PI/2=右(B/C/D)；停车+回中=Flag_Direction=0→'I'
        return when {
            fwd && left -> 'H' to "前进 + 左转"
            fwd && right -> 'B' to "前进 + 右转"
            fwd -> 'A' to "前进"
            back && left -> 'F' to "后退 + 左转"
            back && right -> 'D' to "后退 + 右转"
            back -> 'E' to "后退"
            left -> 'G' to "左转（原地）"
            right -> 'C' to "右转（原地）"
            else -> 'I' to "停 车"
        }
    }

    // ===== 网页(ROS网页Tab)虚拟摇杆 → 蓝牙直连驱动(绕开树莓派，避免其重启) =====
    @Volatile var webThrottle = 0f
    @Volatile var webSteer = 0f
    var webActive by mutableStateOf(false); private set

    /** 网页摇杆下发目标值（油门前+/后-，转向右+/左-，范围约 -1..1）。首帧自动进遥控模式。 */
    fun webDrive(thr: Float, steer: Float) {
        if (!BluetoothRepository.connected) return
        if (!webActive) { startRemote(); webActive = true }
        webThrottle = thr; webSteer = steer
    }

    /** 网页摇杆松手/离开页面：停车回中并退出。 */
    fun webStop() {
        webThrottle = 0f; webSteer = 0f
        if (webActive) { webActive = false; stopRemote() }
    }

    /** 网页驱动的固定周期发送（RosWebScreen 每 100ms 调用），维持底盘失控保护不复位。 */
    fun webTick() {
        if (!webActive || !BluetoothRepository.connected) return
        val (ch, label) = mapTS(webThrottle, webSteer)
        lastCmdByte = ch.code; lastCmdLabel = label
        BluetoothRepository.sendRaw(byteArrayOf(ch.code.toByte()))
    }

    /**
     * ROS网页页统一驱动 tick（每 100ms 调用）：**实体摇杆优先**，无实体输入时用网页虚拟摇杆值。
     * 实体扳机 R2/R1 加速、L2/L1 减速。全部经蓝牙直连 STM32（绕开树莓派）。
     */
    fun rosDriveTick(s: GamepadState) {
        if (!BluetoothRepository.connected) { webActive = false; return }
        val pt = throttleOf(s); val ps = steeringOf(s)
        val physActive = kotlin.math.abs(pt) > DEADZONE || kotlin.math.abs(ps) > DEADZONE
        val thr = if (physActive) pt else webThrottle
        val steer = if (physActive) ps else webSteer
        val anyActive = physActive ||
            kotlin.math.abs(webThrottle) > DEADZONE || kotlin.math.abs(webSteer) > DEADZONE
        if (anyActive && !webActive) { startRemote(); webActive = true }
        if (!webActive) return
        val (ch, label) = mapTS(thr, steer)
        lastCmdByte = ch.code; lastCmdLabel = label
        BluetoothRepository.sendRaw(byteArrayOf(ch.code.toByte()))
        // 实体扳机调速
        val now = SystemClock.elapsedRealtime()
        if (now - lastSpeedStepTime > 250) {
            if (s.rightTrigger > 0.5f || s.isPressed(GamepadButton.R1)) {
                BluetoothRepository.sendRaw(byteArrayOf(0x58)); rcVelocity = (rcVelocity + 100).coerceAtMost(1500); lastSpeedStepTime = now; Haptics.tick()
            } else if (s.leftTrigger > 0.5f || s.isPressed(GamepadButton.L1)) {
                BluetoothRepository.sendRaw(byteArrayOf(0x59)); rcVelocity = (rcVelocity - 100).coerceAtLeast(0); lastSpeedStepTime = now; Haptics.tick()
            }
        }
    }

    /** 喂入蓝牙原始接收字节，解析 {..}$ 遥测帧。 */
    fun feedRx(bytes: ByteArray) {
        for (b in bytes) rxBuf.append((b.toInt() and 0xFF).toChar())
        while (true) {
            val start = rxBuf.indexOf("{")
            if (start < 0) { rxBuf.setLength(0); break }
            val end = rxBuf.indexOf("}$", start)
            if (end < 0) { if (start > 0) rxBuf.delete(0, start); break }
            val frame = rxBuf.substring(start + 1, end)
            rxBuf.delete(0, end + 2)
            parseFrame(frame)
        }
        if (rxBuf.length > 256) rxBuf.delete(0, rxBuf.length - 256)
    }

    private fun parseFrame(f: String) {
        if (f.isEmpty()) return
        when (f[0]) {
            'A' -> {
                val p = f.substring(1).split(":")
                if (p.size >= 4) {
                    leftSpeed = (p[0].toIntOrNull() ?: 0) / 100f
                    rightSpeed = (p[1].toIntOrNull() ?: 0) / 100f
                    battery = (p[2].toIntOrNull() ?: 0).coerceIn(0, 100)
                    gyroZ = p[3].toIntOrNull() ?: 0
                    val now = SystemClock.elapsedRealtime()
                    if (lastTelemetryTime > 0) telemetryMs = (now - lastTelemetryTime).toInt()
                    lastTelemetryTime = now
                }
            }
            'C' -> f.substring(1).split(":").firstOrNull()?.toIntOrNull()?.let { rcVelocity = it }
        }
    }
}
