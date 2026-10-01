#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/asn1.h>
#include <mbedtls/x509_crt.h>
#include <time.h>

namespace {
constexpr uint8_t LED_PIN = 8;
constexpr uint8_t STM32_RX_PIN = 20;  // 接 STM32 PC10 / UART4_TX
constexpr uint8_t STM32_TX_PIN = 21;  // 接 STM32 PC11 / UART4_RX
constexpr uint32_t STM32_BAUD = 115200;

constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t MQTT_RETRY_MIN_MS = 2000;
constexpr uint32_t MQTT_RETRY_MAX_MS = 30000;
constexpr uint32_t CONTROL_TIMEOUT_MS = 300;
constexpr uint32_t TELEMETRY_PUBLISH_MS = 50;  // 20 Hz，合并三类最新遥测
constexpr uint32_t STATUS_PUBLISH_MS = 5000;

constexpr uint8_t CONTROL_FRAME_LENGTH = 20;
constexpr uint8_t CONTROL_HEADER_0 = 0x55;
constexpr uint8_t CONTROL_HEADER_1 = 0x43;
constexpr uint8_t CONTROL_VERSION = 0x01;
constexpr uint8_t CONTROL_TAIL = 0x5B;

constexpr uint8_t COMMAND_FORWARD = 0x41;
constexpr uint8_t COMMAND_FORWARD_RIGHT = 0x42;
constexpr uint8_t COMMAND_ROTATE_RIGHT = 0x43;
constexpr uint8_t COMMAND_BACKWARD_RIGHT = 0x44;
constexpr uint8_t COMMAND_BACKWARD = 0x45;
constexpr uint8_t COMMAND_BACKWARD_LEFT = 0x46;
constexpr uint8_t COMMAND_ROTATE_LEFT = 0x47;
constexpr uint8_t COMMAND_FORWARD_LEFT = 0x48;
constexpr uint8_t COMMAND_STOP = 0x49;
constexpr uint8_t COMMAND_MODE_MANUAL = 0x5B;
constexpr uint8_t COMMAND_MODE_AVOIDANCE = 0x5C;
constexpr uint8_t COMMAND_MODE_AUTONOMOUS = 0x5D;
constexpr uint8_t COMMAND_HORN_ON = 0x5E;
constexpr uint8_t COMMAND_HORN_OFF = 0x5F;
constexpr uint8_t COMMAND_LIGHT_OFF = 0x60;
constexpr uint8_t COMMAND_LIGHT_WATER = 0x61;
constexpr uint8_t COMMAND_LIGHT_FLASH = 0x62;
constexpr uint8_t COMMAND_SPEED_DOWN = 0x6A;
constexpr uint8_t COMMAND_SPEED_UP = 0x6B;

// 服务器 ECDSA 链中的长期信任锚（Root YE，有效至 2032-09-03）。
// Arduino-ESP32 2.0.17 的 Mbed TLS 2.28 对当前四层链顶层兼容不佳，因此从该 CA
// 开始验证叶证书与 YE2；TLS 栈仍校验证书签名、有效期和 MQTT_HOST 的 IP SAN。
constexpr char ROOT_YE_CA[] PROGMEM = R"EOF(-----BEGIN CERTIFICATE-----
MIICpjCCAiugAwIBAgIRAIchZfw0tuX7qK3Vs3BftTowCgYIKoZIzj0EAwMwTzEL
MAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2VhcmNo
IEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDIwHhcNMjYwNTEzMDAwMDAwWhcN
MzIwOTAyMjM1OTU5WjAuMQswCQYDVQQGEwJVUzENMAsGA1UEChMESVNSRzEQMA4G
A1UEAxMHUm9vdCBZRTB2MBAGByqGSM49AgEGBSuBBAAiA2IABDwS/6vhrcVqcbBo
+wgdI3fwn9x7DNJJOY/lTOti0vkwuRN87RhEhTH17E7XyFjWsPYhIPt/wzOqxTd2
b+4ZJNy9ID04YywF9U5zasDVyGSNErVNtz8uSGh5izW87j77GaOB6zCB6DAOBgNV
HQ8BAf8EBAMCAQYwEwYDVR0lBAwwCgYIKwYBBQUHAwEwDwYDVR0TAQH/BAUwAwEB
/zAdBgNVHQ4EFgQUo8gmWo6hTNA1Y/ybI8g6rlbzT1YwHwYDVR0jBBgwFoAUfEKW
rt5LSDv6kviejM9ti6lyN5UwMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAChhZo
dHRwOi8veDIuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcGA1Ud
HwQgMB4wHKAaoBiGFmh0dHA6Ly94Mi5jLmxlbmNyLm9yZy8wCgYIKoZIzj0EAwMD
aQAwZgIxAMU19WCtmxVND8UHBZRoma49Z7jPs64Dma0eTu1OChVbB/2J7GV3nvYK
Ax54uk1G9QIxAO0miLVJu8PLNiXXXkiE/gsK3CTRTF/aeo4bMX42Zw40csRU6AC2
6hSW1/IWaas6dg==
-----END CERTIFICATE-----
)EOF";

