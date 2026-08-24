# Canonical Atom Rule JSON

Night Course 的策划 JSON 只描述 Atom 模板（AtomKey、Actions、Weight）和生成数量，
不描述 Mesh、Transform 或桥材质。
对应 C++：`UNightCourseRuleData`；设计器：
`MiniGame/Tools/NightCourseDesigner.html`。

```json
{
  "seed": 1001,
  "autoSelectAtomKeys": true,
  "baseAtomCount": 30,
  "baseRoute": [
    { "atomKey": "A", "actions": ["Jump", "Jump", "Kill"], "weight": 5 },
    { "atomKey": "A", "actions": ["Kill", "Kill", "Kill"], "weight": 3 }
  ],
  "branchRoutes": {
    "A": {
      "targetAtomCount": 20,
      "atoms": [{ "atomKey": "", "actions": ["Kill", "Jump", "Kill"], "weight": 1 }]
    },
    "B": {
      "targetAtomCount": 20,
      "atoms": [{ "atomKey": "B", "actions": ["Jump", "Kill", "Jump"], "weight": 1 }]
    },
    "C": {
      "targetAtomCount": 20,
      "atoms": [{ "atomKey": "", "actions": ["Kill", "Kill", "Jump"], "weight": 1 }]
    }
  },
  "forkAfterBaseAtomIndex": 30
}
```

| 字段 | 含义 |
|---|---|
| `seed` | Atom 自动选择使用的规则 Seed；运行时 Bootstrap.Seed 非零时覆盖 |
| `autoSelectAtomKeys` | `true` 时空 key 从 `DA_Atoms` 稳定选择；`false` 时所有 key 必须显式填写 |
| `baseAtomCount` | 基础段目标 Atom 数；为 0 时兼容旧配置，使用 `baseRoute` 模板数 |
| `baseRoute` | 基础段模板池；每项的 Actions 数量必须等于 Atom 落脚点数减一，按 `weight` 抽样 |
| `branchRoutes` | A/B/C 分支模板池对象；每个对象用 `targetAtomCount` 控制生成总数 |
| `forkAfterBaseAtomIndex` | 基础队列中进入岔口前保留的 Atom 数量 |
| `atomKey` | 空值=Seed 自动选择；非空值=美术/回归锁定，不会被 Seed 替换 |
| `weight` | 模板权重，必须大于 0；同一 AtomKey 可以有多种 Actions 模板 |

`Route`、`transitionAction` 和 AtomRouteData 的 `AtomSequence` 已移除。
相邻 Atom 的衔接固定为长距离 Jump，间距由 `DA_Atoms.TransitionJumpGapCm`
配置。没有兼容落脚点数量的 Atom 候选时，校验/构建直接失败。

## 道路两侧装饰

房屋和杆子不写入本文件的 Rule JSON，也不写入 `DA_Rules`。它们配置在
`DA_Course`：

- `HouseRoadside` 与 `PoleRoadside` 是两套独立设置。
- `BlueprintPool` 支持多个 `ANightRoadsideSegmentActor` 子类，每项的 `Weight`
  控制确定性随机选择。
- `SpacingCm` 控制相邻模块标记之间的间距；房屋使用 `0` 表示连续拼接。
- `LeftBridgeOffsetCm`、`RightBridgeOffsetCm` 和 `ZOffsetCm` 控制偏移；房屋以路线起点
  的世界 `Y` 为基准，并统一使用路线第一个节点的世界 `Z`；`ZOffsetCm` 再叠加到
  这个固定高度。杆子以采样到的道路/桥中心线和高度为基准。
- 房屋/杆子 Blueprint 的 `StartMarker`、`EndMarker` 定义自身占用长度；Actor 的
  网格和缩放由 Blueprint 自己负责。
- 房屋沿世界固定 `X` 轴排列，不跟随 Atom 的旋转方向；左右侧分别使用对应的
  `Y` 偏移，生成到道路右侧时会对 Actor 做 `Y=-1` 镜像。杆子仍按道路/桥方向排列。
- 房屋和杆子使用由课程 Seed、类别、侧别和 `RandomSeedOffset` 派生的独立随机流，
  不会改变 Atom、敌人或掉落的随机结果。

运行时由 `UNightCourseDirector` 生成，分支重建时同步清理和重建；Host 预览整条
道路，Atom 的 `HouseRoadsidePreviewPrefab` / `PoleRoadsidePreviewPrefab` 只用于
编辑器中的单段对位。

## 确定性规则

- 相同有效 Seed、相同候选库和相同队列，得到相同 Atom key 与变换。
- 不同 Seed 会影响模板抽样和空 key 的候选选择，但不会替换显式 key。
- 模板抽样和 Atom 候选选择使用独立 `FRandomStream`，不会因 Yaw、敌人或其他随机消费者变化。
- DataAsset 不会被运行时改写；资产绑定和 Save All 由使用者手动完成。
