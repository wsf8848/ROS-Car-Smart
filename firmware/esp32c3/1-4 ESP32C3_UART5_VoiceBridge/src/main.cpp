/*
 * 文件名称：main.cpp
 * 功能说明：将STM32 UART5发出的230400波特率二进制数据透明转发给115200波特率语音模块
 * 软件版本：V1.0.0
 * 编写日期：2026-09-28 00:00（Asia/Shanghai）
 * 修改人员：Bee / Codex
 * 硬件平台：ESP32-C3 Super Mini，4MB Flash，无PSRAM
 * 开发框架：Arduino-ESP32 2.0.17 / PlatformIO espressif32 7.1.3
 */

#include <Arduino.h>

namespace {

// 引脚沿用已验证的ESP32-C3 MQTT桥接工程：GPIO20作为输入，GPIO21作为输出。
constexpr uint8_t LED_PIN = 8;                 // GPIO8，板载蓝色LED，低电平点亮。
constexpr int8_t STM32_RX_PIN = 20;            // GPIO20，接STM32 UART5_TX，只接收数据。
constexpr int8_t VOICE_TX_PIN = 21;            // GPIO21，接语音模块RX，只发送数据。
constexpr int8_t UART_UNUSED_PIN = -1;         // 未使用方向不分配引脚，避免产生悬空输出。

constexpr uint32_t STM32_INPUT_BAUD = 230400;  // STM32 UART5 TX输入波特率，8-N-1。
constexpr uint32_t VOICE_OUTPUT_BAUD = 115200; // 语音模块输出波特率，8-N-1。
constexpr uint32_t USB_LOG_BAUD = 115200;      // USB CDC日志标称波特率，仅供监视器配置。

constexpr size_t BRIDGE_QUEUE_SIZE = 4096;     // 速率转换缓冲区；持续高速输入仍受输出带宽限制。
constexpr size_t INPUT_RX_BUFFER_SIZE = 2048;  // UART0硬件驱动接收缓冲区，覆盖短时指令突发。
constexpr uint32_t ACTIVE_HOLD_MS = 500;       // 最后一个输入字节后500ms内视为正在接收。
constexpr uint32_t FAST_BLINK_MS = 80;         // 接收期间每80ms翻转一次LED。
constexpr uint32_t IDLE_PERIOD_MS = 1000;      // 空闲时每秒闪一次。
constexpr uint32_t IDLE_PULSE_MS = 90;         // 空闲心跳单次点亮90ms。
constexpr uint32_t BURST_END_MS = 30;          // 连续30ms无新字节后结束一次调试统计。
constexpr uint32_t STATUS_PERIOD_MS = 5000;    // 每5秒输出一次累计桥接状态。
constexpr size_t BURST_PREVIEW_SIZE = 64;      // 每次数据突发最多预览64字节十六进制。

// ESP32-C3的USB CDC用于日志，因此两个硬件UART可分别使用不同波特率。
HardwareSerial stm32InputUart(0);              // UART0仅RX：230400，GPIO20。
HardwareSerial voiceOutputUart(1);             // UART1仅TX：115200，GPIO21。

uint8_t bridgeQueue[BRIDGE_QUEUE_SIZE] = {};   // 软件环形缓冲区，保存待转发的原始字节。
size_t queueHead = 0;                          // 下一个写入位置。
size_t queueTail = 0;                          // 下一个发送位置。

uint64_t totalReceivedBytes = 0;               // STM32侧累计收到的字节数。
uint64_t totalForwardedBytes = 0;              // 语音模块侧累计发出的字节数。
uint64_t totalDroppedBytes = 0;                // 缓冲区满时丢弃的字节数。
uint32_t lastRxByteMs = 0;                     // 最近一次收到字节的时间。
uint32_t lastLedToggleMs = 0;                  // 接收快闪的最近翻转时间。
uint32_t lastStatusMs = 0;                     // 最近一次状态日志时间。
bool ledOn = false;                            // 当前LED逻辑状态。

uint8_t burstPreview[BURST_PREVIEW_SIZE] = {}; // 当前突发数据的前64字节，只用于调试打印。
size_t burstPreviewLength = 0;                 // 当前已保存的预览长度。
uint32_t burstBytes = 0;                       // 当前突发数据的总字节数。
bool burstPending = false;                     // 是否存在尚未输出摘要的数据突发。

void setLed(bool on) {
  ledOn = on;                                  // 保存逻辑状态，供快闪翻转使用。
  digitalWrite(LED_PIN, on ? LOW : HIGH);      // 板载LED低电平点亮。
}

size_t queueUsed() {
  return (queueHead + BRIDGE_QUEUE_SIZE - queueTail) % BRIDGE_QUEUE_SIZE; // 计算已占用字节数。
}

size_t queueFree() {
  return BRIDGE_QUEUE_SIZE - 1U - queueUsed(); // 环形队列保留一个空位区分空与满。
}

bool queuePush(uint8_t value) {
  const size_t nextHead = (queueHead + 1U) % BRIDGE_QUEUE_SIZE; // 计算写入后的新位置。
  if (nextHead == queueTail) return false;      // 队列已满时拒绝写入，不覆盖旧指令。
  bridgeQueue[queueHead] = value;               // 原样保存输入字节，不解析、不修改协议。
  queueHead = nextHead;                         // 提交本次写入。
  return true;                                  // 返回入队成功。
}

void recordBurstByte(uint8_t value) {
  if (burstPreviewLength < BURST_PREVIEW_SIZE) { // 只保留有限预览，避免日志占用过多内存。
    burstPreview[burstPreviewLength++] = value;  // 保存真实接收的十六进制字节。
  }
  ++burstBytes;                                  // 累计本次突发总长度。
  burstPending = true;                           // 标记后续需要输出摘要。
}

void receiveFromStm32(uint32_t now) {
  while (stm32InputUart.available() > 0) {       // 一次循环尽量取空UART硬件缓冲区。
    const int incoming = stm32InputUart.read();  // 从GPIO20读取一个原始字节。
    if (incoming < 0) break;                     // 驱动没有返回有效字节时退出。
    const uint8_t value = static_cast<uint8_t>(incoming); // 保留0x00至0xFF全部二进制值。
    ++totalReceivedBytes;                        // 更新输入统计。
    lastRxByteMs = now;                          // 触发接收快闪状态。
    recordBurstByte(value);                      // 保存有限十六进制预览用于联调。
    if (!queuePush(value)) ++totalDroppedBytes;  // 缓冲耗尽时如实记录丢包数量。
  }
}

void forwardToVoiceModule() {
  while (queueTail != queueHead) {               // 队列非空时持续尝试向语音模块发送。
    const int writable = voiceOutputUart.availableForWrite(); // 查询UART1当前可接收的发送空间。
    if (writable <= 0) return;                   // TX缓冲暂满时立即让出CPU，下一轮继续。

    const size_t contiguous = queueHead > queueTail
                                ? queueHead - queueTail
                                : BRIDGE_QUEUE_SIZE - queueTail; // 计算当前连续数据长度。
    const size_t sendLength = min(contiguous, static_cast<size_t>(writable)); // 限制本次写入量。
    const size_t written = voiceOutputUart.write(&bridgeQueue[queueTail], sendLength); // 原样写入GPIO21。
    if (written == 0U) return;                   // 驱动未接受数据时不忙等。
    queueTail = (queueTail + written) % BRIDGE_QUEUE_SIZE; // 释放已发送的队列空间。
    totalForwardedBytes += written;              // 更新输出统计。
  }
}

void updateWorkLed(uint32_t now) {
  const bool receiving = totalReceivedBytes > 0U && now - lastRxByteMs <= ACTIVE_HOLD_MS; // 判断接收活跃窗口。
  if (receiving) {                               // 接收期间执行快速闪烁。
    if (now - lastLedToggleMs >= FAST_BLINK_MS) {
      lastLedToggleMs = now;                     // 保存本次快闪时间。
      setLed(!ledOn);                            // 翻转板载LED。
    }
    return;                                      // 快闪优先，不再执行空闲心跳。
  }
  setLed((now % IDLE_PERIOD_MS) < IDLE_PULSE_MS); // 空闲状态每秒短亮一次。
}

void printBurstIfComplete(uint32_t now) {
  if (!burstPending || now - lastRxByteMs < BURST_END_MS) return; // 等待突发数据结束。
  Serial.printf("[RX] burst=%lu bytes, preview=", static_cast<unsigned long>(burstBytes)); // 打印突发长度。
  for (size_t index = 0; index < burstPreviewLength; ++index) {
    Serial.printf("%02X%s", burstPreview[index], index + 1U == burstPreviewLength ? "" : " "); // 十六进制输出。
  }
  if (burstBytes > burstPreviewLength) Serial.print(" ..."); // 超过64字节时明确提示截断。
  Serial.println();                                // 完成本次摘要换行。
  burstPreviewLength = 0;                          // 清空预览长度，准备下一次突发。
  burstBytes = 0;                                  // 清空突发计数。
  burstPending = false;                            // 清除待输出标志。
}

void printPeriodicStatus(uint32_t now) {
  if (now - lastStatusMs < STATUS_PERIOD_MS) return; // 未到周期时不输出日志。
  lastStatusMs = now;                              // 保存本次日志时间。
  Serial.printf("[STATUS] rx=%llu, tx=%llu, dropped=%llu, queued=%u, free=%u\n",
                totalReceivedBytes, totalForwardedBytes, totalDroppedBytes,
                static_cast<unsigned int>(queueUsed()), static_cast<unsigned int>(queueFree())); // 输出完整状态。
}

} // namespace