struct TelemetrySlot {
  uint8_t data[9] = {};
  uint8_t length = 0;
  bool valid = false;
  bool dirty = false;
};

HardwareSerial stm32Serial(1);
WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);
IPAddress mqttServerIp;

String clientId;
TelemetrySlot radarFrame;
TelemetrySlot powerFrame;
TelemetrySlot chassisFrame;
uint8_t uartFrame[9] = {};
uint8_t uartFrameIndex = 0;
uint8_t uartExpectedLength = 0;

uint32_t lastWifiAttemptMs = 0;
uint32_t lastMqttAttemptMs = 0;
uint32_t mqttRetryMs = MQTT_RETRY_MIN_MS;
uint32_t lastTelemetryPublishMs = 0;
uint32_t lastStatusPublishMs = 0;
uint32_t lastMotionControlMs = 0;
uint32_t uartValidFrames = 0;
uint32_t uartInvalidFrames = 0;
uint32_t mqttControlFrames = 0;
uint32_t mqttInvalidControls = 0;
uint8_t lastControlSequence = 0;
uint8_t lastControlCommand = COMMAND_STOP;
bool hasControlSequence = false;
bool motionActive = false;
bool hornActive = false;
bool statusDirty = true;

void setLed(bool on) { digitalWrite(LED_PIN, on ? LOW : HIGH); }

void updateStatusLed(uint32_t now) {
  if (mqttClient.connected()) {
    setLed((now % 2000U) < 80U);
  } else if (WiFi.status() == WL_CONNECTED) {
    setLed((now % 800U) < 400U);
  } else {
    setLed((now % 200U) < 100U);
  }
}

void printHex(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    Serial.printf("%02X%s", data[i], i + 1U == length ? "" : " ");
  }
  Serial.println();
}

uint8_t crc8Atm(const uint8_t *data, size_t length) {
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80U) ? static_cast<uint8_t>((crc << 1U) ^ 0x07U)
                          : static_cast<uint8_t>(crc << 1U);
    }
  }
  return crc;
}

bool isMotionCommand(uint8_t command) {
  return command >= COMMAND_FORWARD && command <= COMMAND_FORWARD_LEFT;
}

bool isAllowedCommand(uint8_t command) {
  return isMotionCommand(command) || command == COMMAND_STOP ||
         command == COMMAND_MODE_MANUAL || command == COMMAND_MODE_AVOIDANCE ||
         command == COMMAND_MODE_AUTONOMOUS || command == COMMAND_HORN_ON ||
         command == COMMAND_HORN_OFF || command == COMMAND_LIGHT_OFF ||
         command == COMMAND_LIGHT_WATER || command == COMMAND_LIGHT_FLASH || command == COMMAND_SPEED_DOWN ||
         command == COMMAND_SPEED_UP;
}

void sendStm32Command(uint8_t command) {
  stm32Serial.write(command);
  stm32Serial.flush();
  Serial.printf("[UART TX] STM32 command=0x%02X\n", command);
}

