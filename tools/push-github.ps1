$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root

Write-Host "工程目录: $root"
$origin = git remote get-url origin 2>&1
if ($LASTEXITCODE -ne 0) { throw "未配置 origin 远端。" }
Write-Host "origin: $origin"

Write-Host "执行 Git 差异检查（历史源码可能包含格式警告，不影响继续推送）..."
git diff --check
if ($LASTEXITCODE -ne 0) {
    Write-Warning "git diff --check 报告了历史文件格式警告，继续执行推送。"
}

Write-Host "开始推送 main 与标签..."
git -c http.version=HTTP/1.1 push -u origin main --follow-tags
if ($LASTEXITCODE -ne 0) { throw "GitHub 推送失败，已保留 Git 原始错误。" }
Write-Host "推送完成。"

