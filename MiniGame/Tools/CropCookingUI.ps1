# Crops/downscales exported cooking UI textures so regions can be measured by eye.
Add-Type -AssemblyName System.Drawing

$dir = "E:\UEProjects\MiniGame\MiniGame\Saved\Exported\CookingUI"

function Save-Crop {
    param(
        [string]$SourceName,
        [string]$OutName,
        [int]$X,
        [int]$Y,
        [int]$W,
        [int]$H,
        [int]$OutW
    )

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
    Write-Output "$OutName  ($OutW x $outH) from ${SourceName} rect ${X},${Y} ${W}x${H}"
}

Save-Crop -SourceName "T_CookingUI_Concept.png" -OutName "crop_concept_top.png" -X 0 -Y 0 -W 3146 -H 1500 -OutW 1100
Save-Crop -SourceName "T_CookingUI_Overlay_01.png" -OutName "crop_overlay01_top.png" -X 0 -Y 0 -W 3146 -H 1500 -OutW 1100
Save-Crop -SourceName "T_CookingUI_Overlay_02.png" -OutName "crop_overlay02_top.png" -X 0 -Y 0 -W 3146 -H 1500 -OutW 1100
Save-Crop -SourceName "T_CookingUI_Overlay_03.png" -OutName "crop_overlay03_top.png" -X 0 -Y 0 -W 3146 -H 1500 -OutW 1100
Save-Crop -SourceName "T_CookingUI_Overlay_04.png" -OutName "crop_overlay04_top.png" -X 0 -Y 0 -W 3146 -H 1500 -OutW 1100