void forceSafeStop(const char *reason) {
  if (!motionActive && !hornActive) return;
  if (motionActive) {
    stm32Serial.write(COMMAND_STOP);
    stm32Serial.write(COMMAND_STOP);
    stm32Serial.write(COMMAND_STOP);
  }
  if (hornActive) stm32Serial.write(COMMAND_HORN_OFF);
  stm32Serial.flush();
  motionActive = false;
  hornActive = false;
  lastControlCommand = COMMAND_STOP;
  statusDirty = true;
  Serial.printf("[SAFE] %s; motion stopped and horn disabled\n", reason);
}

void cacheTelemetryFrame(const uint8_t *frame, uint8_t length) {
  TelemetrySlot *slot = nullptr;
  if (frame[1] == 0x51U && length == 9U) slot = &radarFrame;
  if (frame[1] == 0x52U && length == 8U) slot = &chassisFrame;
  if (frame[1] == 0x53U && length == 5U) slot = &powerFrame;
  if (slot == nullptr) return;

  memcpy(slot->data, frame, length);
  slot->length = length;
  slot->valid = true;
  slot->dirty = true;
  ++uartValidFrames;
  if (uartValidFrames <= 6U || uartValidFrames % 300U == 0U) {
    Serial.printf("[UART RX] valid type=0x%02X length=%u total=%lu: ", frame[1], length,
                  static_cast<unsigned long>(uartValidFrames));
    printHex(frame, length);
  }
}

void feedUartByte(uint8_t value) {
  if (uartFrameIndex == 0U) {
    if (value == 0x55U) {
      uartFrame[0] = value;
      uartFrameIndex = 1U;
    }
    return;
  }

  if (uartFrameIndex == 1U) {
    uartFrame[1] = value;
    uartExpectedLength = value == 0x51U ? 9U : value == 0x52U ? 8U : value == 0x53U ? 5U : 0U;
    if (uartExpectedLength == 0U) {
      uartFrameIndex = value == 0x55U ? 1U : 0U;
      ++uartInvalidFrames;
      statusDirty = true;
      return;
    }
    uartFrameIndex = 2U;
    return;
  }

  uartFrame[uartFrameIndex++] = value;
  if (uartFrameIndex < uartExpectedLength) return;

  if (uartFrame[uartExpectedLength - 1U] == 0x5BU) {
    cacheTelemetryFrame(uartFrame, uartExpectedLength);
  } else {
    ++uartInvalidFrames;
    statusDirty = true;
  }
  uartFrameIndex = value == 0x55U ? 1U : 0U;
  uartExpectedLength = 0U;
}

void serviceStm32Uart() {
  while (stm32Serial.available() > 0) {
    feedUartByte(static_cast<uint8_t>(stm32Serial.read()));
  }
}

