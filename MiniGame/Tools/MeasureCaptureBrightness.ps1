# Mean and peak brightness of the board area in day frame captures, for exposure checks.

param(
    [string[]]$Captures,
    [int]$Left = 420,
    [int]$Right = 620,
    [int]$Top = 200,
    [int]$Bottom = 500
)

Add-Type -AssemblyName System.Drawing

$root = "E:\UEProjects\MiniGame\MiniGame\Saved\NexusCaptures"

foreach ($name in $Captures) {
    $path = if (Test-Path $name) { $name } else { Join-Path $root $name }
    $bitmap = [System.Drawing.Bitmap]::FromFile($path)
    try {
        $sum = 0.0
        $count = 0
        $max = 0
        for ($y = $Top; $y -lt $Bottom; $y += 3) {
            for ($x = $Left; $x -lt $Right; $x += 3) {
                $pixel = $bitmap.GetPixel($x, $y)
                $luma = ($pixel.R + $pixel.G + $pixel.B) / 3.0
                $sum += $luma
                $count++
                if ($luma -gt $max) { $max = $luma }
            }
        }
        "{0,-42} mean {1,6:N1}  peak {2,4:N0}" -f (Split-Path $path -Leaf), ($sum / $count), $max
    }
    finally {
        $bitmap.Dispose()
    }
}
