# AnimatedVisibility 语音页离开后录音重启

## 现象

- 语音助手页面视觉上已返回主控台，但 `dumpsys audio` 仍显示本包的 16kHz `VOICE_RECOGNITION` 录音活动。
- 等待超过退出动画时间后仍出现新的 `rec update` / `rec stop`，不是一次性的录音线程收尾。

## 失败假设

- 仅在语音页 `DisposableEffect.onDispose` 中停止录音，假设页面看不见时 Composable 会立刻销毁。
- 该假设在工具中心使用 `AnimatedVisibility` 时不成立：退出动画期间内容仍保留在组合树中，TTS 完成回调也可能在销毁前重新启动 ASR。

## 根因

视觉不可见、导航状态改变和 Composable 销毁是不同的生命周期时点。麦克风属于隐私与独占硬件资源，不能只依赖最终的 `onDispose` 释放。

## 已验证修复

1. `ToolsScreen` 将返回主控、右滑关闭和标签切换统一收口；离开语音标签前立即调用 `VoiceAssistantController.detach()`。
2. `MainActivity.onStop()` 调用 `VoiceAssistantController.pause()` 与 `SpeakerRecognitionController.stopEnrollment()`，确保 App 退后台立即停录。
3. 语音页原 `onDispose` 继续保留为最后兜底，形成“导航动作 + Activity 生命周期 + Composable 销毁”三层防护。
4. 修复后 Debug/Release 单测、Release、lintVital 共 75 个任务通过，修复版覆盖安装成功。
5. 真机先观察到本包 `active? true`，随后出现对应 `rec stop`；等待后没有新的本包 `rec update`，用户最终反馈测试无误。

## 后续规则

- 麦克风、摄像头、定位等持续资源处于动画页面时，页面导航动作必须显式停止资源，不能把 `onDispose` 当作唯一释放入口。
- App 退后台必须在 Activity/Lifecycle 层再次停止持续采集；回前台是否恢复应由用户明确操作决定。
- 验收麦克风停止不能只看 UI，应保存 `dumpsys audio` 证据：先证明录音确实 active，再证明出现 stop，且等待至少 10 秒无新的本包 update。