/*
 * 主入口版本：V1.0.0
 * 修改时间：2026-09-28 00:00（Asia/Shanghai）
 * 修改人员：Bee / Codex
 * 本版摘要：完成230400输入、115200输出、透明二进制缓存转发与双速LED提示。
 */
void setup() {
  pinMode(LED_PIN, OUTPUT);                        // 配置板载LED为推挽输出。
  setLed(false);                                  // 上电默认熄灭，避免误表示正在接收。

  Serial.begin(USB_LOG_BAUD);                     // 启动原生USB CDC调试日志。
  delay(350);                                     // 给Windows USB串口重新枚举留出时间。

  stm32InputUart.setRxBufferSize(INPUT_RX_BUFFER_SIZE); // 在begin前扩大UART0接收缓冲。
  stm32InputUart.begin(STM32_INPUT_BAUD, SERIAL_8N1, STM32_RX_PIN, UART_UNUSED_PIN); // 启动230400 RX。
  voiceOutputUart.begin(VOICE_OUTPUT_BAUD, SERIAL_8N1, UART_UNUSED_PIN, VOICE_TX_PIN); // 启动115200 TX。

  Serial.println();                               // 与ROM启动日志分隔。
  Serial.println("Bee ESP32-C3 UART5 -> Voice Bridge V1.0.0"); // 输出固件身份。
  Serial.printf("[INPUT] UART0 RX GPIO%d, %lu-8-N-1, source=STM32 UART5_TX\n",
                STM32_RX_PIN, static_cast<unsigned long>(STM32_INPUT_BAUD)); // 输出输入配置。
  Serial.printf("[OUTPUT] UART1 TX GPIO%d, %lu-8-N-1, target=voice module RX\n",
                VOICE_TX_PIN, static_cast<unsigned long>(VOICE_OUTPUT_BAUD)); // 输出输出配置。
  Serial.printf("[LED] GPIO%d active-low; receiving=fast blink, idle=1Hz pulse\n", LED_PIN); // 输出LED规则。
  Serial.printf("[BUFFER] software=%u bytes, UART RX=%u bytes\n",
                static_cast<unsigned int>(BRIDGE_QUEUE_SIZE),
                static_cast<unsigned int>(INPUT_RX_BUFFER_SIZE)); // 输出缓冲区参数。
  Serial.println("[READY] Transparent binary bridge started"); // 标记初始化完成。
}

void loop() {
  const uint32_t now = millis();                  // 获取当前毫秒计数，统一驱动状态机。
  receiveFromStm32(now);                          // 优先接收STM32数据，降低输入溢出概率。
  forwardToVoiceModule();                         // 将缓存数据按115200速率发送给语音模块。
  updateWorkLed(now);                             // 更新接收快闪或空闲秒闪。
  printBurstIfComplete(now);                      // 突发结束后输出十六进制摘要。
  printPeriodicStatus(now);                       // 周期输出累计收发与丢包状态。
  delay(1);                                       // 让出CPU，避免空闲忙等。
}

