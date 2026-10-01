// 定义WiFi摄像头网络模式 Define the WiFi camera network mode
#define AP_MODE 0
#define STA_MODE 1
#define STA_AP_MODE 2

// 配置网络模式参数 Set network mode parameters
const char* WIFI_Name = "Yahboom2";           // 设置需要连接的WiFi名称 Set the name of the WiFi to be connected
const char* WIFI_Password = "yahboom890729";  // 设置需要连接WiFi的密码 Set the password that you need to connect to WiFi
const char* AP_Name = "Yahboom_ESP32_WIFI";   // 设置自发热点的名称 Set the name of the spontaneous hotspot
const char* AP_Password = "";                 // 设置自发热点的密码 Set the password of the spontaneous hotspot
const int Mode = STA_AP_MODE;                 // 设置网络模式 Set network mode

void getVersion(void);                                           // 获取WiFi摄像头版本 Get WiFi camera version
void setReset(void);                                             // 恢复出厂设置 Restore factory settings
void setMODE(const int Mode);                                    // 设置网络模式 Set the network mode
void SetWIFI(const char* WIFI_Name, const char* WIFI_Password);  // 设置WiFi连接信息 Set WiFi connection information
void SetAP(const char* AP_Name, const char* AP_Password);        // 设置自发热点信息 Set hotspot mode information
void getSTAIP(void);                                             // 获取STA模式IP Get STA mode IP
void getAPIP(void);                                              // 获取AP模式IP Get AP mode IP

void setup() {
  Serial.begin(115200);               // 初始化串口波特率115200 Initialize serial communication at 115200 bps
  setMODE(STA_AP_MODE);               // 设置网络模式 Set the network mode
  SetWIFI(WIFI_Name, WIFI_Password);  // 设置WiFi连接信息 Set WiFi connection information
  SetAP(AP_Name, AP_Password);        // 设置自发热点信息 Set hotspot mode information
  getSTAIP();                         // 获取STA模式IP Get STA mode IP
  getAPIP();                          // 获取AP模式IP Get AP mode IP
  if (Serial.available() > 0) {
    Serial.print(Serial.readString());  // 打印WiFi摄像头返回的信息 Print the information returned by the WiFi camera
  }
}

void loop() {
}

/**
 * @brief 获取wiFi摄像头版本 Get WiFi camera version
 * @param 无 None
 * @retval 无 None
 */
void getVersion(void) {
  Serial.print("wifi_ver");
  delay(500);
}

/**
 * @brief 恢复出厂设置 Restore factory settings
 * @param 无 None
 * @retval 无 None
 */
void setReset(void) {
  Serial.print("wifi_reset");
  delay(2000);
}

/**
 * @brief 设置网络模式 Set the network mode
 * @param Mode: 网络模式 The network mode
 * @retval 无 None
 */
void setMODE(const int Mode) {
  Serial.print("wifi_mode:" + String(Mode));
  delay(1000);
}

/**
 * @brief 设置wiFi连接信 Set WiFi connection information
 * @param WIFI_Name: WiFi名称 WIFI Name
 * @param WIFI_Password: WiFi密码 WIFI Password
 * @retval 无 None
 */
void SetWIFI(const char* WIFI_Name, const char* WIFI_Password) {
  Serial.print("sta_ssid:" + String(WIFI_Name));
  delay(1000);
  Serial.print("sta_pd:" + String(WIFI_Password));
  delay(5000);
}

/**
 * @brief 设置自发热点信息 Set AP connection information
 * @param AP_Name: 热点名称 AP Name
 * @param AP_Password: 热点密码 AP Password
 * @retval 无 None
 */
void SetAP(const char* AP_Name, const char* AP_Password) {
  delay(500);
  Serial.print("ap_ssid:" + String(AP_Name));
  delay(1000);
  Serial.print("ap_pd:" + String(AP_Password));
  delay(5000);
}

/**
 * @brief 获取STA模式IP Get STA ip
 * @param 无 None
 * @retval 无 None
 */
void getSTAIP(void) {
  Serial.print("sta_ip");
  delay(500);
}

/**
 * @brief 获取AP模式IP Get AP ip
 * @param 无 None
 * @retval 无 None
 */
void getAPIP(void) {
  Serial.print("ap_ip");
  delay(500);
}