#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

namespace {
constexpr uint8_t LED_PIN = 8;
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t MQTT_RETRY_MIN_MS = 2000;
constexpr uint32_t MQTT_RETRY_MAX_MS = 30000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 5000;
constexpr uint32_t CONTROL_TIMEOUT_MS = 400;
constexpr uint8_t EXPECTED_SERVER_SHA256[32] = {
    0x4F, 0xA4, 0xE8, 0x2D, 0xD5, 0x2A, 0x56, 0xD9,
    0xB0, 0x77, 0x78, 0xA7, 0xB2, 0xFF, 0x89, 0x23,
    0xF8, 0x7F, 0xAA, 0x93, 0xCF, 0x29, 0x6A, 0xC1,
    0x32, 0xF4, 0x44, 0x35, 0x02, 0x43, 0xA4, 0xEB};

WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);
IPAddress mqttServerIp;

String clientId;
uint32_t lastWifiAttemptMs = 0;
uint32_t lastMqttAttemptMs = 0;
uint32_t mqttRetryMs = MQTT_RETRY_MIN_MS;
uint32_t lastTelemetryMs = 0;
uint32_t lastValidControlMs = 0;
uint32_t receivedControlFrames = 0;
bool controlTimedOut = true;

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

void enterSafeState(const char *reason) {
  if (!controlTimedOut) {
    Serial.printf("[SAFE] %s; actuator output remains disabled\n", reason);
  }
  controlTimedOut = true;
}

void printHexPayload(const uint8_t *payload, unsigned int length) {
  for (unsigned int i = 0; i < length; ++i) {
    Serial.printf("%02X%s", payload[i], i + 1U == length ? "" : " ");
  }
  Serial.println();
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  if (strcmp(topic, CONTROL_TOPIC) != 0) return;

  Serial.printf("[MQTT] control received, length=%u: ", length);
  printHexPayload(payload, length);
  if (length != 20U) {
    enterSafeState("invalid control length");
    return;
  }

  ++receivedControlFrames;
  lastValidControlMs = millis();
  controlTimedOut = false;
  Serial.println("[TEST] valid 20-byte frame accepted; no motor is connected");
}

