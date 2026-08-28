---
name: ue-android-apk-packaging
description: Packages the MiniGame Unreal project as an Android APK with cooked project data embedded in the APK. Use when building, cooking, staging, archiving, or troubleshooting Android APK output for this project.
disable-model-invocation: true
---

# MiniGame Android APK Packaging

## Defaults

- Project file: `MiniGame/MiniGame.uproject`
- Preferred UE install: `D:/UE/UE_5.8`
- Launcher fallback: `C:/Program Files/Epic Games/UE_5.8`
- Output directory: `MiniGame/Builds/Android`
- Test package configuration: `Development`
- Android data mode: `bPackageDataInsideApk=True` in `MiniGame/Config/DefaultEngine.ini`

## Preconditions

1. Close Unreal Editor, UnrealBuildTool, UnrealPak, and ShaderCompileWorker processes. A locked `Binaries/Win64/*.dll` can make the package fail before cooking.
2. Confirm the Android SDK/NDK is configured in the selected UE installation.
3. Keep the project configured for portrait Android unless the request explicitly changes orientation.

## Package workflow

Run from the repository root. The output cleanup is intentionally limited to the exact project package directory:

```powershell
$ProjectRoot = (Resolve-Path '.').Path
$ProjectFile = (Resolve-Path 'MiniGame/MiniGame.uproject').Path
$EngineRoot = 'D:/UE/UE_5.8'
if (!(Test-Path "$EngineRoot/Engine/Build/BatchFiles/RunUAT.bat")) {
    $EngineRoot = 'C:/Program Files/Epic Games/UE_5.8'
}
$Output = Join-Path $ProjectRoot 'MiniGame/Builds/Android'
if (Test-Path $Output) {
    $OutputFull = (Resolve-Path $Output).Path
    if (!$OutputFull.StartsWith((Join-Path $ProjectRoot 'MiniGame'), [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean an unexpected package path: $OutputFull"
    }
    Get-ChildItem -LiteralPath $OutputFull -Force | Remove-Item -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $Output -Force | Out-Null
}

# Keep SDK selection explicit for this workstation; update these paths if Android Studio moves.
$env:ANDROID_HOME = 'C:/Users/moonyfli/AppData/Local/Android/Sdk'
$env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
$env:ANDROID_NDK_ROOT = "$env:ANDROID_HOME/ndk/27.2.12479018"
$env:NDKROOT = $env:ANDROID_NDK_ROOT
$env:NDK_ROOT = $env:ANDROID_NDK_ROOT
$env:JAVA_HOME = 'C:/Users/moonyfli/AppData/Local/Programs/TencentKona-21/TencentKona-21.0.11.b1'
$env:Path = "$env:ANDROID_HOME/platform-tools;$env:ANDROID_HOME/cmdline-tools/latest/bin;$env:Path"

& "$EngineRoot/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun `
    "-project=$ProjectFile" -noP4 -utf8output -unattended `
    -platform=Android -clientconfig=Development `
    -build -cook -stage -pak -package -archive `
    "-archivedirectory=$Output"
if ($LASTEXITCODE -ne 0) { throw "Android packaging failed with exit code $LASTEXITCODE" }
```

The APK and any generated packaging metadata are placed under `MiniGame/Builds/Android`. Do not launch the editor or save assets automatically after packaging.

## Troubleshooting

- `No OBB found`: verify `bPackageDataInsideApk=True`; do not solve this by adding a store key for a development test package.
- `LNK1104` for `UnrealEditor-MiniGame.dll`: close UE and rerun the package command; do not delete arbitrary binaries.
- Android SDK/NDK error: set `ANDROID_HOME`, `ANDROID_SDK_ROOT`, `NDKROOT`, `NDK_ROOT`, and `JAVA_HOME` in the packaging process; this project currently uses NDK `27.2.12479018` (`r27c`).
- Cook error for a missing asset: inspect the referenced `/Game/...` path and add the owning content directory to Always Cook only when the asset is truly runtime-required.
- Shipping/distribution output: use `-clientconfig=Shipping` only after a valid Android signing key is configured.

## Completion checklist

- [ ] UE/editor processes are closed before the build.
- [ ] Old contents of `MiniGame/Builds/Android` were cleared only after validating the exact path.
- [ ] `RunUAT.bat BuildCookRun` completed with exit code 0.
- [ ] An `.apk` exists under the archive directory.
- [ ] No OBB dependency remains for the test package.
