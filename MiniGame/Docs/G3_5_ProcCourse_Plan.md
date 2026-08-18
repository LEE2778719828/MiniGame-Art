# G3.5 桥段程序化关卡 + 设计器

在 G1–G3 白模可玩闭环之上，插入本阶段后再回到原计划 G4。

## 目标

1. **错键不推进**：跳只能跳、砍只能砍；WrongButton 扣魂但窗口保留。
2. **美术桥/角/怪**：分类后的 `ArtSubmit/Character`、`ArtSubmit/Stones/Bridge`、`ArtSubmit/Foes` → `/Game/Night/Course/Art/**`，运行时软引用绑定。
3. **Seed 程序化石链**：总节点、Yaw 弯折(0.1°)、岔路区间、环境类型、换键频率。
4. **HTML 俯视设计器**：导出 UE 可读 JSON。

## 关键文件

| 路径 | 说明 |
|---|---|
| `NightCourseTypes.h` | `FNightProcCourseParams` / `FNightBridgeSpec` / WorldPose 石 |
| `NightTrackGenerator.*` | Seed 生成器（FRandomStream） |
| `NightProcParamsAsset.*` | DA + JSON 导入导出 |
| `NightBridgeSegmentActor.*` | 桥板 Actor |
| `NightCourseDirector.*` | 错键门控、WorldPose、桥生成、ImportParams |
| `Tools/NightCourseDesigner.html` | 俯视策划工具 |
| `Docs/NightProcParams_Schema.md` | JSON 字段表 |

## 使用

1. 浏览器打开 `MiniGame/Tools/NightCourseDesigner.html`，调参预览，导出 Params 或 Baked JSON。
2. 将 JSON 放到工程目录（如 `Saved/NightProcCourseParams.json`）。
3. PIE 控制台：`Night.Course.ImportParams Saved/NightProcCourseParams.json`
4. 或在 Host/DA 上勾 `bUseProcGenerator` / 指定 `ProcParamsAsset`。

## 美术导入（编辑器）

| 源 | Content 目标 | 绑到 |
|---|---|---|
| `ArtSubmit/Stones/Bridge/mini_qiao/muban1.fbx` `muban2.fbx` | `/Game/Night/Course/Art/Bridge/` | Config/`ProcParams` → BridgeMeshA/B |
| `ArtSubmit/Character/mini_zhujue/zhujue.fbx` | `/Game/Night/Course/Art/Hero/` | HeroMesh |
| `ArtSubmit/Foes/mini_fish` / `mini_cantingguai` / `mini_box` | `/Game/Night/Course/Art/Foe/` | FoeMeshM01–M03 |
| `ArtSubmit/Environment/mini_canguan/` | `/Game/Night/Course/Art/Environment/` | Environment mesh |

导入后在 `DA_NightG1_T0` 或 `DA_NightProcParams` 填 SoftObject 路径。未导入时仍用 BasicShapes 白盒。

## 完成标准

- [x] Jump 石按 E 不前进；按 Q 前进（Attack 同理）
- [x] 参数改 TotalNodes / MaxYaw / ForkRange / Seed 后轨迹变化；同 Seed 可复现
- [x] HTML 俯视图 + 导出 JSON 可被 `Night.Course.ImportParams` 消费
