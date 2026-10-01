#include <stdint.h>                              //提供可移植的固定宽度整数类型
#include "Avoidance.h"                           //提供避障状态、接口与运动指令类型
#include "AvoidanceConfig.h"                     //提供可独立调整的避障距离和周期参数

static uint16_t AvoidanceDistanceCm[AVOIDANCE_BIN_COUNT];                                  //保存三百六十个方向的最近距离，单位厘米
static uint32_t AvoidanceUpdateMs[AVOIDANCE_BIN_COUNT];                                    //保存三百六十个方向最近一次更新的毫秒时间
static AvoidanceStatusTypeDef AvoidanceStatus;                                             //保存当前避障状态机对外发布的状态快照
static MotionCommandTypeDef AvoidanceCommand;                                              //保存当前避障状态机对外发布的运动指令
static uint32_t AvoidanceLastFrameMs = 0UL;                                                //保存最近一个完整有效雷达帧的接收时间
static uint8_t AvoidanceHasFrame = 0U;                                                     //标记是否曾经收到完整有效雷达帧

/**
  * 函    数：设置安全停车运动指令
  * 参    数：EmergencyStop 是否要求电机适配层执行紧急停车
  * 返 回 值：无
  */
static void Avoidance_SetStopCommand(uint8_t EmergencyStop)
{
    AvoidanceCommand.LinearXPermille = 0;                                                  //清除前后方向速度指令
    AvoidanceCommand.LinearYPermille = 0;                                                  //清除左右方向速度指令
    AvoidanceCommand.AngularZPermille = 0;                                                 //清除车体旋转速度指令
    AvoidanceCommand.EmergencyStop = EmergencyStop;                                        //保存普通停车或紧急停车要求
}

/**
  * 函    数：判断一个角度栅格是否仍然有效
  * 参    数：BinIndex 栅格下标，NowMs 当前毫秒时间
  * 返 回 值：1表示有效，0表示未知或已经超时
  */
static uint8_t Avoidance_IsBinFresh(uint16_t BinIndex, uint32_t NowMs)
{
    if (AvoidanceDistanceCm[BinIndex] == AVOIDANCE_UNKNOWN_DISTANCE_CM)                    //判断当前角度是否从未获得有效距离
    {
        return 0U;                                                                          //未知距离不能参与避障决策
    }
    if ((uint32_t)(NowMs - AvoidanceUpdateMs[BinIndex]) > AVOIDANCE_POINT_STALE_MS)         //判断当前角度距离是否已经超过有效时限
    {
        return 0U;                                                                          //过期距离不能代表当前环境
    }
    return 1U;                                                                              //返回当前角度距离仍然新鲜有效
}

/**
  * 函    数：统计当前全部新鲜角度栅格
  * 参    数：NowMs 当前毫秒时间
  * 返 回 值：仍然有效的角度栅格数量
  */
static uint16_t Avoidance_CountFreshBins(uint32_t NowMs)
{
    uint16_t BinIndex;                                                                      //保存当前检查的角度栅格下标
    uint16_t FreshCount = 0U;                                                               //累计仍然有效的角度栅格数量

    for (BinIndex = 0U; BinIndex < AVOIDANCE_BIN_COUNT; BinIndex++)                         //依次检查完整三百六十度距离图
    {
        if (Avoidance_IsBinFresh(BinIndex, NowMs) != 0U)                                    //判断当前角度距离是否仍然有效
        {
            FreshCount++;                                                                   //累计一个新鲜角度栅格
        }
    }
    return FreshCount;                                                                      //返回当前完整距离图的新鲜栅格数量
}

/**
  * 函    数：计算指定方向扇区的距离指标
  * 参    数：CenterDeg 中心角度，HalfWidthDeg 半宽，NowMs 当前时间，MinDistanceCm 最近距离，AverageDistanceCm 平均距离，ValidCount 有效栅格数
  * 返 回 值：无
  */
