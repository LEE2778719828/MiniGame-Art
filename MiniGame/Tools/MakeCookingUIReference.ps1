# Flattens the cookingUI source layers over black at frame resolution, so the PIE capture can be
# eyeballed and diffed against the art exactly as authored.
param(
    [Parameter(Mandatory = $true)][string[]]$Layers,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$Width = 825,
    [int]$Height = 1833
)

Add-Type -AssemblyName System.Drawing

$canvas = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($canvas)
$g.Clear([System.Drawing.Color]::Black)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver

foreach ($layer in $Layers) {
    if (-not (Test-Path $layer)) { throw "missing $layer" }
    $bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $layer))
    $g.DrawImage($bmp, (New-Object System.Drawing.Rectangle(0, 0, $Width, $Height)))
    $bmp.Dispose()
}

$g.Dispose()
$canvas.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$canvas.Dispose()
Write-Output ("wrote " + $Out)
