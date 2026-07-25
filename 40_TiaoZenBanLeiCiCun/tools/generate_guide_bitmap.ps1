param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\TiaoZenBanLeiCiCunGuide.bmp')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$resolved = [System.IO.Path]::GetFullPath($OutputPath)
$bitmap = New-Object System.Drawing.Bitmap 340, 50
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([System.Drawing.Color]::White)

$plateFill = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(245, 142, 32))
$platePen = New-Object System.Drawing.Pen(
    [System.Drawing.Color]::FromArgb(45, 62, 75), 2)
$arrowPen = New-Object System.Drawing.Pen(
    [System.Drawing.Color]::FromArgb(0, 112, 150), 2)
$arrowPen.StartCap = [System.Drawing.Drawing2D.LineCap]::ArrowAnchor
$arrowPen.EndCap = [System.Drawing.Drawing2D.LineCap]::ArrowAnchor

$graphics.FillRectangle($plateFill, 90, 11, 160, 28)
$graphics.DrawRectangle($platePen, 90, 11, 160, 28)
$graphics.DrawLine($arrowPen, 15, 25, 88, 25)
$graphics.DrawLine($arrowPen, 252, 25, 325, 25)
$graphics.DrawLine($arrowPen, 170, 1, 170, 9)
$graphics.DrawLine($arrowPen, 170, 41, 170, 49)

$bitmap.Save($resolved, [System.Drawing.Imaging.ImageFormat]::Bmp)

$arrowPen.Dispose()
$platePen.Dispose()
$plateFill.Dispose()
$graphics.Dispose()
$bitmap.Dispose()
Write-Host "Generated $resolved"
