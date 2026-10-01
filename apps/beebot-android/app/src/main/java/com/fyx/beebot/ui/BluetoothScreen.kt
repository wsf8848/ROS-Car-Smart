package com.fyx.beebot.ui

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fyx.beebot.bt.BAUD_RATES
import com.fyx.beebot.bt.BtCodec
import com.fyx.beebot.bt.BtDevice
import com.fyx.beebot.bt.BtDir
import com.fyx.beebot.bt.BtEncoding
import com.fyx.beebot.bt.BluetoothRepository
import com.fyx.beebot.ui.theme.Hud
import kotlinx.coroutines.delay
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** 蓝牙调试页（经典蓝牙 SPP 串口助手）。 */
@Composable
fun BluetoothScreen() {
    val context = LocalContext.current
    val prefs = remember { context.getSharedPreferences(Auth.PREFS, Context.MODE_PRIVATE) }

    var baud by remember { mutableStateOf(prefs.getInt("bt_baud", 9600)) }
    var baudMenu by remember { mutableStateOf(false) }
    var rxEnc by remember { mutableStateOf(enc(prefs.getString("bt_rx_enc", "HEX"))) }
    var txEnc by remember { mutableStateOf(enc(prefs.getString("bt_tx_enc", "HEX"))) }
    var sendText by remember { mutableStateOf("") }
    var timedOn by remember { mutableStateOf(false) }
    var intervalMs by remember { mutableStateOf(500) }
    var sendErr by remember { mutableStateOf("") }

    LaunchedEffect(Unit) { BluetoothRepository.refreshPaired(context) }

    val doSend: () -> Unit = {
        val bytes = BtCodec.parse(sendText, txEnc)
        if (bytes == null) sendErr = "HEX 格式错误（应为偶数位十六进制）"
        else { sendErr = ""; BluetoothRepository.send(bytes) }
    }

    // 定时发送
    LaunchedEffect(timedOn, intervalMs, BluetoothRepository.connected) {
        while (timedOn && BluetoothRepository.connected) {
            BtCodec.parse(sendText, txEnc)?.let { BluetoothRepository.send(it) }
            delay(intervalMs.toLong())
        }
    }

    Column(Modifier.fillMaxSize()) {
        // 连接条
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(Hud.Panel, RoundedCornerShape(10.dp))
                .border(1.dp, Hud.Border, RoundedCornerShape(10.dp))
                .padding(horizontal = 12.dp, vertical = 7.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            val fg = if (BluetoothRepository.connected) Hud.Success else if (BluetoothRepository.connecting) Hud.Warning else Hud.TextMuted
            Row(
                modifier = Modifier.clip(RoundedCornerShape(20.dp)).background(Hud.PanelDeep).padding(horizontal = 10.dp, vertical = 5.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Box(Modifier.size(7.dp).clip(CircleShape).background(fg))
                Spacer(Modifier.width(6.dp))
                Text(BluetoothRepository.statusMsg, color = fg, fontSize = 11.sp, maxLines = 1)
            }
            Spacer(Modifier.weight(1f))
            Text("接收频率", color = Hud.TextMuted, fontSize = 11.sp)
            Spacer(Modifier.width(6.dp))
            Text(
                if (BluetoothRepository.rxIntervalMs >= 0) "${BluetoothRepository.rxIntervalMs}ms" else "--ms",
                color = Hud.Warning, fontSize = 13.sp, fontFamily = FontFamily.Monospace
            )
            Spacer(Modifier.width(12.dp))
            PillButton("扫描", Hud.Border, Hud.TextSecondary) { com.fyx.beebot.util.Haptics.tick(); BluetoothRepository.refreshPaired(context) }
            Spacer(Modifier.width(6.dp))
            PillButton("断开", Hud.Danger, Hud.Danger) { com.fyx.beebot.util.Haptics.heavy(); BluetoothRepository.disconnect() }
        }

        Spacer(Modifier.height(8.dp))

        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {

            // 左列
            Column(Modifier.width(158.dp).fillMaxHeight()) {
                BtPanel(Modifier.weight(1f)) {
                    Text("配对设备", color = Hud.TextSecondary, fontSize = 12.sp)
                    Spacer(Modifier.height(8.dp))
                    LazyColumn(Modifier.fillMaxWidth().weight(1f)) {
                        items(BluetoothRepository.paired) { d -> DeviceItem(d) { BluetoothRepository.connect(context, d) } }
                    }
                }
                Spacer(Modifier.height(8.dp))
                BtPanel(Modifier.wrapContentHeight()) {
                    Text("波特率", color = Hud.TextSecondary, fontSize = 12.sp)
                    Spacer(Modifier.height(6.dp))
                    Box {
                        Row(
                            modifier = Modifier.fillMaxWidth()
                                .background(Hud.PanelDeep, RoundedCornerShape(7.dp))
                                .border(1.dp, Hud.Border, RoundedCornerShape(7.dp))
                                .clickable { baudMenu = true }
                                .padding(horizontal = 9.dp, vertical = 7.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("$baud", color = Hud.TextPrimary, fontSize = 13.sp, fontFamily = FontFamily.Monospace)
                            Spacer(Modifier.weight(1f))
                            Text("▾", color = Hud.TextMuted, fontSize = 12.sp)
                        }
                        DropdownMenu(expanded = baudMenu, onDismissRequest = { baudMenu = false }) {
                            BAUD_RATES.forEach { b ->
                                DropdownMenuItem(text = { Text("$b") }, onClick = {
                                    baud = b; baudMenu = false; prefs.edit().putInt("bt_baud", b).apply()
                                })
                            }
                        }
                    }
                    Text("SPP 下仅 App 内记录", color = Hud.TextMuted, fontSize = 10.sp, modifier = Modifier.padding(top = 5.dp))
                }
                Spacer(Modifier.height(8.dp))
                BtPanel(Modifier.wrapContentHeight()) {
                    Row(Modifier.fillMaxWidth()) {
                        Text("RX", color = Hud.Success, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                        Spacer(Modifier.weight(1f))
                        Text(fmtBytes(BluetoothRepository.rxBytes), color = Hud.Success, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                    }
                    Row(Modifier.fillMaxWidth().padding(top = 3.dp)) {
                        Text("TX", color = Hud.Accent, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                        Spacer(Modifier.weight(1f))
                        Text(fmtBytes(BluetoothRepository.txBytes), color = Hud.Accent, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }

            // 右列：接收 + 发送
            Column(Modifier.weight(1f).fillMaxHeight()) {
                // 接收区
                BtPanel(Modifier.weight(1f)) {
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Text("接收区", color = Hud.TextSecondary, fontSize = 12.sp)
                        Spacer(Modifier.weight(1f))
                        EncToggle(rxEnc) { rxEnc = it; prefs.edit().putString("bt_rx_enc", it.name).apply() }
                        Spacer(Modifier.width(8.dp))
                        Text("清空", color = Hud.TextMuted, fontSize = 11.sp, modifier = Modifier.clickable { com.fyx.beebot.util.Haptics.tick(); BluetoothRepository.clearLog() })
                    }
                    Spacer(Modifier.height(6.dp))
                    val fmt = remember { SimpleDateFormat("HH:mm:ss.SSS", Locale.US) }
                    LazyColumn(
                        Modifier.fillMaxSize().background(Hud.PanelDeep, RoundedCornerShape(7.dp)).padding(8.dp)
                    ) {
                        items(BluetoothRepository.frames) { f ->
                            Row(Modifier.fillMaxWidth().padding(vertical = 1.dp)) {
                                Text(fmt.format(Date(f.timestamp)), color = Hud.TextMuted, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                                Spacer(Modifier.width(8.dp))
                                when (f.dir) {
                                    BtDir.RX -> {
                                        Text("RX", color = Hud.Success, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                                        Spacer(Modifier.width(8.dp))
                                        Text(BtCodec.format(f.data, rxEnc), color = Hud.TextSecondary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                                    }
                                    BtDir.TX -> {
                                        Text("TX", color = Hud.Accent, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                                        Spacer(Modifier.width(8.dp))
                                        Text(BtCodec.format(f.data, txEnc), color = Hud.TextMuted, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                                    }
                                    BtDir.SYS -> {
                                        Text("SYS", color = Hud.Warning, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                                        Spacer(Modifier.width(8.dp))
                                        Text(f.sysMsg ?: "", color = Hud.Warning, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                                    }
                                }
                            }
                        }
                    }
                }
                Spacer(Modifier.height(8.dp))
                // 发送区
                BtPanel(Modifier.wrapContentHeight()) {
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Text("发送区", color = Hud.TextSecondary, fontSize = 12.sp)
                        Spacer(Modifier.weight(1f))
                        EncToggle(txEnc) { txEnc = it; prefs.edit().putString("bt_tx_enc", it.name).apply() }
                    }
                    Spacer(Modifier.height(6.dp))
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Box(
                            Modifier.weight(1f).height(38.dp)
                                .background(Hud.PanelDeep, RoundedCornerShape(7.dp))
                                .border(1.dp, Hud.Border, RoundedCornerShape(7.dp))
                                .padding(horizontal = 9.dp),
                            contentAlignment = Alignment.CenterStart
                        ) {
                            BasicTextField(
                                value = sendText, onValueChange = { sendText = it; sendErr = "" },
                                singleLine = true,
                                textStyle = TextStyle(color = Hud.TextPrimary, fontSize = 13.sp, fontFamily = FontFamily.Monospace),
                                cursorBrush = SolidColor(Hud.Accent),
                                decorationBox = { inner ->
                                    if (sendText.isEmpty()) Text(if (txEnc == BtEncoding.HEX) "AA 55 01 00 FF" else "输入要发送的文本", color = Hud.TextMuted, fontSize = 13.sp, fontFamily = FontFamily.Monospace)
                                    inner()
                                }
                            )
                        }
                        Spacer(Modifier.width(8.dp))
                        Box(
                            Modifier.width(74.dp).height(38.dp)
                                .clip(RoundedCornerShape(7.dp)).background(Hud.Accent)
                                .clickable { com.fyx.beebot.util.Haptics.click(); doSend() },
                            contentAlignment = Alignment.Center
                        ) { Text("发送", color = Color(0xFF042027), fontSize = 14.sp, fontWeight = FontWeight.Medium) }
                    }
                    if (sendErr.isNotEmpty()) Text(sendErr, color = Hud.Danger, fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
                    Row(Modifier.fillMaxWidth().padding(top = 9.dp), verticalAlignment = Alignment.CenterVertically) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.clickable { timedOn = !timedOn }
                        ) {
                            Box(
                                Modifier.size(16.dp).clip(RoundedCornerShape(4.dp))
                                    .background(if (timedOn) Hud.Accent else Hud.PanelDeep)
                                    .border(1.dp, if (timedOn) Hud.Accent else Hud.Border, RoundedCornerShape(4.dp)),
                                contentAlignment = Alignment.Center
                            ) { if (timedOn) Text("✓", color = Color(0xFF042027), fontSize = 11.sp) }
                            Spacer(Modifier.width(6.dp))
                            Text("定时发送", color = Hud.TextPrimary, fontSize = 12.sp)
                        }
                        Spacer(Modifier.width(12.dp))
                        IntervalChip("10ms", intervalMs == 10) { intervalMs = 10 }
                        Spacer(Modifier.width(6.dp))
                        IntervalChip("500ms", intervalMs == 500) { intervalMs = 500 }
                        Spacer(Modifier.width(6.dp))
                        IntervalChip("1S", intervalMs == 1000) { intervalMs = 1000 }
                        Spacer(Modifier.weight(1f))
                        if (timedOn && BluetoothRepository.connected)
                            Text("周期${intervalMs}ms发送中…", color = Hud.TextMuted, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }
        }
    }
}

private fun enc(s: String?): BtEncoding = if (s == "UTF8") BtEncoding.UTF8 else BtEncoding.HEX

private fun fmtBytes(n: Long): String = if (n < 1024) "$n B" else "%.2f KB".format(n / 1024f)

@Composable
private fun BtPanel(modifier: Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = modifier.fillMaxWidth()
            .background(Hud.Panel, RoundedCornerShape(10.dp))
            .border(1.dp, Hud.Border, RoundedCornerShape(10.dp))
            .padding(10.dp),
        content = content
    )
}

@Composable
private fun DeviceItem(d: BtDevice, onClick: () -> Unit) {
    Column(
        Modifier.fillMaxWidth().padding(bottom = 5.dp)
            .clip(RoundedCornerShape(7.dp)).background(Hud.PanelDeep)
            .border(1.dp, Hud.BorderSoft, RoundedCornerShape(7.dp))
            .clickable { com.fyx.beebot.util.Haptics.click(); onClick() }.padding(horizontal = 9.dp, vertical = 7.dp)
    ) {
        Text(d.name, color = Hud.TextPrimary, fontSize = 12.sp, maxLines = 1)
        Text(d.address, color = Hud.TextMuted, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
    }
}

@Composable
private fun EncToggle(current: BtEncoding, onChange: (BtEncoding) -> Unit) {
    Row(Modifier.clip(RoundedCornerShape(7.dp)).border(1.dp, Hud.Border, RoundedCornerShape(7.dp))) {
        listOf(BtEncoding.HEX, BtEncoding.UTF8).forEach { e ->
            val active = e == current
            Text(
                if (e == BtEncoding.HEX) "HEX" else "UTF-8",
                color = if (active) Color(0xFF042027) else Hud.TextSecondary,
                fontSize = 11.sp,
                modifier = Modifier.background(if (active) Hud.Accent else Color.Transparent).clickable { com.fyx.beebot.util.Haptics.tick(); onChange(e) }.padding(horizontal = 10.dp, vertical = 4.dp)
            )
        }
    }
}

@Composable
private fun IntervalChip(label: String, active: Boolean, onClick: () -> Unit) {
    Text(
        label, color = if (active) Hud.Accent else Hud.TextMuted, fontSize = 11.sp,
        modifier = Modifier.clip(RoundedCornerShape(20.dp))
            .background(if (active) Color(0xFF0E1E2A) else Color.Transparent)
            .border(1.dp, if (active) Hud.Accent else Hud.BorderSoft, RoundedCornerShape(20.dp))
            .clickable { com.fyx.beebot.util.Haptics.tick(); onClick() }.padding(horizontal = 12.dp, vertical = 4.dp)
    )
}

@Composable
private fun PillButton(label: String, borderColor: Color, textColor: Color, onClick: () -> Unit) {
    Text(
        label, color = textColor, fontSize = 12.sp,
        modifier = Modifier.clip(RoundedCornerShape(7.dp)).border(1.dp, borderColor, RoundedCornerShape(7.dp))
            .clickable { onClick() }.padding(horizontal = 12.dp, vertical = 5.dp)
    )
}
