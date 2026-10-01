package com.fyx.beebot.ui

import android.Manifest
import android.content.pm.PackageManager
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.DeleteOutline
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.fyx.beebot.speech.*
import com.fyx.beebot.ui.theme.Hud

@Composable
fun SpeechRecognitionScreen() {
    val context = LocalContext.current
    val clipboard = LocalClipboardManager.current
    val state = SpeechRecognitionController.state
    val partialText = SpeechRecognitionController.partialText
    val level = SpeechRecognitionController.audioLevel
    val speakerState = SpeakerRecognitionController.state
    val messages = VoiceAssistantController.messages
    val active = VoiceAssistantController.active
    val speaking = VoiceAssistantController.speaking
    val dialogueScroll = rememberScrollState()
    var speakerName by rememberSaveable { mutableStateOf("") }
    var selectedProfile by remember { mutableStateOf<String?>(null) }
    var selectedScene by remember { mutableStateOf(VoiceScene.QUIET_ROOM) }
    var selectedDistance by remember { mutableStateOf(VoiceDistance.MEDIUM) }
    var keepRecording by rememberSaveable { mutableStateOf(false) }
    val permissionLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        if (granted) VoiceAssistantController.start(context) else SpeechRecognitionController.reportPermissionDenied()
    }

    LaunchedEffect(Unit) {
        SpeechRecognitionController.initialize(context)
        SpeakerRecognitionController.initialize(context)
        SpeechRecognitionController.audioSegmentSpeakerResolver = { segment -> SpeakerRecognitionController.identify(segment)?.speakerName ?: "未知说话人" }
        VoiceAssistantController.attach(context)
    }
    LaunchedEffect(messages.size, partialText) { dialogueScroll.animateScrollTo(dialogueScroll.maxValue) }
    DisposableEffect(Unit) {
        onDispose { VoiceAssistantController.detach(); SpeechRecognitionController.audioSegmentSpeakerResolver = null; SpeakerRecognitionController.stopEnrollment() }
    }

    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(9.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Column {
                Text("离线语音助手", color = Hud.TextPrimary, fontSize = 20.sp, fontWeight = FontWeight.SemiBold)
                Text("关键词意图 · 真实状态回答 · 不接入大模型", color = Hud.TextSecondary, fontSize = 11.sp)
            }
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                StatusChip(statusLabel(state), statusColor(state))
                StatusChip(if (speakerState == SpeakerRuntimeState.READY || speakerState == SpeakerRuntimeState.RECORDING) "声纹就绪" else "声纹加载中", if (speakerState == SpeakerRuntimeState.READY || speakerState == SpeakerRuntimeState.RECORDING) Hud.Success else Hud.Warning)
                StatusChip(if (speaking) "正在回答" else "关键词就绪", if (speaking) Hud.Warning else Hud.Success)
            }
        }
        Row(Modifier.weight(1f).fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight().clip(RoundedCornerShape(8.dp)).background(Hud.PanelDeep).border(1.dp, Hud.Border, RoundedCornerShape(8.dp)).padding(14.dp)) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("实时对话", color = Hud.TextSecondary, fontSize = 11.sp)
                    Text(when { speaking -> "播报中，麦克风已暂停"; active -> "正在监听"; else -> "对话已暂停" }, color = when { speaking -> Hud.Warning; active -> Hud.Success; else -> Hud.TextMuted }, fontSize = 10.sp)
                }
                Spacer(Modifier.height(8.dp))
                Column(Modifier.weight(1f).fillMaxWidth().verticalScroll(dialogueScroll), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    if (messages.isEmpty()) Text(when (state) { SpeechRecognitionState.LOADING -> "正在加载离线语音模型，请稍候…"; SpeechRecognitionState.ERROR -> SpeechRecognitionController.errorMessage ?: "语音助手不可用"; else -> "点击“开始对话”，可以问：现在电量多少？车辆在线吗？前方距离多少？" }, color = if (state == SpeechRecognitionState.ERROR) Hud.Danger else Hud.TextMuted, fontSize = 15.sp, lineHeight = 24.sp)
                    messages.forEach { ConversationBubble(it) }
                    if (partialText.isNotBlank() && active && !speaking) Text("正在识别：$partialText｜", color = Hud.Accent, fontSize = 14.sp, lineHeight = 22.sp)
                }
                Spacer(Modifier.height(8.dp)); MicrophoneLevel(level, active && !speaking)
            }
            Column(Modifier.width(244.dp).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(9.dp)) {
                InfoPanel("第一版可以询问") {
                    Text("掌机电量 · 充电状态 · 电池温度\n时间日期 · 网络状态 · APP版本\n车辆在线 · 车辆电量 · 通信延迟\n雷达距离 · 车辆模式", color = Hud.TextSecondary, fontSize = 10.sp, lineHeight = 16.sp)
                    Spacer(Modifier.height(6.dp)); Text("车辆数据超时会明确提示，不播报旧值。", color = Hud.Warning, fontSize = 9.sp, lineHeight = 14.sp)
                }
                InfoPanel("说话人管理") {
                    OutlinedTextField(value = speakerName, onValueChange = { speakerName = it }, singleLine = true, label = { Text("角色名") }, modifier = Modifier.fillMaxWidth())
                    Spacer(Modifier.height(5.dp))
                    SpeakerRecognitionController.profiles.forEach { profile ->
                        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                            Text("${profile.name} (${profile.samples.size})", color = Hud.TextSecondary, fontSize = 10.sp, modifier = Modifier.weight(1f))
                            TextButton(onClick = { selectedProfile = profile.id; speakerName = profile.name }, contentPadding = PaddingValues(3.dp)) { Text("续录", fontSize = 9.sp) }
                            TextButton(onClick = { SpeakerRecognitionController.delete(profile.id) }, contentPadding = PaddingValues(3.dp)) { Text("删除", fontSize = 9.sp, color = Hud.Danger) }
                        }
                    }
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        OutlinedButton(onClick = { val values = VoiceScene.values(); selectedScene = values[(selectedScene.ordinal + 1) % values.size] }, modifier = Modifier.weight(1f), contentPadding = PaddingValues(horizontal = 3.dp)) { Text(selectedScene.label, fontSize = 9.sp) }
                        OutlinedButton(onClick = { val values = VoiceDistance.values(); selectedDistance = values[(selectedDistance.ordinal + 1) % values.size] }, modifier = Modifier.weight(1f), contentPadding = PaddingValues(horizontal = 3.dp)) { Text(selectedDistance.label, fontSize = 9.sp) }
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) { Checkbox(checked = keepRecording, onCheckedChange = { keepRecording = it }); Text("保留训练WAV", color = Hud.TextSecondary, fontSize = 9.sp) }
                    Button(onClick = { if (speakerState == SpeakerRuntimeState.RECORDING) SpeakerRecognitionController.stopEnrollment() else { VoiceAssistantController.pause(); SpeakerRecognitionController.startEnrollment(context, selectedProfile, speakerName, selectedScene, selectedDistance, keepRecording) } }, enabled = speakerName.isNotBlank() && speakerState != SpeakerRuntimeState.LOADING, colors = ButtonDefaults.buttonColors(containerColor = Hud.AccentDim, contentColor = Color.White), modifier = Modifier.fillMaxWidth(), contentPadding = PaddingValues(horizontal = 6.dp)) { Text(if (speakerState == SpeakerRuntimeState.RECORDING) "停止并保存样本" else "录制一段样本", fontSize = 10.sp) }
                    if (selectedProfile != null) {
                        OutlinedButton(onClick = { SpeakerRecognitionController.rename(selectedProfile!!, speakerName); selectedProfile = null }, modifier = Modifier.fillMaxWidth(), contentPadding = PaddingValues(horizontal = 5.dp)) { Text("保存名称 / 结束续录", fontSize = 9.sp) }
                        SpeakerRecognitionController.lastResult?.embedding?.let { embedding -> OutlinedButton(onClick = { SpeakerRecognitionController.confirmLearning(selectedProfile!!, embedding, selectedScene, selectedDistance) }, modifier = Modifier.fillMaxWidth(), contentPadding = PaddingValues(horizontal = 5.dp)) { Text("标定最近语音给此角色", fontSize = 9.sp) } }
                    }
                    SpeakerRecognitionController.enrollmentHint?.let { Text(it, color = Hud.Warning, fontSize = 9.sp, lineHeight = 13.sp) }
                }
            }
        }
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text("全部意图匹配和状态查询均在本机完成。", color = Hud.TextMuted, fontSize = 10.sp, modifier = Modifier.weight(1f))
            OutlinedButton(onClick = { VoiceAssistantController.clear() }, enabled = messages.isNotEmpty(), colors = ButtonDefaults.outlinedButtonColors(contentColor = Hud.Danger)) { Icon(Icons.Filled.DeleteOutline, null, Modifier.size(16.dp)); Spacer(Modifier.width(5.dp)); Text("清空") }
            Spacer(Modifier.width(7.dp))
            OutlinedButton(onClick = { clipboard.setText(AnnotatedString(VoiceAssistantController.transcript())) }, enabled = messages.isNotEmpty(), colors = ButtonDefaults.outlinedButtonColors(contentColor = Hud.TextPrimary)) { Icon(Icons.Filled.ContentCopy, null, Modifier.size(16.dp)); Spacer(Modifier.width(5.dp)); Text("复制") }
            Spacer(Modifier.width(7.dp))
            Button(onClick = { if (active) VoiceAssistantController.pause() else if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) { SpeakerRecognitionController.stopEnrollment(); VoiceAssistantController.start(context) } else permissionLauncher.launch(Manifest.permission.RECORD_AUDIO) }, enabled = state != SpeechRecognitionState.LOADING && !speaking, colors = ButtonDefaults.buttonColors(containerColor = Hud.AccentDim, contentColor = Color.White)) { Icon(if (active) Icons.Filled.Pause else Icons.Filled.Mic, null, Modifier.size(18.dp)); Spacer(Modifier.width(7.dp)); Text(if (active) "暂停对话" else "开始对话") }
        }
    }
}