bool controlFrameIsValid(const uint8_t *payload, unsigned int length, const char **reason) {
  if (length != CONTROL_FRAME_LENGTH) {
    *reason = "length";
    return false;
  }
  if (payload[0] != CONTROL_HEADER_0 || payload[1] != CONTROL_HEADER_1 ||
      payload[2] != CONTROL_VERSION || payload[19] != CONTROL_TAIL) {
    *reason = "header/version/tail";
    return false;
  }
  for (uint8_t i = 5U; i <= 17U; ++i) {
    if (payload[i] != 0U) {
      *reason = "reserved bytes";
      return false;
    }
  }
  if (crc8Atm(payload, 18U) != payload[18]) {
    *reason = "CRC-8";
    return false;
  }
  if (!isAllowedCommand(payload[4])) {
    *reason = "command whitelist";
    return false;
  }
  const uint8_t sequenceDelta = static_cast<uint8_t>(payload[3] - lastControlSequence);
  const bool staleSequence = hasControlSequence && (sequenceDelta == 0U || sequenceDelta > 127U);
  if (staleSequence && payload[4] != COMMAND_STOP) {
    *reason = "stale sequence";
    return false;
  }
  return true;
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  if (strcmp(topic, CONTROL_TOPIC) != 0) return;

  const char *invalidReason = nullptr;
  if (!controlFrameIsValid(payload, length, &invalidReason)) {
    ++mqttInvalidControls;
    statusDirty = true;
    Serial.printf("[MQTT RX] rejected control (%s), length=%u\n", invalidReason, length);
    forceSafeStop("invalid MQTT control frame");
    return;
  }

  const uint8_t sequence = payload[3];
  const uint8_t command = payload[4];
  const uint8_t sequenceDelta = static_cast<uint8_t>(sequence - lastControlSequence);
  const bool stopSequenceResync = hasControlSequence && command == COMMAND_STOP &&
                                  (sequenceDelta == 0U || sequenceDelta > 127U);
  hasControlSequence = true;
  lastControlSequence = sequence;
  lastControlCommand = command;
  ++mqttControlFrames;
  sendStm32Command(command);
  if (stopSequenceResync) {
    Serial.printf("[SAFE] STOP resynchronized control sequence to %u\n", sequence);
  }

  if (isMotionCommand(command)) {
    motionActive = true;
    lastMotionControlMs = millis();
  } else if (command == COMMAND_STOP || command == COMMAND_MODE_MANUAL ||
             command == COMMAND_MODE_AVOIDANCE || command == COMMAND_MODE_AUTONOMOUS) {
    motionActive = false;
  }
  if (command == COMMAND_HORN_ON) hornActive = true;
  if (command == COMMAND_HORN_OFF) hornActive = false;
  statusDirty = true;
  Serial.printf("[MQTT RX] accepted seq=%u command=0x%02X total=%lu\n", sequence, command,
                static_cast<unsigned long>(mqttControlFrames));
}

