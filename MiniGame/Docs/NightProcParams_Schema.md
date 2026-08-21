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

## 确定性规则

- 相同有效 Seed、相同候选库和相同队列，得到相同 Atom key 与变换。
- 不同 Seed 会影响模板抽样和空 key 的候选选择，但不会替换显式 key。
- 模板抽样和 Atom 候选选择使用独立 `FRandomStream`，不会因 Yaw、敌人或其他随机消费者变化。
- DataAsset 不会被运行时改写；资产绑定和 Save All 由使用者手动完成。
