# Crops the newest Nexus capture so small on-screen readouts can be inspected.
param(
    [double]$Left = 0.0,
    [double]$Top = 0.0,
    [double]$Width = 1.0,
    [double]$Height = 1.0,
    [int]$OutW = 1000
)

Add-Type -AssemblyName System.Drawing

$captureDir = "E:\UEProjects\MiniGame\MiniGame\Saved\NexusCaptures"
$latest = Get-ChildItem $captureDir -Filter "NexusCapture_*.png" | Sort-Object LastWriteTime -Descending | Select-Object -First 1

$src = [System.Drawing.Image]::FromFile($latest.FullName)
$x = [int]($src.Width * $Left)
$y = [int]($src.Height * $Top)
$w = [int]($src.Width * $Width)
$h = [int]($src.Height * $Height)
$outH = [int][math]::Round($OutW * $h / $w)

$bmp = New-Object System.Drawing.Bitmap $OutW, $outH
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $OutW, $outH), (New-Object System.Drawing.Rectangle $x, $y, $w, $h), [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose()

$out = Join-Path $captureDir "crop_latest.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
$src.Dispose()

Write-Output "$($latest.Name) ($($src.Width)x$($src.Height)) -> $out ($OutW x $outH) rect $x,$y ${w}x${h}"
