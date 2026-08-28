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
set AFS=.\win-x64\UnrealAndroidFileTool.exe
set DEVICE=
if not "%1"=="" set DEVICE=-s %1
for /f "delims=" %%A in ('%ADB% %DEVICE% shell "echo $EXTERNAL_STORAGE"') do @set STORAGE=%%A
@echo.
@echo Uninstalling existing application. Failures here can almost always be ignored.
%ADB% %DEVICE% uninstall com.YourCompany.MiniGame
@echo.
@echo Installing existing application. Failures here indicate a problem with the device (connection or storage permissions) and are fatal.
%ADB% %DEVICE% install MiniGame-arm64.apk
@if "%ERRORLEVEL%" NEQ "0" goto Error
%ADB% %DEVICE% shell pm list packages com.YourCompany.MiniGame



%ADB% %DEVICE% shell rm -r %STORAGE%/UnrealGame/MiniGame
%ADB% %DEVICE% shell rm -r %STORAGE%/UnrealGame/UECommandLine.txt
%ADB% %DEVICE% shell rm -r %STORAGE%/obb/com.YourCompany.MiniGame
%ADB% %DEVICE% shell rm -r %STORAGE%/Android/obb/com.YourCompany.MiniGame
%ADB% %DEVICE% shell rm -r %STORAGE%/Download/obb/com.YourCompany.MiniGame












@echo.
@echo Grant READ_EXTERNAL_STORAGE and WRITE_EXTERNAL_STORAGE to the apk for reading OBB file or game file in external storage.
%ADB% %DEVICE% shell pm grant com.YourCompany.MiniGame android.permission.READ_EXTERNAL_STORAGE >nul 2>&1
%ADB% %DEVICE% shell pm grant com.YourCompany.MiniGame android.permission.WRITE_EXTERNAL_STORAGE >nul 2>&1

@echo.
@echo Installation successful
goto:eof
:Error
@echo.
@echo There was an error installing the game or the obb file. Look above for more info.
@echo.
@echo Things to try:
@echo Check that the device (and only the device) is listed with "%ADB$ devices" from a command prompt.
@echo Make sure all Developer options look normal on the device
@echo Check that the device has an SD card.
@pause
