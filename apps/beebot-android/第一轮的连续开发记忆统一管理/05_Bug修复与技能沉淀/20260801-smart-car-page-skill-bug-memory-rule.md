# Bug 修复与 Skill 记忆规则

- 时间：2026-08-01 Asia/Shanghai
- 目标：把 Bug 修复方法和持续沉淀规则固化到 `smart-car-page-development` Skill。
- 强制闭环：复现条件 → 根因 → 最小修复 → 回归验证 → 记忆与索引 → Skill 经验。
- 本次经验：控制模式字节 `0x5B/0x5C/0x5D` 与调速字节 `0x6A/0x6B` 互不门控。
- 当前规则：每次 Bug 修复后写清失败尝试、正确修复、验证证据和硬件边界；不得覆盖或删除历史失败记录。
- 验证边界：Skill 自动校验曾因 Python 缺少 `yaml` 模块未运行，Markdown/YAML 结构仅人工确认。
