// ESP32-C3 Super Mini 板载蓝色 LED 快闪测试
// 购买资料标注板载 LED 连接 GPIO8，通常为低电平点亮。

constexpr uint8_t LED_PIN = 8;
constexpr uint32_t BLINK_INTERVAL_MS = 100;

bool ledOn = false;
uint32_t lastBlinkAt = 0;
uint32_t blinkCount = 0;

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("ESP32-C3 Super Mini LED fast-blink test started");
  Serial.println("Board LED: GPIO8, active LOW, interval: 100 ms");
}

void loop() {
  const uint32_t now = millis();
  if (now - lastBlinkAt >= BLINK_INTERVAL_MS) {
    lastBlinkAt = now;
    setLed(!ledOn);
    ++blinkCount;

    // 每秒输出一次运行心跳，不影响 LED 快闪。
    if (blinkCount % 10 == 0) {
      Serial.printf("Running: %lu ms, LED toggles: %lu\n",
                    static_cast<unsigned long>(now),
                    static_cast<unsigned long>(blinkCount));
    }
  }
}
