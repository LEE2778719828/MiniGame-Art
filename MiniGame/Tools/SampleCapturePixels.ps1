# Print pixel colours at a grid of points in a capture, for checking frame edges and tints.

param(
    [Parameter(Mandatory = $true)][string]$Capture,
    [int[]]$Columns = @(710, 760, 1180, 1215),
    [int[]]$Rows = @(10, 60, 120, 200, 400, 600, 800, 1000, 1080, 1140)
)

Add-Type -AssemblyName System.Drawing

$root = "E:\UEProjects\MiniGame\MiniGame\Saved\NexusCaptures"
$path = if (Test-Path $Capture) { $Capture } else { Join-Path $root $Capture }

$bitmap = [System.Drawing.Bitmap]::FromFile($path)
try {
    foreach ($x in $Columns) {
        $line = "x {0,5}:" -f $x
        foreach ($y in $Rows) {
            $pixel = $bitmap.GetPixel($x, $y)
            $line += "  y{0,4}=({1,3},{2,3},{3,3})" -f $y, $pixel.R, $pixel.G, $pixel.B
        }
        $line
    }
}
finally {
    $bitmap.Dispose()
}
