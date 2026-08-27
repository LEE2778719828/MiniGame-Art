# Material Tune Archive — R03 Fork Tune

日期：2026-08-27
关卡：`/Game/Night/Course/Maps/L_Night_G1_ForkTest`
本轮范围：同时调整三个岔路后处理材质；R02 的默认画面、Plane 和场景雾保持不动。

## 对应关系

- ForkA → 原画 2：深蓝夜景、红月
- ForkB → 原画 3：红色氛围、暖色月亮
- ForkC → 原画 4：灰白雾化、低饱和

## ForkA — 深蓝夜景

资产：`/Game/Adidiang/模型和材质/材质实例/M_houchulipaoku_Inst_ForkA`

- 亮度 `0.80`
- 对比度 `1.18`
- 饱和度 `0.82`
- 调色强弱 `0.62`
- 月亮亮度 `1.05`
- 月亮位置 `X=0.70, Y=0.11`
- 月亮颜色 `(0.75,0.08,0.10,1)`
- 雾可视距离 `10500`
- 雾渐变强度 `0.82`
- 雾颜色 `(0.12,0.18,0.30,1)`
- 暗部偏色 `(0.08,0.15,0.30,1)`
- 亮部偏色 `(0.65,0.76,0.95,1)`

## ForkB — 红色氛围

资产：`/Game/Adidiang/模型和材质/材质实例/M_houchulipaoku_Inst_ForkB`

- 亮度 `0.96`
- 对比度 `1.22`
- 饱和度 `1.08`
- 调色强弱 `0.70`
- 月亮亮度 `1.10`
- 月亮位置 `X=0.52, Y=0.12`
- 月亮颜色 `(1.00,0.48,0.43,1)`
- 雾可视距离 `9000`
- 雾渐变强度 `0.80`
- 雾颜色 `(0.22,0.025,0.035,1)`
- 暗部偏色 `(0.30,0.04,0.05,1)`
- 亮部偏色 `(1.00,0.68,0.60,1)`

## ForkC — 灰白雾化

资产：`/Game/Adidiang/模型和材质/材质实例/M_houchulipaoku_Inst_ForkC`

- 亮度 `0.90`
- 对比度 `0.95`
- 饱和度 `0.40`
- 调色强弱 `0.62`
- 月亮亮度 `1.00`
- 月亮位置 `X=0.53, Y=0.13`
- 月亮颜色 `(0.95,0.85,0.82,1)`
- 雾可视距离 `7000`
- 雾渐变强度 `0.72`
- 雾颜色 `(0.66,0.68,0.68,1)`
- 暗部偏色 `(0.42,0.44,0.48,1)`
- 亮部偏色 `(0.95,0.95,0.95,1)`

## 未修改

- R02 默认后处理材质 `M_houchulipaoku_Inst`
- R02 Plane 材质 `MI_G1_Plane_Tune`
- R02 场景雾 `NightFog`
- `DA_Course`、`DA_RouteRules`
- 两个后处理父材质

## 快照与测试

- 修改前：`NightMaterialTune_R03_ForkA_BeforeTune`
- 修改前：`NightMaterialTune_R03_ForkB_BeforeTune`
- 修改前：`NightMaterialTune_R03_ForkC_BeforeTune`
- 修改后：`NightMaterialTune_R03_ForkA_AfterTune`
- 修改后：`NightMaterialTune_R03_ForkB_AfterTune`
- 修改后：`NightMaterialTune_R03_ForkC_AfterTune`
- PIE 启动测试截图：`MaterialTuneArchive_R03_ForkTune_Test.png`

三套材质均通过 UE 资产校验，无错误和警告。
