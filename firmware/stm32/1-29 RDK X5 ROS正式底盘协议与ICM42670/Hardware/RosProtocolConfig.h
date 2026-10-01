#ifndef __ROS_PROTOCOL_CONFIG_H__                       // 防止ROS底盘配置头文件被重复包含
#define __ROS_PROTOCOL_CONFIG_H__                       // 定义ROS底盘配置头文件保护宏

#define ROS_PROTOCOL_VERSION 1U                         // 定义Bee USART1正式协议版本号
#define ROS_PROTOCOL_MAX_PAYLOAD 512U                   // 定义正式协议允许的最大载荷长度
#define ROS_PROTOCOL_COMMAND_TIMEOUT_MS 350UL           // 定义ROS速度命令本地失联停车时限
#define ROS_PROTOCOL_TELEMETRY_PERIOD_MS 50UL           // 定义正式遥测发送周期为五十毫秒
#define ROS_PROTOCOL_IMU_PERIOD_MS 10UL                 // 定义ICM42670采样周期为十毫秒
#define ROS_PROTOCOL_ROS_PWM_ENABLED 0U                 // 未完成实车标定前保持ROS动力输出安全锁定
#define ROS_PROTOCOL_WHEEL_RADIUS_M 0.000000f           // 待实测轮半径，当前零值表示尚未标定
#define ROS_PROTOCOL_TRACK_WIDTH_M 0.000000f            // 待实测左右轮中心距，当前零值表示尚未标定
#define ROS_PROTOCOL_ENCODER_CPR 0.000000f              // 待实测单轮每圈编码器计数，当前零值表示尚未标定
#define ROS_PROTOCOL_MAX_LINEAR_MPS 0.800000f           // 定义协议层允许的最大线速度绝对值
#define ROS_PROTOCOL_MAX_ANGULAR_RADPS 2.000000f        // 定义协议层允许的最大角速度绝对值
#define ROS_PROTOCOL_MAX_PWM 3300.000000f               // 定义底盘PWM换算使用的最大安全计数
#define ROS_PROTOCOL_LINEAR_TO_PWM 3000.000000f         // 待实车标定的线速度到PWM初始换算系数
#define ROS_PROTOCOL_ANGULAR_TO_PWM 450.000000f         // 待实车标定的角速度到左右差速PWM初始系数
#define ROS_PROTOCOL_IMU_ACCEL_X_SIGN 1.000000f         // 待安装确认的IMU X轴加速度方向符号
#define ROS_PROTOCOL_IMU_ACCEL_Y_SIGN 1.000000f         // 待安装确认的IMU Y轴加速度方向符号
#define ROS_PROTOCOL_IMU_ACCEL_Z_SIGN 1.000000f         // 待安装确认的IMU Z轴加速度方向符号
#define ROS_PROTOCOL_IMU_GYRO_X_SIGN 1.000000f          // 待安装确认的IMU X轴角速度方向符号
#define ROS_PROTOCOL_IMU_GYRO_Y_SIGN 1.000000f          // 待安装确认的IMU Y轴角速度方向符号
#define ROS_PROTOCOL_IMU_GYRO_Z_SIGN 1.000000f          // 待安装确认的IMU Z轴角速度方向符号

#endif                                                  // 结束ROS底盘配置头文件保护
