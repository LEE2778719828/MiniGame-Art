# G3.5 桥段程序化关卡 + 设计器

在 G1–G3 白模可玩闭环之上，插入本阶段后再回到原计划 G4。

## 当前实现状态（2026-08-20）

本阶段的可玩课程已经迁移到 canonical Atom 链：
`DA_Course` → `DA_Rules` + `DA_Atoms` + `DA_RouteRules`。
`RouteModes[A/B/C]` 与 `BranchRoutes[A/B/C]` 描述 `AtomKey + Actions + Weight` 模板池；
各队列的 `TargetAtomCount` 决定主段和分支段的生成总数。Day 的 `DefaultRoute`
选择主段模式，模板按权重
使用有效 Seed 稳定抽样；空 `AtomKey` 再从 `DA_Atoms` 稳定选择，非空
`AtomKey` 保留为美术/回归锁定。
相邻 Atom 由 Entry/Exit 对齐，并使用 `DA_Atoms.TransitionJumpGapCm` 的固定
长距离 Jump 衔接。缺少 canonical 引用或兼容 Atom 会直接校验/构建失败。
旧的 `ProcParams` 课程回退和 transient `Night.Course.ImportParams` 已移除；
Standalone `FNightProcCourseParams` / `UNightTrackGenerator` 仅保留给独立测试。

## 目标

1. **错键不推进**：跳只能跳、砍只能砍；WrongButton 扣魂但窗口保留。
2. **美术桥/角/怪**：桥继续由 Atom BP 的 `BridgeVisualComponent` 配置；敌人 BP 统一登记到
   `DA_Course.FoeActorMap`，Atom 落脚点只保留可选的 `Temporary Preview Prefab`。
3. **Seed Atom 组合**：同 Seed 得到相同 Atom key/变换，不同 Seed 在有候选时得到不同合法组合。
4. **HTML 俯视设计器**：导出 `RouteModes/BranchRoutes` UE 可读 JSON，并将旧
   `baseRoute/baseAtomCount` 导入迁移到 `routeModes.A`。

## 关键文件

| 路径 | 说明 |
|---|---|
| `NightCourseTypes.h` | `FNightProcCourseParams` / `FNightBridgeSpec` / WorldPose 石 |
| `NightTrackGenerator.*` | Seed 生成器（FRandomStream） |
| `NightBridgeSegmentActor.*` | 桥板 Actor |
| `NightCourseDirector.*` | Seed 选择、WorldPose、桥生成和错键门控 |
| `Tools/NightCourseDesigner.html` | 俯视策划工具 |
| `Docs/G2_Course_Usage.md` / `G3_Course_Usage.md` | 岔路和换键配置 |

## 使用

1. 浏览器打开 `MiniGame/Tools/NightCourseDesigner.html`，编辑 Actions 和 Seed。
2. 导出 Atom Rule JSON，导入 `DA_Rules.EditorJson` 或使用 `ImportJson`。
3. 在 UE 中手动确认 `DA_Course`、`DA_Rules`、`DA_Atoms`、`DA_RouteRules`
   已绑定；Save All 由使用者手动执行。

## 美术导入（编辑器）

| 源 | Content 目标 | 绑到 |
|---|---|---|
| `ArtSubmit/Stones/Bridge/mini_qiao/muban1.fbx` `muban2.fbx` | `/Game/Night/Course/Art/Bridge/` | Atom BP 的 BridgeVisualComponent |
| `ArtSubmit/Character/mini_zhujue/zhujue.fbx` | `/Game/Night/Course/Art/Hero/` | `BP_NightHero` |
| `ArtSubmit/Foes/mini_fish` / `mini_cantingguai` / `mini_box` | `/Game/Night/Course/Art/Foe/` | `BP_NightFoeM01`–`M05`，再登记到 `DA_Course.FoeActorMap` 与 `FoeDropMap` |
| `ArtSubmit/Environment/mini_canguan/` | `/Game/Night/Course/Art/Environment/` | Environment mesh |

导入后由美术在敌人 BP 中绑定 Mesh/碰撞/动画，并在 `DA_Course.FoeActorMap` 绑定
`EFoeId`，再在 `FoeDropMap` 绑定对应 `EIngredientId`。Atom BP 只在需要对准位置时填写
临时预览 Prefab；策划不填写运行时 Mesh/Transform。
资产绑定和 Save All 由用户手动完成。

## 完成标准

- [x] Jump 石按 E 不前进；按 Q 前进（Attack 同理）
- [x] Atom Actions 按 LandingPoint 顺序绑定；同 Seed 可复现 Atom 组合
- [x] HTML 俯视图 + 导出 JSON 可被 `UNightCourseRuleData::ImportJson` 消费
- [x] 选路后从 BranchRoutes 重建 Atom 队列并保留基础段状态
- [x] `Night.Course.Validate`、`ChooseLeft/Right`、`SkipFork`、`ForceKeySwap`、`Reset`
