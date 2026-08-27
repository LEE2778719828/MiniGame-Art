# Material Tune Archive — R04 Apply Fix

日期：2026-08-28
关卡：`/Game/Night/Course/Maps/L_Night_G1_ForkTest`

## 目的

检查发现此前 R02/R03 的部分修改只存在于当时的编辑器状态，当前打开的项目实际仍使用旧参数。本轮将已确认的 R02/R03 结果重新应用并保存到当前项目。

## 已应用

- 默认后处理：`/Game/Adidiang/模型和材质/材质实例/M_houchulipaoku_Inst`
- 旧后处理叠加材质：`/Game/新建文件夹/模型和材质/材质实例/MI_houchuli_Inst`
- ForkA：`/Game/Adidiang/模型和材质/材质实例/M_houchulipaoku_Inst_ForkA`
- ForkB：`/Game/Adidiang/模型和材质/材质实例/M_houchulipaoku_Inst_ForkB`
- ForkC：`/Game/Adidiang/模型和材质/材质实例/M_houchulipaoku_Inst_ForkC`
- Plane 专用材质：`/Game/Night/Course/Art/Environment/Night/MI_G1_Plane_Tune`

默认材质恢复 R02；ForkA/B/C 恢复 R03，具体参数见 `R02_JointTune.md` 和 `R03_ForkTune.md`。

## 关卡绑定

- `DA_Course.DefaultPostProcessMaterial` 指向默认后处理材质
- `DA_RouteRules` 的 A/B/C 行分别指向 ForkA/ForkB/ForkC
- `G1_Exposure` 保持 Unbound，并继续引用旧后处理材质和默认后处理材质
- `Plane.StaticMeshComponent0` 已从 `M_dibang` 改为 `MI_G1_Plane_Tune`
- `BP_NightCourseHost.NightFog` 已应用：`FogDensity=0.015`、`FogHeightFalloff=0.22`、`FogMaxOpacity=0.78`、`StartDistance=2500`

## 验证

- 重新打开并保存 `L_Night_G1_ForkTest`
- PIE 启动成功
- 回读确认默认、ForkA、ForkB、ForkC、Plane 材质关键参数已生效
- PIE 应用验证截图：`MaterialTuneArchive_R04_Apply_Verified.png`

## 快照

- 应用前：`NightMaterialTune_R04_Default_BeforeApply`
- 应用前：`NightMaterialTune_R04_Legacy_BeforeApply`
- 应用前：`NightMaterialTune_R04_ForkA_BeforeApply`
- 应用前：`NightMaterialTune_R04_ForkB_BeforeApply`
- 应用前：`NightMaterialTune_R04_ForkC_BeforeApply`
- 应用前：`NightMaterialTune_R04_Plane_BeforeApply`
- 应用前场景：`NightMaterialTune_R04_Scene_BeforeApply`
- 应用后：`NightMaterialTune_R04_Default_AfterApply`
- 应用后：`NightMaterialTune_R04_Legacy_AfterApply`
- 应用后：`NightMaterialTune_R04_ForkA_AfterApply`
- 应用后：`NightMaterialTune_R04_ForkB_AfterApply`
- 应用后：`NightMaterialTune_R04_ForkC_AfterApply`
- 应用后：`NightMaterialTune_R04_Plane_AfterApply`
- 应用后场景：`NightMaterialTune_R04_Scene_AfterApply`
