# 2026-09-12 Windows 中文 Skill 校验问题

## 现象

首次执行：

```powershell
python C:\Users\Bee\.codex\skills\.system\skill-creator\scripts\quick_validate.py C:\Users\Bee\.codex\skills\xiaomifeng-robot-controller
```

脚本在读取中文 UTF-8 `SKILL.md` 时抛出 `UnicodeDecodeError: 'gbk' codec can't decode byte ...`。

## 失败尝试与边界

- 直接使用系统 `python` 失败。
- 这不是 YAML、frontmatter 或中文正文写错，不能通过把 Skill 转为 GBK 来规避，否则会破坏通用 UTF-8 约定。
- 未修改 `quick_validate.py`，避免污染系统技能工具。

## 根因

Windows 系统 Python 默认文本编码为 GBK；当前校验器内部使用 `Path.read_text()` 且未传入 `encoding='utf-8'`，因此按系统默认编码读取 UTF-8 中文文件失败。

## 正确处理

在当前 PowerShell 进程启用 Python UTF-8 模式后再运行：

```powershell
$env:PYTHONUTF8='1'
python C:\Users\Bee\.codex\skills\.system\skill-creator\scripts\quick_validate.py C:\Users\Bee\.codex\skills\xiaomifeng-robot-controller
```

随后校验器成功读取文件，并继续发现旧 Skill frontmatter 中的 `agent_created` 已不被当前规范支持。该字段没有运行功能，删除后再次校验，最终输出：

```text
Skill is valid!
```

## 可复用结论

- Windows 中文 Skill 保持 UTF-8 编码。
- 遇到 GBK 解码错误时优先用 `PYTHONUTF8=1` 运行校验，不改文件编码、不改系统校验器。
- 校验器继续报告的 frontmatter 问题需要独立处理，不能把所有错误都归因于编码。
- 只保留当前规范支持的 frontmatter 字段。

该结论已同步写入 `xiaomifeng-robot-controller` Skill。

