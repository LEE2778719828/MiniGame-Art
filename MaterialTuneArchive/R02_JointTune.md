# Material Tune Archive — R02 Joint Tune

日期：2026-08-27
关卡：`/Game/Night/Course/Maps/L_Night_G1_ForkTest`
参考方向：默认画面对应原画 1；本轮同时处理月亮、Plane 地面和远景雾气。

## 本轮改动

### 1. 月亮与后处理雾

`/Game/Adidiang/模型和材质/材质实例/M_houchulipaoku_Inst`

- `月亮位置X`: `0.325333` → `0.50`
- `月亮位置Y`: `0.165334` → `0.13`
- `雾可视距离`: `10560` → `9000`
- `雾渐变强度`: `0.985562` → `0.78`
- `雾颜色`: `(1,1,1,1)` → `(0.56,0.60,0.60,1)`

同时调整当前 `G1_Exposure` 中另一套仍在生效的后处理材质：

`/Game/新建文件夹/模型和材质/材质实例/MI_houchuli_Inst`

- `月亮位置`: `(0.34,0,0,1)` → `(0.50,0.13,0,1)`
- `雾可视距离`: 继承值 `20000`，本轮覆盖为 `14000`

### 2. Plane 地面

新建材质实例：

`/Game/Night/Course/Art/Environment/Night/MI_G1_Plane_Tune`

父材质：`/Game/新建文件夹/模型和材质/材质/M_dibang`

- `地板近处颜色4`: `(0,0,0,1)` → `(0.005,0.12,0.14,1)`
- `地板远处颜色4`: `(0.089111,0.269063,0.633681,1)` → `(0.02,0.34,0.36,1)`
- 关卡中的 `Plane.StaticMeshComponent0` 已改为使用该实例

### 3. 场景雾

修改关卡实例 `BP_NightCourseHost` 的 `NightFog`（`ExponentialHeightFogComponent`）：

- `FogDensity`: `0.015`
- `FogHeightFalloff`: `0.22`
- `FogMaxOpacity`: `0.78`
- `StartDistance`: `2500`

`FogInscatteringColor` 的写入未得到可靠回读，本轮不将它计为已修改项。

## 未修改

- `DA_Course`、`DA_RouteRules`
- A/B/C 三个岔路后处理材质实例
- `M_houchulipaoku`、`M_houchuli` 两个父材质
- 其他关卡 Actor

## 快照与截图

- 修改前资产快照：`NightMaterialTune_R02_Default_BeforeJointTune`
- 修改前资产快照：`NightMaterialTune_R02_LegacyPostProcess_BeforeJointTune`
- 修改前场景快照：`NightMaterialTune_R02_BeforeJointTune`
- 修改后默认材质快照：`NightMaterialTune_R02_Default_AfterJointTune`
- 修改后旧后处理快照：`NightMaterialTune_R02_LegacyPostProcess_AfterJointTune`
- 修改后 Plane 材质快照：`NightMaterialTune_R02_Plane_AfterJointTune`
- 修改后场景快照：`NightMaterialTune_R02_AfterJointTune`
- PIE 截图：`MaterialTuneArchive_R02_JointTune.png`