@Composable private fun ConversationBubble(message: VoiceConversationMessage) { val assistant = message.role == ConversationRole.ASSISTANT; Row(Modifier.fillMaxWidth(), horizontalArrangement = if (assistant) Arrangement.End else Arrangement.Start) { Column(Modifier.fillMaxWidth(0.84f).clip(RoundedCornerShape(8.dp)).background(if (assistant) Hud.AccentDim.copy(alpha = 0.28f) else Hud.Panel).border(1.dp, if (assistant) Hud.Success.copy(alpha = 0.55f) else Hud.Border, RoundedCornerShape(8.dp)).padding(horizontal = 11.dp, vertical = 8.dp)) { Text(message.speakerName, color = if (assistant) Hud.Success else Hud.TextSecondary, fontSize = 9.sp); Spacer(Modifier.height(2.dp)); Text(message.text, color = Hud.TextPrimary, fontSize = 14.sp, lineHeight = 21.sp) } } }
@Composable private fun MicrophoneLevel(level: Float, listening: Boolean) { Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) { Text(if (listening) "麦克风" else "待机", color = if (listening) Hud.Success else Hud.TextMuted, fontSize = 9.sp); Spacer(Modifier.width(8.dp)); Row(Modifier.weight(1f).height(7.dp), horizontalArrangement = Arrangement.spacedBy(3.dp)) { repeat(12) { index -> Box(Modifier.weight(1f).fillMaxHeight().clip(RoundedCornerShape(2.dp)).background(if (listening && level >= (index + 1) / 14f) Hud.Accent else Hud.Border)) } } } }
@Composable private fun StatusChip(label: String, color: Color) { Row(Modifier.height(30.dp).clip(RoundedCornerShape(7.dp)).background(Hud.Panel).border(1.dp, Hud.Border, RoundedCornerShape(7.dp)).padding(horizontal = 10.dp), verticalAlignment = Alignment.CenterVertically) { Box(Modifier.size(6.dp).clip(RoundedCornerShape(50)).background(color)); Spacer(Modifier.width(6.dp)); Text(label, color = color, fontSize = 10.sp, maxLines = 1) } }
@Composable private fun InfoPanel(title: String, content: @Composable ColumnScope.() -> Unit) { Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(8.dp)).background(Hud.Panel).border(1.dp, Hud.Border, RoundedCornerShape(8.dp)).padding(11.dp)) { Text(title, color = Hud.TextPrimary, fontSize = 11.sp, fontWeight = FontWeight.Medium); Spacer(Modifier.height(8.dp)); content() } }
private fun statusLabel(state: SpeechRecognitionState): String = when (state) { SpeechRecognitionState.IDLE -> "等待加载"; SpeechRecognitionState.LOADING -> "模型加载中"; SpeechRecognitionState.READY -> "模型就绪"; SpeechRecognitionState.LISTENING -> "识别中"; SpeechRecognitionState.ERROR -> "需要处理" }
private fun statusColor(state: SpeechRecognitionState): Color = when (state) { SpeechRecognitionState.READY, SpeechRecognitionState.LISTENING -> Hud.Success; SpeechRecognitionState.ERROR -> Hud.Danger; SpeechRecognitionState.LOADING -> Hud.Warning; SpeechRecognitionState.IDLE -> Hud.TextMuted }
