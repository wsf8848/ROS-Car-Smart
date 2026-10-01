# 总自动验收ROS环境与PowerShell编码兼容

- 日期：2026-09-28
- 状态：已闭环

## 现象

1. 板端直接用`python3 test_patrol_contract.py`执行巡逻契约测试时，导入网关报`ModuleNotFoundError: rclpy`。
2. Windows PowerShell 5执行无BOM UTF-8的`.ps1`时，中文`throw`字符串被错误代码页解码，导致引号和语句块解析失败。
3. 远端验收曾返回非零，但PowerShell包装脚本未检查`$LASTEXITCODE`，外层仍显示成功。

## 根因

- ROS2 Python包依赖`/opt/ros/humble/setup.bash`提供运行环境，普通登录Shell或非ROS Python进程不能假定已经具备`rclpy`路径。
- Windows PowerShell 5对无BOM UTF-8脚本的默认编码兼容性不足；中文多字节内容可能被按ANSI代码页误解。
- `$ErrorActionPreference='Stop'`不会自动把原生程序非零退出码转换为PowerShell终止错误。

## 修复

- 总验收使用`/bin/bash -lc`，先source ROS2 Humble环境，再以固定参数运行巡逻契约脚本。
- Windows包装器的报错文本保持纯ASCII，避免依赖BOM和主机代码页。
- 每个`python/node/ssh/scp`调用后显式检查`$LASTEXITCODE`并抛出错误。

## 验证

- Windows一键入口完整执行成功。
- 板端巡逻契约输出`{"ok": true, "checks": "patrol_contract_and_fail_closed"}`。
- 最终总验收`baseline_pass=true`、`failure_count=0`、`warning_count=0`。

## 可复用规则

- 板端自动化中任何导入`rclpy`或ROS消息的测试都必须显式加载目标ROS发行版环境。
- 面向Windows PowerShell 5的无BOM脚本，优先保持源码ASCII；如必须包含中文，应显式以UTF-8 BOM交付并做Windows PowerShell 5实测。
- PowerShell调用原生程序必须检查`$LASTEXITCODE`，不能只依赖`$ErrorActionPreference`。
- 地图文件或元数据变化后，旧恢复报告立即视为过期，必须重新执行隔离恢复演练后再跑总验收。