bool syncClock() {
  Serial.print("[NTP] synchronizing");
  configTime(0, 0, "ntp.aliyun.com", "pool.ntp.org", "time.cloudflare.com");
  const uint32_t started = millis();
  time_t now = time(nullptr);
  while (now < 1700000000 && millis() - started < 20000U) {
    delay(250);
    Serial.print('.');
    now = time(nullptr);
  }
  Serial.println();
  if (now < 1700000000) {
    Serial.println("[NTP] failed; TLS connection postponed");
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
  Serial.printf("[WiFi] connecting to SSID %s\n", WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool connectMqtt() {
  Serial.printf("[TLS] connecting to pinned endpoint %s:%u\n", MQTT_HOST,
                static_cast<unsigned int>(MQTT_PORT));
  tlsClient.stop();
  tlsClient.setInsecure();
  if (!tlsClient.connect(mqttServerIp, MQTT_PORT)) {
    char tlsError[160] = {};
    const int tlsCode = tlsClient.lastError(tlsError, sizeof(tlsError));
    Serial.printf("[TLS] handshake failed: %d (%s)\n", tlsCode, tlsError);
    return false;
  }

  uint8_t actualFingerprint[32] = {};
  if (!tlsClient.getFingerprintSHA256(actualFingerprint) ||
      memcmp(actualFingerprint, EXPECTED_SERVER_SHA256, sizeof(actualFingerprint)) != 0) {
    Serial.println("[TLS] FATAL: server certificate SHA-256 mismatch; credentials not sent");
    tlsClient.stop();
    return false;
  }
  Serial.println("[TLS] server certificate SHA-256 pin verified before authentication");

  Serial.printf("[MQTT] authenticating as %s\n", MQTT_USERNAME);
  const bool connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD,
                                             STATUS_TOPIC, 0, true, "offline", true);
  if (!connected) {
    char tlsError[160] = {};
    const int tlsCode = tlsClient.lastError(tlsError, sizeof(tlsError));
    Serial.printf("[MQTT] connect failed, state=%d, TLS=%d (%s)\n",
                  mqttClient.state(), tlsCode, tlsError);
    return false;
  }

  mqttRetryMs = MQTT_RETRY_MIN_MS;
  const bool subscribed = mqttClient.subscribe(CONTROL_TOPIC, 0);
  const bool statusPublished = mqttClient.publish(
      STATUS_TOPIC, "online:esp32c3-mqtt-test", true);
  Serial.printf("[MQTT] connected; subscribe=%s, online_publish=%s\n",
                subscribed ? "OK" : "FAIL", statusPublished ? "OK" : "FAIL");
  return subscribed && statusPublished;
}

void maintainMqtt(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED) {
    if (mqttClient.connected()) mqttClient.disconnect();
    enterSafeState("Wi-Fi disconnected");
    return;
  }
  if (mqttClient.connected()) return;
  if (now - lastMqttAttemptMs < mqttRetryMs) return;

  lastMqttAttemptMs = now;
  if (!connectMqtt()) {
    mqttRetryMs = min(mqttRetryMs * 2U, MQTT_RETRY_MAX_MS);
  }
}

void publishTestTelemetry(uint32_t now) {
  if (!mqttClient.connected() || now - lastTelemetryMs < TELEMETRY_INTERVAL_MS) return;
  lastTelemetryMs = now;

  uint8_t frame[20] = {'C', '3', 'T', 'E', 'S', 'T', 1, 0};
  frame[8] = static_cast<uint8_t>(receivedControlFrames >> 24U);
  frame[9] = static_cast<uint8_t>(receivedControlFrames >> 16U);
  frame[10] = static_cast<uint8_t>(receivedControlFrames >> 8U);
  frame[11] = static_cast<uint8_t>(receivedControlFrames);
  frame[12] = static_cast<uint8_t>(now >> 24U);
  frame[13] = static_cast<uint8_t>(now >> 16U);
  frame[14] = static_cast<uint8_t>(now >> 8U);
  frame[15] = static_cast<uint8_t>(now);
  const int32_t rssi = WiFi.RSSI();
  frame[16] = static_cast<uint8_t>(rssi >> 24U);
  frame[17] = static_cast<uint8_t>(rssi >> 16U);
  frame[18] = static_cast<uint8_t>(rssi >> 8U);
  frame[19] = static_cast<uint8_t>(rssi);

  const bool published = mqttClient.publish(TELEMETRY_TOPIC, frame, sizeof(frame), false);
  Serial.printf("[MQTT] 20-byte telemetry publish=%s, controls=%lu, RSSI=%ld dBm\n",
                published ? "OK" : "FAIL",
                static_cast<unsigned long>(receivedControlFrames),
                static_cast<long>(rssi));
}
}  // namespace

void setup() {
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  Serial.begin(115200);
  delay(400);

  Serial.println("\nESP32-C3 secure MQTT integration test");
  Serial.println("TLS test mode: verify SHA-256 certificate pin before MQTT credentials");
  Serial.println("Actuator output: disabled (network-only test)");

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
    updateStatusLed(millis());
    delay(50);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] connected, IP=%s, RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    if (syncClock()) {
      tlsClient.setHandshakeTimeout(15);
    }
  } else {
    Serial.println("[WiFi] initial connection timed out; background retry enabled");
  }

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  if (!mqttServerIp.fromString(MQTT_HOST)) {
    Serial.println("[MQTT] invalid fixed server IP");
  } else {
    mqttClient.setServer(mqttServerIp, MQTT_PORT);
    Serial.println("[TLS] test mode: fixed IP plus pre-auth SHA-256 certificate pin");
  }
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(15);
  mqttClient.setBufferSize(512);
}

void loop() {
  const uint32_t now = millis();
  updateStatusLed(now);
  maintainWifi(now);
  maintainMqtt(now);
  if (mqttClient.connected()) mqttClient.loop();
  publishTestTelemetry(now);

  if (!controlTimedOut && now - lastValidControlMs > CONTROL_TIMEOUT_MS) {
    enterSafeState("control watchdog exceeded 400 ms");
  }
  delay(2);
}