static void Avoidance_GetSectorMetrics(uint16_t CenterDeg, uint16_t HalfWidthDeg, uint32_t NowMs, uint16_t *MinDistanceCm, uint16_t *AverageDistanceCm, uint16_t *ValidCount)
{
    int16_t Offset;                                                                         //保存相对扇区中心的有符号角度偏移
    int16_t SignedIndex;                                                                    //保存完成圆周换算前的有符号栅格下标
    uint16_t BinIndex;                                                                      //保存归一化后的角度栅格下标
    uint16_t DistanceCm;                                                                    //保存当前角度栅格的厘米距离
    uint32_t DistanceSum = 0UL;                                                             //累计当前扇区全部有效距离

    *MinDistanceCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                         //默认当前扇区没有有效最近距离
    *AverageDistanceCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                     //默认当前扇区没有有效平均距离
    *ValidCount = 0U;                                                                       //清除当前扇区有效栅格计数
    for (Offset = -(int16_t)HalfWidthDeg; Offset <= (int16_t)HalfWidthDeg; Offset++)         //依次遍历扇区内每一个一度栅格
    {
        SignedIndex = (int16_t)CenterDeg + Offset;                                          //计算当前栅格相对车头零度的角度
        if (SignedIndex < 0)                                                                //判断当前角度是否小于零度
        {
            SignedIndex += (int16_t)AVOIDANCE_BIN_COUNT;                                    //将负角度环绕到圆周尾部
        }
        else if (SignedIndex >= (int16_t)AVOIDANCE_BIN_COUNT)                               //判断当前角度是否越过完整圆周
        {
            SignedIndex -= (int16_t)AVOIDANCE_BIN_COUNT;                                    //将超出角度环绕到圆周起点
        }
        BinIndex = (uint16_t)SignedIndex;                                                   //取得可用于访问距离图的无符号下标
        if (Avoidance_IsBinFresh(BinIndex, NowMs) != 0U)                                    //判断当前角度距离是否仍然有效
        {
            DistanceCm = AvoidanceDistanceCm[BinIndex];                                     //读取当前角度的厘米距离
            if (DistanceCm < *MinDistanceCm)                                                //判断当前距离是否小于已记录最近距离
            {
                *MinDistanceCm = DistanceCm;                                                //更新当前扇区最近障碍物距离
            }
            DistanceSum += DistanceCm;                                                      //把当前厘米距离加入平均值累计量
            (*ValidCount)++;                                                                //累计一个有效扇区栅格
        }
    }
    if (*ValidCount != 0U)                                                                  //判断当前扇区是否包含有效距离
    {
        *AverageDistanceCm = (uint16_t)(DistanceSum / *ValidCount);                          //计算当前扇区有效距离算术平均值
    }
}

/**
  * 函    数：选择左右避障方向
  * 参    数：LeftAverageCm 左侧平均距离，RightAverageCm 右侧平均距离
  * 返 回 值：正一表示左转，负一表示右转
  */
static int8_t Avoidance_SelectTurnDirection(uint16_t LeftAverageCm, uint16_t RightAverageCm)
{
    if (LeftAverageCm == AVOIDANCE_UNKNOWN_DISTANCE_CM)                                    //判断左侧是否缺少有效空间数据
    {
        return -1;                                                                          //左侧未知时优先选择数据有效的右侧
    }
    if (RightAverageCm == AVOIDANCE_UNKNOWN_DISTANCE_CM)                                   //判断右侧是否缺少有效空间数据
    {
        return 1;                                                                           //右侧未知时优先选择数据有效的左侧
    }
    if (LeftAverageCm >= RightAverageCm)                                                    //比较左右两侧的平均可通行距离
    {
        return 1;                                                                           //左侧空间更大或相同时选择左转
    }
    return -1;                                                                              //右侧空间更大时选择右转
}

/**
  * 函    数：初始化与硬件无关的避障决策核心
  * 参    数：无
  * 返 回 值：无
  */
