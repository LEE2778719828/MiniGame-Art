setlocal ENABLEEXTENSIONS
setlocal ENABLEDELAYEDEXPANSION
if NOT "%UE_SDKS_ROOT%"=="" (call %UE_SDKS_ROOT%\HostWin64\Android\SetupEnvironmentVars.bat)
set ADB=adb.exe
where.exe /q %ADB%
if /i "%ERRORLEVEL%" neq "0" (
	set STUDIO_SDK_PATH=%ANDROID_HOME%
	if not exist "!STUDIO_SDK_PATH!" (
		set STUDIO_SDK_PATH=
		for /f "tokens=2*" %%A in ('reg.exe query "HKLM\SOFTWARE\Android Studio" /v "SdkPath"') do (set STUDIO_SDK_PATH=%%B)
		if not exist "!STUDIO_SDK_PATH!" (
			set STUDIO_SDK_PATH=%LOCALAPPDATA%\Android\Sdk
			if not exist "!STUDIO_SDK_PATH!" (
				echo Unable to locate local Android SDK location.
				exit /b 1
			)
		)
	)
	set ADB=!STUDIO_SDK_PATH!\platform-tools\adb.exe
)
set DEVICE=
if not "%1"=="" set DEVICE=-s %1
for /f "delims=" %%A in ('%ADB% %DEVICE% shell "echo $EXTERNAL_STORAGE"') do @set STORAGE=%%A
@echo.
@echo Uninstalling existing application. Failures here can almost always be ignored.
%ADB% %DEVICE% uninstall com.YourCompany.MiniGame
@echo.
echo Removing old data. Failures here are usually fine - indicating the files were not on the device.
%ADB% %DEVICE% shell rm -r %STORAGE%/UnrealGame/MiniGame
%ADB% %DEVICE% shell rm -r %STORAGE%/UnrealGame/UECommandLine.txt
%ADB% %DEVICE% shell rm -r %STORAGE%/obb/com.YourCompany.MiniGame
%ADB% %DEVICE% shell rm -r %STORAGE%/Android/obb/com.YourCompany.MiniGame
@echo.
@echo Uninstall completed
