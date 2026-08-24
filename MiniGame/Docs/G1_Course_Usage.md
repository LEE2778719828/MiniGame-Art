# G1 直线节拍关 — 使用说明（刃心石头链）

## 核心交互（持久约定）

关卡是一串**落脚石**，不是「每个判定自带一对平台」。

示例：`石头1` → `空` → `石头2` → `石头带怪3`

1. 玩家站在**石头1**：前方是空档 → 按 **Q 跳** → 落到石头2  
2. 站在**石头2**：前方石头3带怪 → 按 **E 砍** → 冲到石头3，胶囊体消失  
3. 不按键则角色/镜头/石头都不推进  

| 概念 | 含义 |
|---|---|
| 石头 | 落脚方块；整条路的实体 |
| 空 | 两石间距大 → 当前石上要跳 |
| 带怪 | 目标石上有胶囊体 → 当前石上要砍；怪挂在目标石 |

## 已在引擎内建好的资产

| 路径 | 用途 |
|---|---|
| `/Game/Night/Course/Maps/L_Night_G1_ForkTest` | 唯一正式 G1 关卡 |
| `/Game/Night/Course/Blueprints/BP_NightCourseGameMode` | 开局刷 Host |
| `/Game/Night/Course/Blueprints/BP_NightCoursePawn` | 后上方相机 + Feel stub；Q/E |
| `/Game/Night/Course/Blueprints/BP_NightCourseHost` | Director 宿主 |
| `/Game/Night/Course/Blueprints/BP_NightFoeM01`–`M05` | `DA_Course.FoeActorMap` 指定的敌人 Actor |
| `/Game/Night/Course/Config/DA_Course` | G1 课程调优、规则、Atom 引用和敌人 Map |
| `/Game/Night/Course/Config/DA_Atoms` | A/B/C Atom BP 库与跳跃衔接间距 |
| `/Game/Night/Course/Config/DA_Rules` | `BaseAtomCount` + 加权 `BaseRoute/BranchRoutes`；空 AtomKey 由 Seed 选择 |
| `/Game/Night/Course/Input/IA_NightJump` / `IA_NightAttack` + `IMC_NightCourse` | Q=跳，E/LMB=劈 |

默认图：`L_Night_G1_ForkTest`（`DefaultEngine.ini`）。

## 怎么玩（PIE）

1. 打开 `L_Night_G1_ForkTest` → PIE
2. 看 HUD：`NOW: JUMP` / `NOW: ATTACK` → 按 Q / E  
3. 控制台：`Night.Course.Dump` / `SkipToExit` / `Finish 1`

## 数据语义（C++）

- `FNightStoneSpec`：一块石的轨距、是否带怪、`EFoeId`、掉落
- `FNightBeatSpec`：站在 From 石、按 Jump/Attack、前进到 To 石  
- `DA_Course.FoeActorMap`：`EFoeId` 到 `ANightCourseStoneActor` Blueprint 的唯一运行时映射；
  Director 直接生成映射 Actor，不再生成原生载体再挂视觉 Actor
- `DA_Course.HouseRoadside` / `PoleRoadside`：道路两侧房屋和杆子的独立 Blueprint 池、
  权重、间距、桥侧左右偏移、Z 偏移和随机种子偏移
- `ANightRoadsideSegmentActor`：房屋/杆子 Blueprint 的父类；`StartMarker` 和
  `EndMarker` 定义模块沿道路方向的占用范围，房屋按标记首尾连续拼接
- Atom Composer：按 `Actions` 绑定 Atom 内按序落脚点；相邻 Atom 固定用长距离 Jump 衔接

## 美术对接

| 对象 | 接口 |
|---|---|
| 石/敌人 Actor | `FoeActorMap` 指定的 Blueprint 同时承载 Mesh、碰撞和击杀逻辑 |
| `BP_NightCoursePawn` | `ArtRoot` 下挂角色 |
| `DA_Course` | 运行速度、惩罚、岔口和玩法调优 |
| `DA_Atoms` | Atom BP 的落脚点、桥、Entry/Exit、`TransitionJumpGapCm`；落脚点可选临时预览 BP |
| `DA_Rules` | `Seed`、目标 Atom 数、`AtomKey + Actions + Weight` 模板池 |

## 道路两侧房屋 / 杆子

运行时道路装饰由 Director 根据已组合的石头和桥生成，不会写回任何 DataAsset：

1. 在 `DA_Course.HouseRoadside` 中启用房屋，向 `BlueprintPool` 添加一个或多个
   `ANightRoadsideSegmentActor` 子类，并用 `Weight` 控制选择概率。
2. 在 `DA_Course.PoleRoadside` 中独立配置杆子池和杆子间距；杆子不会消耗或改变房屋链。
3. `SpacingCm` 是一个模块 End 到下一个模块 Start 之间的间距。房屋使用 `0` 可保持连续；
   房屋/杆子的网格、组件、动画和缩放都由各自 Blueprint 控制，生成器不覆盖缩放。
4. `LeftBridgeOffsetCm` / `RightBridgeOffsetCm` 是相对道路基准线的左右距离。
   房屋以路线起点的世界 `Y` 为基准，沿世界固定 `X` 轴连续排列，不跟随 Atom 的方向；右侧房屋会自动做
   `Y=-1` 镜像。房屋统一使用路线第一个节点的世界 `Z`，再叠加 `HouseRoadside.ZOffsetCm`；
   杆子仍使用桥方向和采样高度，没有桥的长跳段使用相邻石头连线补齐。
5. `RandomSeedOffset`、Blueprint 权重和杆子 `RandomYawRangeDeg` 只影响道路装饰，
   并使用独立随机流，不改变 Atom、敌人或掉落结果。

编辑器中 Host 会预览整条最终道路，Atom BP 可用 `HouseRoadsidePreviewPrefab` /
`PoleRoadsidePreviewPrefab` 预览单段位置；这些预览字段不会参与运行时选择。

## Host LayoutBounds

Host 的 `bEnforceLayoutBounds` 必须开启，`LayoutBoundsExtent` 是以
`LayoutBounds` 中心为原点的半尺寸。运行时会优先尝试 Atom 的候选角度；如果所有角度
都越界，会只沿世界 `Y` 轴把当前完整 Atom 平移到 Bounds 内再继续生成，`X/Z` 保持原值。
这个应急平移可能让当前 Atom 与前一个 Atom 的过渡跳跃变长，但不会让整局直接卡死；
如果仅靠 Y 平移仍无法放入 Bounds，仍会报告生成失败。

## R1 替换

实现 `INightFeelBridge`，`Director->BindFeelBridge`；Jump↔Hazard，Attack↔Enemy。

## 文档 / 规则

- 本文件 + `Docs/R2_Gameplay_Plan.md` Phase G1  
- Cursor 规则：`.cursor/rules/blade-heart-stone-chain.mdc`
