# Compose 遥测 Freshness 短路不刷新

## 现象

MQTT 收到 22 字节遥测且解析器拆帧成功，但 HUD 长期显示 `--`。

## 失败尝试

- 按首字节明确区分二进制和 JSON。
- 将批量订阅改成逐主题订阅并检查异步 SUBACK。

这些改动提高了协议和订阅健壮性，但没有解决显示问题。诊断计数 `RX 52 / parsed 156` 证明每条 Payload 的三帧都已解析。

## 根因

Composable 首次执行时 freshness 为 false，`if (fresh) value else "--"` 的短路分支没有读取具体遥测 `mutableStateOf`；freshness 时间戳本身只是普通变量，不会触发重组。因此解析成功后页面仍停留在首次分支。

## 修复与验证

TelemetryStrip 显式读取 `parsedPacketCount` 这个 Compose state 作为 revision，再计算 freshness。真机流内发布非保留 22 字节测试遥测后，电量、电压、四轮有符号值、雷达和模式全部显示，`RX 43/129` 与三子帧结构一致。

## 可复用规则

任何用普通时间戳判断遥测新鲜度的 Compose 页面，都必须同时订阅一个随解析更新的 Compose state（revision/counter），或把 freshness 时间戳本身设计为 Compose state；不能依赖条件分支内的数据字段间接触发重组。
