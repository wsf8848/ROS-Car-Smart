param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$AssetVersion = '20260929-timesync1'
)

$ErrorActionPreference = 'Stop'
$frontend = Join-Path $ProjectRoot 'frontend'
$fragmentPath = Join-Path $frontend 'index-fragment.html'
$indexPath = Join-Path $frontend 'index.html'
$fragment = Get-Content -LiteralPath $fragmentPath -Raw -Encoding UTF8
$fragment = $fragment -replace '^<link rel="stylesheet" href="/live\.css(?:\?v=[^"]+)?">\s*', ''
$fragment = $fragment -replace '\s*<script src="/live\.js(?:\?v=[^"]+)?"></script>\s*$', ''
$document = @"
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="color-scheme" content="dark">
  <meta name="theme-color" content="#0b1118">
  <title>Bee ROS Web Console</title>
  <link rel="icon" type="image/svg+xml" href="/bee-ros-console.svg?v=$AssetVersion">
  <link rel="stylesheet" href="/live.css?v=$AssetVersion">
  <script src="/vendor/lucide.min.js"></script>
</head>
<body style="margin:0;background:#0b1118">
$fragment
<script src="/live.js?v=$AssetVersion"></script>
</body>
</html>
"@
[System.IO.File]::WriteAllText($indexPath, $document, [System.Text.UTF8Encoding]::new($false))
Write-Output "Generated $indexPath"
