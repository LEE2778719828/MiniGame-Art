# Measures whether a cookingUI page reaches the screen with the exact sRGB values the artist
# authored. Only points where a source layer is fully opaque are compared: there the composite
# equals that layer's pixel regardless of the space Slate blends in, so the check makes no
# assumption about gamma or premultiplication.
#
# A page rarely covers the camera frame exactly (the backdrop overfills by a few percent), so the
# page-to-frame scale and centre offset are searched around the caller's estimate. A good fit that
# lands near the analytically predicted scale is itself evidence the mapping is right.
param(
    [Parameter(Mandatory = $true)][string]$Capture,
    [Parameter(Mandatory = $true)][string[]]$Layers,
    [double]$Scale = 1.0,          # frame width / page width
    [int]$ProbeRow = 900,
    [int]$GridX = 40,
    [int]$GridY = 90,
    [int]$Tolerance = 2,
    [double]$MaskTop = 0.0,        # fraction of frame height to skip (HUD bands)
    [double]$MaskBottom = 0.0
)

Add-Type -AssemblyName System.Drawing

function Load-Flat([string]$Path, [int]$W, [int]$H) {
    if (-not (Test-Path $Path)) { throw "missing $Path" }
    $src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Path))
    $dst = New-Object System.Drawing.Bitmap($W, $H, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($dst)
    # Nearest neighbour keeps every sampled value an exact authored pixel.
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle(0, 0, $W, $H)))
    $g.Dispose(); $src.Dispose()

    $rect = New-Object System.Drawing.Rectangle(0, 0, $W, $H)
    $data = $dst.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bytes = New-Object byte[] ($data.Stride * $H)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
    $dst.UnlockBits($data); $dst.Dispose()
    return @{ bytes = $bytes; stride = $data.Stride; w = $W; h = $H }
}

$shotBmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Capture))

# The viewport is aspect-constrained, so the frame sits inside a black letterbox.
$x0 = -1; $x1 = -1
for ($x = 0; $x -lt $shotBmp.Width; $x++) {
    $p = $shotBmp.GetPixel($x, $ProbeRow)
    if ($p.R + $p.G + $p.B -gt 24) { if ($x0 -lt 0) { $x0 = $x }; $x1 = $x }
}
if ($x0 -lt 0) { throw "no frame found on row $ProbeRow" }
$frameW = $x1 - $x0 + 1
$frameH = $shotBmp.Height
Write-Output ("frame x=[{0}..{1}] w={2} h={3} aspect={4:N4}" -f $x0, $x1, $frameW, $frameH, ($frameW / $frameH))

$shot = @{}
$rect = New-Object System.Drawing.Rectangle(0, 0, $shotBmp.Width, $shotBmp.Height)
$data = $shotBmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$shotBytes = New-Object byte[] ($data.Stride * $shotBmp.Height)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $shotBytes, 0, $shotBytes.Length)
$shotStride = $data.Stride
$shotBmp.UnlockBits($data)
$shotW = $shotBmp.Width; $shotH = $shotBmp.Height
$shotBmp.Dispose()

$flats = @()
foreach ($layer in $Layers) { $flats += (Load-Flat $layer $frameW $frameH) }

function Score([double]$s, [double]$ox, [double]$oy) {
    $deltas = New-Object System.Collections.Generic.List[int]
    $within = 0
    for ($gy = 0; $gy -lt $GridY; $gy++) {
        $v = ($gy + 0.5) / $GridY
        if ($v -lt $MaskTop -or $v -gt (1.0 - $MaskBottom)) { continue }
        $sy = [int]($v * $frameH); if ($sy -ge $shotH) { continue }
        $tv = 0.5 + ($v - 0.5 - $oy) / $s
        if ($tv -lt 0 -or $tv -ge 1) { continue }
        $ty = [int]($tv * $frameH)
        for ($gx = 0; $gx -lt $GridX; $gx++) {
            $u = ($gx + 0.5) / $GridX
            $sx = $x0 + [int]($u * $frameW); if ($sx -ge $shotW) { continue }
            $tu = 0.5 + ($u - 0.5 - $ox) / $s
            if ($tu -lt 0 -or $tu -ge 1) { continue }
            $tx = [int]($tu * $frameW)

            $ref = $null
            for ($i = $flats.Count - 1; $i -ge 0; $i--) {
                $f = $flats[$i]
                $o = $ty * $f.stride + $tx * 4
                if ($f.bytes[$o + 3] -eq 255) { $ref = @($f.bytes[$o + 2], $f.bytes[$o + 1], $f.bytes[$o]); break }
            }
            if ($null -eq $ref) { continue }

            $so = $sy * $shotStride + $sx * 4
            $d = [Math]::Max([Math]::Abs($shotBytes[$so + 2] - $ref[0]),
                 [Math]::Max([Math]::Abs($shotBytes[$so + 1] - $ref[1]), [Math]::Abs($shotBytes[$so] - $ref[2])))
            $deltas.Add($d)
            if ($d -le $Tolerance) { $within++ }
        }
    }
    if ($deltas.Count -eq 0) { return @{ median = 999; n = 0; within = 0 } }
    $sorted = $deltas | Sort-Object
    return @{ median = $sorted[[int]($deltas.Count * 0.5)]; n = $deltas.Count; within = (100.0 * $within / $deltas.Count) }
}

$best = @{ median = 9999 }
$scaleCandidates = @()
for ($k = -10; $k -le 10; $k++) { $scaleCandidates += ($Scale + $k * 0.005) }
foreach ($s in $scaleCandidates) {
    $r = Score $s 0.0 0.0
    if ($r.median -lt $best.median) { $best = @{ median = $r.median; s = $s; ox = 0.0; oy = 0.0; n = $r.n; within = $r.within } }
}
foreach ($oy in -8..8 | ForEach-Object { $_ * 0.005 }) {
    $r = Score $best.s 0.0 $oy
    if ($r.median -lt $best.median) { $best = @{ median = $r.median; s = $best.s; ox = 0.0; oy = $oy; n = $r.n; within = $r.within } }
}
foreach ($ox in -8..8 | ForEach-Object { $_ * 0.005 }) {
    $r = Score $best.s $ox $best.oy
    if ($r.median -lt $best.median) { $best = @{ median = $r.median; s = $best.s; ox = $ox; oy = $best.oy; n = $r.n; within = $r.within } }
}

Write-Output ("best fit: scale={0:N3} (predicted {1:N3})  offset=({2:N3},{3:N3})" -f $best.s, $Scale, $best.ox, $best.oy)
Write-Output ("samples={0}  median delta={1}  within +/-{2} = {3:N1}%" -f $best.n, $best.median, $Tolerance, $best.within)