void Avoidance_Init(void)
{
    uint16_t BinIndex;                                                                      //保存初始化过程中的角度栅格下标

    for (BinIndex = 0U; BinIndex < AVOIDANCE_BIN_COUNT; BinIndex++)                         //依次初始化完整三百六十度距离图
    {
        AvoidanceDistanceCm[BinIndex] = AVOIDANCE_UNKNOWN_DISTANCE_CM;                      //将当前角度距离标记为未知
        AvoidanceUpdateMs[BinIndex] = 0UL;                                                  //清除当前角度的更新时间
    }
    AvoidanceStatus.State = AVOIDANCE_STATE_WAIT;                                           //上电后先等待完整雷达环境数据
    AvoidanceStatus.FrontDistanceCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                        //将前方最近距离初始化为未知
    AvoidanceStatus.LeftDistanceCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                         //将左侧最近距离初始化为未知
    AvoidanceStatus.RightDistanceCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                        //将右侧最近距离初始化为未知
    AvoidanceStatus.RearDistanceCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                         //将后方最近距离初始化为未知
    AvoidanceStatus.FreshBinCount = 0U;                                                     //清除新鲜角度栅格数量
    AvoidanceStatus.DataAgeMs = 0UL;                                                        //清除雷达数据年龄
    AvoidanceLastFrameMs = 0UL;                                                             //清除最近有效帧时间
    AvoidanceHasFrame = 0U;                                                                //标记尚未收到完整有效雷达帧
    Avoidance_SetStopCommand(1U);                                                           //等待雷达数据期间保持紧急停车输出
}

/**
  * 函    数：输入一个雷达测量点
  * 参    数：AngleHundredth 放大一百倍的角度，DistanceCm 厘米距离，Confidence 置信度，NowMs 当前毫秒时间
  * 返 回 值：无
  */
void Avoidance_InputPoint(uint16_t AngleHundredth, uint16_t DistanceCm, uint8_t Confidence, uint32_t NowMs)
{
    uint32_t NormalizedAngle;                                                               //保存完成安装角补偿后的角度
    uint16_t BinIndex;                                                                      //保存当前测量点对应的一度栅格下标

    if (DistanceCm == 0U)                                                                   //判断雷达是否使用零距离表示当前方向没有目标回波
    {
        DistanceCm = AVOIDANCE_MAX_DISTANCE_CM;                                             //把无目标回波记录为配置的最大可通行距离
    }
    else if (DistanceCm < AVOIDANCE_MIN_DISTANCE_CM || DistanceCm > AVOIDANCE_MAX_DISTANCE_CM) //判断非零厘米距离是否处于可用量程范围
    {
        return;                                                                             //丢弃小于最小量程或超出最大量程的异常测量点
    }
    if (Confidence < AVOIDANCE_MIN_CONFIDENCE)                                              //判断测量点置信度是否达到最低要求
    {
        return;                                                                             //丢弃低置信度噪声测量点
    }
    NormalizedAngle = AngleHundredth;                                                       //读取雷达输出的放大一百倍角度
    NormalizedAngle += AVOIDANCE_FULL_CIRCLE_HUNDREDTH;                                     //增加一圈角度以便安全执行安装角补偿
    NormalizedAngle -= (uint32_t)AVOIDANCE_FORWARD_ANGLE_DEG * AVOIDANCE_ANGLE_SCALE;        //把雷达安装方向旋转到车头零度坐标系
    NormalizedAngle %= AVOIDANCE_FULL_CIRCLE_HUNDREDTH;                                     //将补偿后的角度归一化到完整圆周内
    BinIndex = (uint16_t)((NormalizedAngle + 50UL) / AVOIDANCE_ANGLE_SCALE);                 //将角度四舍五入映射到最近的一度栅格
    if (BinIndex >= AVOIDANCE_BIN_COUNT)                                                    //判断四舍五入结果是否恰好到达三百六十度
    {
        BinIndex = 0U;                                                                       //把三百六十度环绕到零度车头方向
    }
    AvoidanceDistanceCm[BinIndex] = DistanceCm;                                             //保存当前角度最新厘米距离
    AvoidanceUpdateMs[BinIndex] = NowMs;                                                    //记录当前角度距离的更新时间
}

/**
  * 函    数：通知收到完整有效雷达帧
  * 参    数：NowMs 当前毫秒时间
  * 返 回 值：无
  */
void Avoidance_NotifyFrame(uint32_t NowMs)
{
    AvoidanceLastFrameMs = NowMs;                                                           //记录最近完整有效雷达帧的接收时间
    AvoidanceHasFrame = 1U;                                                                 //标记雷达通信链路已经产生有效帧
}