bool syncClock() {
  Serial.print("[NTP] synchronizing");
  configTime(0, 0, "ntp.aliyun.com", "pool.ntp.org", "time.cloudflare.com");
  const uint32_t started = millis();
  time_t now = time(nullptr);
  while (now < 1700000000 && millis() - started < 20000U) {
    serviceStm32Uart();
    updateStatusLed(millis());
    delay(100);
    now = time(nullptr);
  }
  Serial.println();
  if (now < 1700000000) {
    Serial.println("[NTP] failed; background networking continues");
    return false;
  }
  struct tm utc {};
  gmtime_r(&now, &utc);
  Serial.printf("[NTP] UTC %04d-%02d-%02d %02d:%02d:%02d\n", utc.tm_year + 1900,
                utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
  return true;
}

void maintainWifi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED || now - lastWifiAttemptMs < WIFI_RETRY_MS) return;
  lastWifiAttemptMs = now;
  Serial.printf("[WiFi] reconnecting to SSID %s\n", WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool peerCertificateHasExpectedIpSan(const mbedtls_x509_crt *peer) {
  if (peer == nullptr) return false;
  constexpr int IP_SAN_TAG = MBEDTLS_ASN1_CONTEXT_SPECIFIC | 7;
  for (const mbedtls_x509_sequence *san = &peer->subject_alt_names; san != nullptr;
       san = san->next) {
    if (san->buf.tag != IP_SAN_TAG || san->buf.len != 4U || san->buf.p == nullptr) continue;
    bool matches = true;
    for (size_t i = 0; i < 4U; ++i) {
      if (san->buf.p[i] != mqttServerIp[i]) {
        matches = false;
        break;
      }
    }
    if (matches) return true;
  }
  return false;
}

bool verifyPeerBeforeAuthentication() {
  mbedtls_x509_crt *peer = const_cast<mbedtls_x509_crt *>(tlsClient.getPeerCertificate());
  if (peer == nullptr) {
    Serial.println("[TLS] FATAL: server did not provide a certificate; credentials not sent");
    return false;
  }

  mbedtls_x509_crt trustedCa;
  mbedtls_x509_crt_init(&trustedCa);
  int result = mbedtls_x509_crt_parse(
      &trustedCa, reinterpret_cast<const unsigned char *>(ROOT_YE_CA),
      strlen(ROOT_YE_CA) + 1U);
  uint32_t flags = 0;
  if (result == 0) {
    // Mbed TLS 2.28 only supports DNS names in its built-in name check. Verify the
    // cryptographic chain/time here, then verify the IPv4 SAN separately below.
    result = mbedtls_x509_crt_verify(peer, &trustedCa, nullptr, nullptr, &flags, nullptr,
                                     nullptr);
  }
  mbedtls_x509_crt_free(&trustedCa);

  if (result != 0 || flags != 0U) {
    char details[256] = {};
    mbedtls_x509_crt_verify_info(details, sizeof(details), "", flags);
    Serial.printf("[TLS] FATAL: certificate chain/time verification failed: %d flags=0x%08lX %s; "
                  "credentials not sent\n",
                  result, static_cast<unsigned long>(flags), details);
    return false;
  }
  if (!peerCertificateHasExpectedIpSan(peer)) {
    Serial.printf("[TLS] FATAL: certificate IP SAN does not match %s; credentials not sent\n",
                  MQTT_HOST);
    return false;
  }
  return true;
}

bool publishStatus(bool retained) {
  if (!mqttClient.connected()) return false;
  char payload[256];
  snprintf(payload, sizeof(payload),
           "{\"online\":true,\"bridge\":\"esp32c3-uart4\",\"uart_frames\":%lu,"
           "\"uart_errors\":%lu,\"control_rx\":%lu,\"control_errors\":%lu,"
           "\"last_seq\":%u,\"last_cmd\":%u,\"rssi\":%ld}",
           static_cast<unsigned long>(uartValidFrames),
           static_cast<unsigned long>(uartInvalidFrames),
           static_cast<unsigned long>(mqttControlFrames),
           static_cast<unsigned long>(mqttInvalidControls), lastControlSequence,
           lastControlCommand, static_cast<long>(WiFi.RSSI()));
  return mqttClient.publish(STATUS_TOPIC, payload, retained);
}

bool connectMqtt() {
  Serial.printf("[TLS] connecting with CA verification to %s:%u\n", MQTT_HOST,
                static_cast<unsigned int>(MQTT_PORT));
  tlsClient.stop();
  // Arduino-ESP32 2.0.17 cannot match an IP-address SAN during its built-in handshake.
  // Complete the encrypted handshake first, then perform chain/time/IP-SAN verification
  // ourselves before PubSubClient is allowed to send MQTT credentials.
  tlsClient.setInsecure();
  if (!tlsClient.connect(MQTT_HOST, MQTT_PORT)) {
    char tlsError[160] = {};
    const int tlsCode = tlsClient.lastError(tlsError, sizeof(tlsError));
    Serial.printf("[TLS] verified handshake failed: %d (%s); credentials not sent\n",
                  tlsCode, tlsError);
    return false;
  }
  if (!verifyPeerBeforeAuthentication()) {
    tlsClient.stop();
    return false;
  }
  Serial.println("[TLS] certificate chain, validity, and endpoint identity verified");

  const char *willPayload = "{\"online\":false,\"bridge\":\"esp32c3-uart4\"}";
  if (!mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, STATUS_TOPIC, 0,
                          true, willPayload, true)) {
    Serial.printf("[MQTT] authentication failed, state=%d\n", mqttClient.state());
    return false;
  }

  hasControlSequence = false;
  mqttRetryMs = MQTT_RETRY_MIN_MS;
  const bool subscribed = mqttClient.subscribe(CONTROL_TOPIC, 0);
  const bool statusPublished = publishStatus(true);
  statusDirty = !(subscribed && statusPublished);
  Serial.printf("[MQTT] connected; control_subscribe=%s, status_publish=%s\n",
                subscribed ? "OK" : "FAIL", statusPublished ? "OK" : "FAIL");
  return subscribed && statusPublished;
}

