@echo off
REM ============================================================
REM  MiniGame Editor 一键重编入口（双击即可）
REM  会调用 Scripts\Rebuild-Editor.ps1，绕过执行策略限制
REM ============================================================
setlocal
set "SCRIPT=%~dp0Scripts\Rebuild-Editor.ps1"

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
set "RC=%ERRORLEVEL%"

echo.
echo ============================================================
echo  流程结束，ExitCode=%RC%
echo ============================================================
pause
exit /b %RC%