/**
  * 函    数：更新实时避障状态与标准化运动指令
  * 参    数：NowMs 当前毫秒时间
  * 返 回 值：无
  */
void Avoidance_Update(uint32_t NowMs)
{
    uint16_t FrontAverageCm;                                                                //保存前方扇区平均距离
    uint16_t LeftAverageCm;                                                                 //保存左侧扇区平均距离
    uint16_t RightAverageCm;                                                                //保存右侧扇区平均距离
    uint16_t RearAverageCm;                                                                 //保存后方扇区平均距离
    uint16_t FrontValidCount;                                                               //保存前方扇区有效栅格数量
    uint16_t LeftValidCount;                                                                //保存左侧扇区有效栅格数量
    uint16_t RightValidCount;                                                               //保存右侧扇区有效栅格数量
    uint16_t RearValidCount;                                                                //保存后方扇区有效栅格数量
    int8_t TurnDirection;                                                                   //保存左转或右转的避障方向
    int32_t SlowSpeed;                                                                      //保存根据前方距离线性计算的减速指令

    AvoidanceStatus.FreshBinCount = Avoidance_CountFreshBins(NowMs);                        //统计当前完整距离图的新鲜角度数量
    if (AvoidanceHasFrame == 0U)                                                            //判断系统是否尚未收到任何有效雷达帧
    {
        AvoidanceStatus.State = AVOIDANCE_STATE_WAIT;                                       //保持等待雷达数据状态
        AvoidanceStatus.DataAgeMs = 0UL;                                                    //尚无有效帧时不计算无意义的数据年龄
        Avoidance_SetStopCommand(1U);                                                       //等待期间要求电机适配层保持停车
        return;                                                                             //结束本次没有雷达数据的避障更新
    }
    AvoidanceStatus.DataAgeMs = (uint32_t)(NowMs - AvoidanceLastFrameMs);                   //计算最近完整雷达帧的数据年龄
    if (AvoidanceStatus.DataAgeMs > AVOIDANCE_SENSOR_TIMEOUT_MS)                            //判断雷达链路是否已经超过安全时限
    {
        AvoidanceStatus.State = AVOIDANCE_STATE_SENSOR_FAULT;                               //进入雷达数据超时故障状态
        Avoidance_SetStopCommand(1U);                                                       //雷达失联时要求电机立即停车
        return;                                                                             //结束本次故障状态更新
    }

    Avoidance_GetSectorMetrics(180U, AVOIDANCE_FRONT_HALF_WIDTH_DEG, NowMs, &AvoidanceStatus.FrontDistanceCm, &FrontAverageCm, &FrontValidCount); //按实车验证结果读取一百八十度前方障碍物距离
    Avoidance_GetSectorMetrics(90U, AVOIDANCE_SIDE_HALF_WIDTH_DEG, NowMs, &AvoidanceStatus.LeftDistanceCm, &LeftAverageCm, &LeftValidCount); //计算左侧可通行空间
    Avoidance_GetSectorMetrics(270U, AVOIDANCE_SIDE_HALF_WIDTH_DEG, NowMs, &AvoidanceStatus.RightDistanceCm, &RightAverageCm, &RightValidCount); //计算右侧可通行空间
    Avoidance_GetSectorMetrics(0U, AVOIDANCE_REAR_HALF_WIDTH_DEG, NowMs, &AvoidanceStatus.RearDistanceCm, &RearAverageCm, &RearValidCount); //按实车验证结果读取零度后方障碍物距离
    (void)RearAverageCm;                                                                     //保留后方平均空间供后续倒车决策扩展
    (void)LeftValidCount;                                                                    //保留左侧有效数量供后续高级通道评分扩展
    (void)RightValidCount;                                                                   //保留右侧有效数量供后续高级通道评分扩展
    (void)RearValidCount;                                                                    //保留后方有效数量供后续倒车脱困扩展

    if (AvoidanceStatus.FreshBinCount < AVOIDANCE_READY_MIN_BINS || FrontValidCount < AVOIDANCE_FRONT_MIN_BINS) //判断环境数据是否达到启动条件
    {
        AvoidanceStatus.State = AVOIDANCE_STATE_WAIT;                                       //环境数据不足时保持等待状态
        Avoidance_SetStopCommand(1U);                                                       //禁止在环境感知不完整时输出行驶指令
        return;                                                                             //结束本次环境未就绪的避障更新
    }
    if (AvoidanceStatus.FrontDistanceCm <= AVOIDANCE_EMERGENCY_DISTANCE_CM)                 //判断前方障碍物是否进入紧急停车距离
    {
        AvoidanceStatus.State = AVOIDANCE_STATE_EMERGENCY_STOP;                             //进入障碍物过近紧急停车状态
        Avoidance_SetStopCommand(1U);                                                       //要求后续电机适配层立即切断运动输出
        return;                                                                             //结束本次紧急停车状态更新
    }
    if ((AvoidanceStatus.State == AVOIDANCE_STATE_TURN_LEFT || AvoidanceStatus.State == AVOIDANCE_STATE_TURN_RIGHT) && AvoidanceStatus.FrontDistanceCm < AVOIDANCE_TURN_RELEASE_DISTANCE_CM) //判断转向避障是否尚未获得足够前方空间
    {
        AvoidanceCommand.LinearXPermille = 0;                                               //转向释放前保持停止前进
        AvoidanceCommand.LinearYPermille = 0;                                               //当前通用底盘接口不输出横移指令
        AvoidanceCommand.AngularZPermille = AvoidanceStatus.State == AVOIDANCE_STATE_TURN_LEFT ? AVOIDANCE_TURN_SPEED_PERMILLE : -AVOIDANCE_TURN_SPEED_PERMILLE; //保持原转向避免左右抖动
        AvoidanceCommand.EmergencyStop = 0U;                                                //转向避障属于受控运动而不是紧急停车
        return;                                                                             //保持当前转向状态直到前方空间恢复
    }
    if (AvoidanceStatus.FrontDistanceCm <= AVOIDANCE_TURN_DISTANCE_CM)                      //判断前方距离是否需要停止前进并转向
    {
        TurnDirection = Avoidance_SelectTurnDirection(LeftAverageCm, RightAverageCm);        //选择平均空间更大的避障方向
        AvoidanceCommand.LinearXPermille = 0;                                               //转向避障阶段停止前进
        AvoidanceCommand.LinearYPermille = 0;                                               //当前算法不主动输出横移运动
        AvoidanceCommand.AngularZPermille = (int16_t)(TurnDirection * AVOIDANCE_TURN_SPEED_PERMILLE); //输出标准化左转或右转指令
        AvoidanceCommand.EmergencyStop = 0U;                                                //当前距离允许执行受控原地转向
        if (AvoidanceStatus.LeftDistanceCm <= AVOIDANCE_SIDE_BLOCK_DISTANCE_CM && AvoidanceStatus.RightDistanceCm <= AVOIDANCE_SIDE_BLOCK_DISTANCE_CM) //判断左右两侧是否同时受阻
        {
            AvoidanceStatus.State = AVOIDANCE_STATE_ESCAPE;                                 //进入选择较大空间原地旋转的脱困状态
        }
        else if (TurnDirection > 0)                                                         //判断通道评分是否选择左侧
        {
            AvoidanceStatus.State = AVOIDANCE_STATE_TURN_LEFT;                              //记录当前正在执行左转避障
        }
        else                                                                                //处理通道评分选择右侧的情况
        {
            AvoidanceStatus.State = AVOIDANCE_STATE_TURN_RIGHT;                             //记录当前正在执行右转避障
        }
        return;                                                                             //结束本次转向避障状态更新
    }
    if (AvoidanceStatus.FrontDistanceCm <= AVOIDANCE_SLOW_DISTANCE_CM)                      //判断前方距离是否进入线性减速区域
    {
        SlowSpeed = AVOIDANCE_MIN_SLOW_SPEED_PERMILLE;                                      //从减速区域最低前进速度开始计算
        SlowSpeed += ((int32_t)AvoidanceStatus.FrontDistanceCm - AVOIDANCE_TURN_DISTANCE_CM) * (AVOIDANCE_CRUISE_SPEED_PERMILLE - AVOIDANCE_MIN_SLOW_SPEED_PERMILLE) / (AVOIDANCE_SLOW_DISTANCE_CM - AVOIDANCE_TURN_DISTANCE_CM); //按剩余距离线性增加前进速度
        AvoidanceStatus.State = AVOIDANCE_STATE_SLOW;                                       //记录当前处于接近障碍物减速状态
        AvoidanceCommand.LinearXPermille = (int16_t)SlowSpeed;                              //输出计算得到的标准化减速前进指令
        AvoidanceCommand.LinearYPermille = 0;                                               //当前算法不主动输出横移运动
        if (LeftAverageCm > RightAverageCm)                                                 //判断左侧平均空间是否更大
        {
            AvoidanceCommand.AngularZPermille = AVOIDANCE_SLOW_TURN_PERMILLE;               //减速前进同时轻微向左侧空闲区修正
        }
        else if (RightAverageCm > LeftAverageCm)                                            //判断右侧平均空间是否更大
        {
            AvoidanceCommand.AngularZPermille = -AVOIDANCE_SLOW_TURN_PERMILLE;              //减速前进同时轻微向右侧空闲区修正
        }
        else                                                                                //处理左右平均空间相同的情况
        {
            AvoidanceCommand.AngularZPermille = 0;                                          //左右空间相同时保持当前车头方向
        }
        AvoidanceCommand.EmergencyStop = 0U;                                                //减速行驶不触发紧急停车标志
        return;                                                                             //结束本次减速避障状态更新
    }

    AvoidanceStatus.State = AVOIDANCE_STATE_CRUISE;                                         //前方安全时进入正常巡航状态
    AvoidanceCommand.LinearXPermille = AVOIDANCE_CRUISE_SPEED_PERMILLE;                     //输出百分之六十标准化前进速度
    AvoidanceCommand.LinearYPermille = 0;                                                   //普通四驱差速底盘不使用横移速度
    AvoidanceCommand.AngularZPermille = 0;                                                  //前方安全时不主动改变车头方向
    AvoidanceCommand.EmergencyStop = 0U;                                                    //清除紧急停车标志并允许后续电机适配层运动
}

