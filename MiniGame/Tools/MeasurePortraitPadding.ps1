# Report the transparent padding around each customer portrait PNG, as a fraction of height.

Add-Type -AssemblyName System.Drawing

$roots = @(
    "E:\UEProjects\MiniGame\ArtSubmit\Character\npc\normal_npc",
    "E:\UEProjects\MiniGame\ArtSubmit\Character\npc\nb_npc"
)

foreach ($root in $roots) {
    Get-ChildItem -Path $root -Filter *.png | Sort-Object Name | ForEach-Object {
        $bitmap = [System.Drawing.Bitmap]::FromFile($_.FullName)
        try {
            $w = $bitmap.Width
            $h = $bitmap.Height
            $bottom = -1
            for ($y = $h - 1; $y -ge 0 -and $bottom -lt 0; $y--) {
                for ($x = 0; $x -lt $w; $x += 2) {
                    if ($bitmap.GetPixel($x, $y).A -gt 8) { $bottom = $y; break }
                }
            }
            $top = -1
            for ($y = 0; $y -lt $h -and $top -lt 0; $y++) {
                for ($x = 0; $x -lt $w; $x += 2) {
                    if ($bitmap.GetPixel($x, $y).A -gt 8) { $top = $y; break }
                }
            }
            $padBottom = $h - 1 - $bottom
            "{0,-18} {1}x{2}  content rows {3}..{4}  bottom pad {5,4} ({6,5:P1})  top pad {7,4} ({8,5:P1})" -f `
                $_.Name, $w, $h, $top, $bottom, $padBottom, ($padBottom / $h), $top, ($top / $h)
        }
        finally {
            $bitmap.Dispose()
        }
    }
}
