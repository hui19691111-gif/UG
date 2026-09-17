$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# Keep the generated transparent master intact. Export the existing NX bitmap
# format and background key, plus transparent PNGs for inspection and reuse.
$master = [Drawing.Bitmap]::new((Join-Path $PSScriptRoot 'StandardPartsLibrary.master.png'))
try {
    foreach ($size in @(32, 256)) {
        $png = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [Drawing.Graphics]::FromImage($png)
        try {
            $graphics.Clear([Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.DrawImage($master, [Drawing.Rectangle]::new(0, 0, $size, $size))
            $png.Save((Join-Path $PSScriptRoot "StandardPartsLibrary.$size.png"), [Drawing.Imaging.ImageFormat]::Png)
            if ($size -eq 32) {
                $bitmap = [Drawing.Bitmap]::new(32, 32, [Drawing.Imaging.PixelFormat]::Format32bppRgb)
                $bitmapGraphics = [Drawing.Graphics]::FromImage($bitmap)
                try {
                    $bitmapGraphics.Clear([Drawing.Color]::FromArgb(209, 209, 209))
                    $bitmapGraphics.DrawImageUnscaled($png, 0, 0)
                    $bitmap.Save((Join-Path $PSScriptRoot '..\..\StandardPartsLibrary.bmp'), [Drawing.Imaging.ImageFormat]::Bmp)
                } finally {
                    $bitmapGraphics.Dispose()
                    $bitmap.Dispose()
                }
            }
        } finally {
            $graphics.Dispose()
            $png.Dispose()
        }
    }
} finally {
    $master.Dispose()
}