void maintainMqtt(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED) {
    forceSafeStop("Wi-Fi disconnected");
    if (mqttClient.connected()) mqttClient.disconnect();
    return;
  }
  if (mqttClient.connected()) return;

  forceSafeStop("MQTT disconnected");
  if (now - lastMqttAttemptMs < mqttRetryMs) return;
  lastMqttAttemptMs = now;
  if (!connectMqtt()) mqttRetryMs = min(mqttRetryMs * 2U, MQTT_RETRY_MAX_MS);
}

void appendTelemetry(uint8_t *payload, size_t *length, TelemetrySlot &slot) {
  if (!slot.valid) return;
  memcpy(payload + *length, slot.data, slot.length);
  *length += slot.length;
}

void publishTelemetry(uint32_t now) {
  if (!mqttClient.connected() || now - lastTelemetryPublishMs < TELEMETRY_PUBLISH_MS) return;
  if (!radarFrame.dirty && !powerFrame.dirty && !chassisFrame.dirty) return;
  lastTelemetryPublishMs = now;

  uint8_t payload[22] = {};
  size_t length = 0;
  appendTelemetry(payload, &length, radarFrame);
  appendTelemetry(payload, &length, powerFrame);
  appendTelemetry(payload, &length, chassisFrame);
  if (length == 0U) return;

  if (mqttClient.publish(TELEMETRY_TOPIC, payload, length, false)) {
    radarFrame.dirty = false;
    powerFrame.dirty = false;
    chassisFrame.dirty = false;
  }
}

void publishPeriodicStatus(uint32_t now) {
  if (!mqttClient.connected()) return;
  if (!statusDirty && now - lastStatusPublishMs < STATUS_PUBLISH_MS) return;
  if (publishStatus(true)) {
    statusDirty = false;
    lastStatusPublishMs = now;
  }
}
}  // namespace

void setup() {
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  Serial.begin(115200);
  delay(350);

  Serial.println("\nESP32-C3 STM32 UART4 <-> MQTT bridge");
  Serial.printf("[UART] Serial1 %lu-8-N-1, RX=GPIO%u, TX=GPIO%u\n",
                static_cast<unsigned long>(STM32_BAUD), STM32_RX_PIN, STM32_TX_PIN);
  Serial.println("[SAFE] 300 ms motion timeout; invalid/disconnected control sends STOP 0x49");
  stm32Serial.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  stm32Serial.write(COMMAND_STOP);
  stm32Serial.write(COMMAND_STOP);
  stm32Serial.write(COMMAND_STOP);
  stm32Serial.flush();
  Serial.println("[SAFE] startup STOP 0x49 x3 sent to STM32");

  const uint64_t chipId = ESP.getEfuseMac();
  char idBuffer[32];
  snprintf(idBuffer, sizeof(idBuffer), "vehicle-c3-%08lX",
           static_cast<unsigned long>(chipId & 0xFFFFFFFFULL));
  clientId = idBuffer;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] initial connection to SSID %s\n", WIFI_SSID);

  const uint32_t waitStarted = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - waitStarted < 20000U) {
    serviceStm32Uart();
    updateStatusLed(millis());
    delay(20);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] connected, IP=%s, RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    syncClock();
    tlsClient.setHandshakeTimeout(15);
  } else {
    Serial.println("[WiFi] initial timeout; background retry enabled");
  }

  if (!mqttServerIp.fromString(MQTT_HOST)) {
    Serial.println("[MQTT] invalid fixed server IP");
  } else {
    mqttClient.setServer(mqttServerIp, MQTT_PORT);
  }
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(15);
  mqttClient.setBufferSize(512);
}

void loop() {
  const uint32_t now = millis();
  serviceStm32Uart();
  updateStatusLed(now);
  maintainWifi(now);
  maintainMqtt(now);
  if (mqttClient.connected()) mqttClient.loop();
  publishTelemetry(now);
  publishPeriodicStatus(now);

  const uint32_t watchdogNow = millis();
  if (motionActive && watchdogNow - lastMotionControlMs > CONTROL_TIMEOUT_MS) {
    forceSafeStop("motion control timeout exceeded 300 ms");
  }
  delay(1);
}
