# ============================================================
#  MiniGame Editor 涓€閿噸缂栬剼鏈?#  娴佺▼锛氭彁閱掍繚瀛?-> 纭 -> 鍏?UE -> 缂栬瘧 -> 閲嶆柊鎵撳紑缂栬緫鍣?#  浣滆€咃細榛勬瘺馃惡 for 灏廗
# ============================================================

[CmdletBinding()]
param(
    [switch]$Yes,
    [switch]$NoLaunch,
    [switch]$ForceKill
)

$ErrorActionPreference = 'Stop'
$OutputEncoding = [System.Text.Encoding]::UTF8
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}

# ---- 鍥哄畾璺緞锛堟湰鏈虹幆澧冿級 ----
$EngineRoot   = 'D:\UE_5.8'
$BuildBat     = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EditorExe    = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$ProjectPath  = 'D:\MiniGame-Art\MiniGame\MiniGame.uproject'
$Target       = 'MiniGameEditor'
$Platform     = 'Win64'
$Config       = 'Development'

function Write-Step ($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Ok   ($msg) { Write-Host "[OK] $msg"  -ForegroundColor Green }
function Write-Warn2($msg) { Write-Host "[!]  $msg"  -ForegroundColor Yellow }
function Write-Err  ($msg) { Write-Host "[X]  $msg"  -ForegroundColor Red }

Write-Step "棰勬鐜"
foreach ($p in @($BuildBat, $EditorExe, $ProjectPath)) {
    if (-not (Test-Path $p)) { Write-Err "缂哄け鏂囦欢锛?p"; exit 1 }
}
Write-Ok "寮曟搸锛?EngineRoot"
Write-Ok "椤圭洰锛?ProjectPath"

Write-Step "绗?1 姝?/ 4锛氳鍏堝湪 UE 缂栬緫鍣ㄩ噷 File -> Save All (Ctrl+Shift+S) 淇濆瓨鎵€鏈夎祫浜т笌钃濆浘"
if (-not $Yes) {
    $ans = Read-Host "宸蹭繚瀛樺畬姣曚簡鍚楋紵(Y=缁х画 / N=鍙栨秷)"
    if ($ans -notmatch '^[Yy]') { Write-Warn2 "宸插彇娑堛€傝淇濆瓨鍚庡啀杩愯銆?; exit 0 }
}
Write-Ok "纭宸蹭繚瀛?

Write-Step "绗?2 姝?/ 4锛氬叧闂?UE 缂栬緫鍣ㄨ繘绋?
$targets = @('UnrealEditor', 'UnrealEditor-Cmd', 'CrashReportClient')
$running = Get-Process -Name $targets -ErrorAction SilentlyContinue
if ($running) {
    Write-Warn2 "妫€娴嬪埌浠ヤ笅 UE 杩涚▼姝ｅ湪杩愯锛?
    $running | Select-Object Id, ProcessName, StartTime | Format-Table -AutoSize | Out-Host
    if (-not $ForceKill -and -not $Yes) {
        $ans = Read-Host "鏄惁寮哄埗缁撴潫杩欎簺杩涚▼锛?Y=缁撴潫 / N=鍙栨秷)"
        if ($ans -notmatch '^[Yy]') { Write-Warn2 "宸插彇娑堛€?; exit 0 }
    }
    $running | ForEach-Object {
        try { Stop-Process -Id $_.Id -Force -ErrorAction Stop; Write-Ok "宸茬粨鏉燂細$($_.ProcessName) (PID=$($_.Id))" }
        catch { Write-Warn2 "缁撴潫澶辫触锛?($_.ProcessName) (PID=$($_.Id)) -> $_" }
    }
    Start-Sleep -Seconds 2
} else {
    Write-Ok "娌℃湁姝ｅ湪杩愯鐨?UE 缂栬緫鍣ㄨ繘绋?
}

Write-Step "绗?3 姝?/ 4锛氱紪璇?$Target ($Platform $Config)"
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& $BuildBat $Target $Platform $Config "-Project=$ProjectPath" -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles
$exit = $LASTEXITCODE
$sw.Stop()

if ($exit -ne 0) {
    Write-Err "缂栬瘧澶辫触锛圗xitCode=$exit锛岀敤鏃?$([int]$sw.Elapsed.TotalSeconds)s锛夈€傝鏌ョ湅涓婃柟 UBT 杈撳嚭銆?
    exit $exit
}
Write-Ok "缂栬瘧鎴愬姛锛岀敤鏃?$([int]$sw.Elapsed.TotalSeconds)s"

if ($NoLaunch) {
    Write-Warn2 "宸茶烦杩囪嚜鍔ㄥ惎鍔紙-NoLaunch锛夈€?
    exit 0
}
Write-Step "绗?4 姝?/ 4锛氬惎鍔?UE 缂栬緫鍣?
Start-Process -FilePath $EditorExe -ArgumentList "`"$ProjectPath`""
Write-Ok "宸插惎鍔?UnrealEditor锛屾鍦ㄥ姞杞介」鐩€︹€?