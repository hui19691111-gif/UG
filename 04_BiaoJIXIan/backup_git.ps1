$ErrorActionPreference = "Stop"

$git = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe"

if (-not (Test-Path $git)) {
    Write-Host "未找到 Git：" $git -ForegroundColor Red
    exit 1
}

$repo = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repo

$message = Read-Host "请输入本次版本说明"
if ([string]::IsNullOrWhiteSpace($message)) {
    $message = "更新于 $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
}

& $git add .

$status = & $git status --short
if ([string]::IsNullOrWhiteSpace(($status | Out-String))) {
    Write-Host "没有可提交的改动。" -ForegroundColor Yellow
    exit 0
}

& $git commit -m $message

Write-Host ""
Write-Host "提交完成。" -ForegroundColor Green
& $git log -1 --oneline
