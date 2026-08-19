# Where the rendered frame starts and ends in a capture: scan one row and one column and
# report the runs of non-black pixels, plus the colour on each side of the transition.

param(
    [Parameter(Mandatory = $true)][string]$Capture,
    [int]$Row = -1,
    [int]$Column = -1,
    [int]$BlackThreshold = 12
)

Add-Type -AssemblyName System.Drawing

$root = "E:\UEProjects\MiniGame\MiniGame\Saved\NexusCaptures"
$path = if (Test-Path $Capture) { $Capture } else { Join-Path $root $Capture }

$bitmap = [System.Drawing.Bitmap]::FromFile($path)
try {
    if ($Row -lt 0) { $Row = [int]($bitmap.Height / 2) }
    if ($Column -lt 0) { $Column = [int]($bitmap.Width / 2) }

    function Describe($pixel) { "({0},{1},{2})" -f $pixel.R, $pixel.G, $pixel.B }

    $first = -1
    $last = -1
    for ($x = 0; $x -lt $bitmap.Width; $x++) {
        $pixel = $bitmap.GetPixel($x, $Row)
        if (($pixel.R + $pixel.G + $pixel.B) / 3 -gt $BlackThreshold) {
            if ($first -lt 0) { $first = $x }
            $last = $x
        }
    }
    "row {0}: lit x {1}..{2} ({3} px)  inside {4}  outside {5}" -f `
        $Row, $first, $last, ($last - $first + 1),
        (Describe $bitmap.GetPixel([int](($first + $last) / 2), $Row)),
        (Describe $bitmap.GetPixel([Math]::Max($first - 5, 0), $Row))

    $first = -1
    $last = -1
    for ($y = 0; $y -lt $bitmap.Height; $y++) {
        $pixel = $bitmap.GetPixel($Column, $y)
        if (($pixel.R + $pixel.G + $pixel.B) / 3 -gt $BlackThreshold) {
            if ($first -lt 0) { $first = $y }
            $last = $y
        }
    }
    "col {0}: lit y {1}..{2} ({3} px)  top {4}  bottom {5}" -f `
        $Column, $first, $last, ($last - $first + 1),
        (Describe $bitmap.GetPixel($Column, $first)),
        (Describe $bitmap.GetPixel($Column, $last))
}
finally {
    $bitmap.Dispose()
}
