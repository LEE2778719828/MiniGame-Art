# High-zoom crops of the concept art so text anchors can be read off in texture pixels.
Add-Type -AssemblyName System.Drawing

$dir = "E:\UEProjects\MiniGame\MiniGame\Saved\Exported\CookingUI"

function Save-Crop {
    param([string]$SourceName, [string]$OutName, [int]$X, [int]$Y, [int]$W, [int]$H, [int]$OutW)

    $src = [System.Drawing.Image]::FromFile((Join-Path $dir $SourceName))
    $outH = [int][math]::Round($OutW * $H / $W)
    $bmp = New-Object System.Drawing.Bitmap $OutW, $outH
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $OutW, $outH), (New-Object System.Drawing.Rectangle $X, $Y, $W, $H), [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()
    $bmp.Save((Join-Path $dir $OutName), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $src.Dispose()
    Write-Output "$OutName ($OutW x $outH) <- $SourceName rect X=$X Y=$Y W=$W H=$H  scale=$([math]::Round($OutW / $W, 4))"
}

Save-Crop -SourceName "T_CookingUI_Concept.png" -OutName "zoom_coin.png" -X 300 -Y 150 -W 600 -H 300 -OutW 1200
Save-Crop -SourceName "T_CookingUI_Concept.png" -OutName "zoom_tally.png" -X 0 -Y 850 -W 450 -H 600 -OutW 900
Save-Crop -SourceName "T_CookingUI_Overlay_04.png" -OutName "zoom_overlay04_tally.png" -X 0 -Y 850 -W 450 -H 600 -OutW 900
Save-Crop -SourceName "T_CookingUI_Overlay_01.png" -OutName "zoom_overlay01_coin.png" -X 300 -Y 150 -W 600 -H 300 -OutW 1200
