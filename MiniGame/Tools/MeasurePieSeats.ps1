# Measure where the portraits and the stall edge land in a PIE capture, in camera-space
# world units, so the sprite height can be solved instead of eyeballed.
param(
    [Parameter(Mandatory = $true)][string]$Capture,
    [double]$OrthoWidth = 1417.2,
    [double]$Aspect = 0.45,
    # Camera-space right offsets of the seats, screen-right positive.
    [double[]]$SeatRight = @(-210.0, 215.7),
    # Skip the HUD text band at the top of the window.
    [int]$MinY = 300,
    [int]$Bright = 330,
    [int]$MinRun = 10
)

Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Capture))
$frameW = $img.Height * $Aspect
$unitsPerPixel = $OrthoWidth / $frameW
$centreX = $img.Width / 2.0
$centreY = $img.Height / 2.0
"capture {0}x{1}  frame width {2:N1}px  {3:N4} world/px" -f $img.Width, $img.Height, $frameW, $unitsPerPixel

function TopEdge($x0, $x1) {
    for ($y = $MinY; $y -lt $img.Height; $y += 1) {
        $run = 0
        for ($x = [int]$x0; $x -le [int]$x1; $x += 2) {
            $p = $img.GetPixel($x, $y)
            if (($p.R + $p.G + $p.B) -gt $Bright) { $run++ } 
        }
        if ($run -ge $MinRun) { return $y }
    }
    return -1
}

function ToUp($y) { return ($centreY - $y) * $unitsPerPixel }

# Stall edge: sample a band clear of the portraits.
$edgeY = TopEdge ($centreX - 330) ($centreX - 250)
$edgeUp = ToUp $edgeY
"stall top edge   y={0}px  up={1:N1}" -f $edgeY, $edgeUp

foreach ($right in $SeatRight) {
    $x = $centreX + $right / $unitsPerPixel
    $topY = TopEdge ($x - 70) ($x + 70)
    "portrait right={0,7:N1}  x={1,7:N0}px  top y={2}px  up={3:N1}  visible above edge={4:N1}" -f `
        $right, $x, $topY, (ToUp $topY), ((ToUp $topY) - $edgeUp)
}

$img.Dispose()
