# G1 直线节拍关 — 使用说明

## 已在引擎内建好的资产

| 路径 | 用途 |
|---|---|
| `/Game/Night/Course/Maps/L_Night_G1` | G1 关卡（GameMode 已挂） |
| `/Game/Night/Course/Blueprints/BP_NightCourseGameMode` | 开局刷 Host |
| `/Game/Night/Course/Blueprints/BP_NightCoursePawn` | 相机 + Feel stub；Q/E 已绑 |
| `/Game/Night/Course/Blueprints/BP_NightCourseHost` | Director 宿主 |
| `/Game/Night/Course/Blueprints/BP_NightFoe` / `BP_NightHazard` | 美术子类入口 |
| `/Game/Night/Course/Data/DA_NightG1_T0` | 节拍/窗口/掉落配置 |
| `/Game/Night/Course/Input/IA_NightJump` / `IA_NightAttack` + `IMC_NightCourse` | Q=跳，E/LMB=劈 |

项目默认图已指向 `L_Night_G1`（`DefaultEngine.ini`）。

## 怎么玩（PIE）

1. 打开 `L_Night_G1` → PIE  
2. 红块=怪 → **E**；蓝条=障 → **Q**  
3. 控制台：
   - `Night.Course.Dump`
   - `Night.Course.SkipToExit`
   - `Night.Course.Finish 1`

## 美术对接（只改 BP 即可）

| 类 | 接口 |
|---|---|
| `BP_NightFoe` | `PlaySlashVFX` / `PlayDropBurst` |
| `BP_NightHazard` | `PlayClearVFX` / `PlayImpactVFX` |
| 基类生命周期 | `OnNodeActivated` / `OnJudgeWindowOpened` / `OnResolved` / `OnDespawnRequested` |
| `BP_NightCoursePawn` | `ArtRoot` 下挂 Mesh |
| `DA_NightG1_T0` | `FoeClass` / `HazardClass` 换成美术 BP |

## 调试对接

| 对象 | 接口 |
|---|---|
| `UNightCourseDirector` | `OnPhaseChanged` / `OnNodeResolved` / `OnDebugTick` / `OnFinished` |
| `UNightFeelStubComponent` | `OnDebugSoulChanged` / `OnInputResolved` |
| `ANightCourseHost` | `LastResult` |

## R1 替换

实现 `INightFeelBridge`，`Director->BindFeelBridge`，输入结果走 `NotifyFeelResolved`。

## 验证记录（MCP PIE）

无人操作：8 节点全部 Miss → `Finished success=1 soul=52 drops=0`，流程闭环 OK。有输入时怪 Success 会写入 Ingredients。