/**
  * 函    数：复制当前避障状态与运动指令
  * 参    数：Status 状态输出地址，Command 运动指令输出地址
  * 返 回 值：无
  */
void Avoidance_GetSnapshot(AvoidanceStatusTypeDef *Status, MotionCommandTypeDef *Command)
{
    *Status = AvoidanceStatus;                                                               //复制完整避障状态快照给调用模块
    *Command = AvoidanceCommand;                                                             //复制标准化运动指令给调用模块
}

/**
  * 函    数：取得避障状态短名称
  * 参    数：State 当前避障状态
  * 返 回 值：适合OLED和USART2显示的只读英文短名称
  */
const char *Avoidance_GetStateText(AvoidanceStateTypeDef State)
{
    switch (State)                                                                          //根据避障状态选择固定显示文本
    {
        case AVOIDANCE_STATE_WAIT: return "WAIT";                                           //返回等待环境数据状态名称
        case AVOIDANCE_STATE_CRUISE: return "GO";                                           //返回正常巡航状态名称
        case AVOIDANCE_STATE_SLOW: return "SLOW";                                           //返回接近障碍物减速状态名称
        case AVOIDANCE_STATE_TURN_LEFT: return "LEFT";                                      //返回向左转向避障状态名称
        case AVOIDANCE_STATE_TURN_RIGHT: return "RIGHT";                                    //返回向右转向避障状态名称
        case AVOIDANCE_STATE_ESCAPE: return "ESC";                                          //返回左右受阻脱困状态名称
        case AVOIDANCE_STATE_EMERGENCY_STOP: return "ESTOP";                                //返回障碍物过近紧急停车状态名称
        case AVOIDANCE_STATE_SENSOR_FAULT: return "FAULT";                                  //返回雷达数据超时故障状态名称
        default: return "UNKNOWN";                                                          //返回未知状态保护文本
    }
}
