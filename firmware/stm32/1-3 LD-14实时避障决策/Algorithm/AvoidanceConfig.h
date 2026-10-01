#ifndef __AVOIDANCE_CONFIG_H                     //防止避障参数头文件被重复包含
#define __AVOIDANCE_CONFIG_H                     //定义避障参数头文件包含保护宏

#define AVOIDANCE_BIN_COUNT 360U                 //定义一度分辨率的三百六十个极坐标栅格
#define AVOIDANCE_ANGLE_SCALE 100U               //定义输入角度采用一度的一百分之一为单位
#define AVOIDANCE_FULL_CIRCLE_HUNDREDTH 36000UL  //定义放大一百倍后的完整圆周角度
#define AVOIDANCE_FORWARD_ANGLE_DEG 0U           //保持已由实车确认正确的雷达左右角度坐标方向
#define AVOIDANCE_MIN_DISTANCE_CM 5U             //定义参与避障判断的最小有效距离为五厘米
#define AVOIDANCE_MAX_DISTANCE_CM 1200U          //定义参与避障判断的最大有效距离为一千二百厘米
#define AVOIDANCE_MIN_CONFIDENCE 20U             //定义接收测量点的最低置信度阈值
#define AVOIDANCE_UNKNOWN_DISTANCE_CM 65535U     //定义极坐标栅格没有有效距离时的占位值
#define AVOIDANCE_POINT_STALE_MS 500UL           //定义单个角度测量点超过五百毫秒即失效并保持障碍消失判断保守
#define AVOIDANCE_SENSOR_TIMEOUT_MS 300UL        //定义超过三百毫秒没有有效雷达帧即进入故障停车
#define AVOIDANCE_READY_MIN_BINS 120U            //定义至少一百二十个新鲜角度栅格后允许输出行驶指令
#define AVOIDANCE_FRONT_MIN_BINS 15U             //定义前方扇区至少需要十五个新鲜栅格
#define AVOIDANCE_FRONT_HALF_WIDTH_DEG 35U       //定义车头左右各三十五度为前方碰撞检测区
#define AVOIDANCE_SIDE_HALF_WIDTH_DEG 45U        //定义左右方向各九十度宽的可通行空间评估区
#define AVOIDANCE_REAR_HALF_WIDTH_DEG 35U        //定义车尾左右各三十五度为后方检测区
#define AVOIDANCE_EMERGENCY_DISTANCE_CM 25U      //定义二十五厘米以内立即输出紧急停车

#define AVOIDANCE_TURN_DISTANCE_CM 45U           //定义四十五厘米以内停止前进并执行转向避障
#define AVOIDANCE_SLOW_DISTANCE_CM 40U           //定义九十厘米以内按距离降低前进速度


#define AVOIDANCE_TURN_RELEASE_DISTANCE_CM 110U  //定义前方恢复到一百一十厘米后退出转向状态
#define AVOIDANCE_SIDE_BLOCK_DISTANCE_CM 30U     //定义侧向空间小于三十厘米时视为受阻
#define AVOIDANCE_ALERT_DISTANCE_CM 90U          //定义四向提示器检测到九十厘米内障碍物时开始报警
#define AVOIDANCE_LED_FAST_HALF_PERIOD_MS 100UL  //定义红绿状态灯快速闪烁的明暗半周期为一百毫秒
#define AVOIDANCE_CRUISE_SPEED_PERMILLE 600      //定义巡航前进速度指令为百分之六十
#define AVOIDANCE_MIN_SLOW_SPEED_PERMILLE 180    //定义减速区最低前进速度指令为百分之十八
#define AVOIDANCE_TURN_SPEED_PERMILLE 650        //定义原地避障旋转速度指令为百分之六十五
#define AVOIDANCE_SLOW_TURN_PERMILLE 180         //定义减速行驶时的小幅方向修正指令
#define AVOIDANCE_DECISION_PERIOD_MS 5UL         //定义避障状态机每五毫秒执行一次
#define AVOIDANCE_OLED_PERIOD_MS 100UL           //定义OLED避障信息每一百毫秒刷新一次
#define AVOIDANCE_SERIAL_PERIOD_MS 200UL         //定义USART2避障信息每二百毫秒打印一次

#endif                                           //结束避障参数头文件包含保护
