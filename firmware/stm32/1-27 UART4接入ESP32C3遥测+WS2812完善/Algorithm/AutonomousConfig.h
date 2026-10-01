#ifndef __AUTONOMOUS_CONFIG_H__                    // 防止自主导航参数头文件被重复包含
#define __AUTONOMOUS_CONFIG_H__                    // 定义自主导航参数头文件保护宏

#define AUTONOMOUS_FRONT_TRIGGER_CM 70U            // 前方小于该距离时开始自主选侧避障，单位厘米
#define AUTONOMOUS_FRONT_RELEASE_CM 100U           // 前方恢复到该距离后释放绕行状态，单位厘米
#define AUTONOMOUS_SIDE_CLEAR_CM 55U               // 左右侧达到该距离才允许选择为绕行方向，单位厘米
#define AUTONOMOUS_EMERGENCY_CM 25U                // 前方小于该距离时进入紧急原地转向，单位厘米
#define AUTONOMOUS_CLEAR_MARGIN_CM 12U             // 左右距离选择的最小优势差值，单位厘米
#define AUTONOMOUS_SIDE_PWM_RATIO 2U               // 自主弧线内侧轮速度为外侧轮速度的倒数比例
#define AUTONOMOUS_SPEED_MISMATCH_CM 30U           // 预留电机编码速度差异保护阈值，单位编码增量
#define AUTONOMOUS_SPEED_MISMATCH_COUNT 3U         // 连续多少次速度差异超阈值后停车保护
#define AUTONOMOUS_PITCH_LIMIT_DEG10 200           // 预留俯仰角保护阈值，单位0.1度
#define AUTONOMOUS_ROLL_LIMIT_DEG10 200            // 预留横滚角保护阈值，单位0.1度
#define AUTONOMOUS_LOW_BATTERY_PERCENT 10U        // 电量低于该百分比时进入停车保护
#define AUTONOMOUS_DECISION_PERIOD_MS 5UL         // 自主导航决策周期，单位毫秒

#endif                                           // 结束自主导航参数头文件保护
