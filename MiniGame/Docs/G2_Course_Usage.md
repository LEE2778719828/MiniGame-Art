# G2 唯一岔口 + A/B 规则 — 使用说明

在 G1 刃心石头链之上：基础段结束后进入**唯一岔口**，选 A 清途 / B 瘴途，再跑分支石链，出口缓冲回传 `FNightResult.RouteTaken`。

对照：`Docs/G1_Course_Usage.md`、`Docs/R2_Gameplay_Plan.md` Phase G2、`Docs/R1_TrackContract.md`

---

## 当前实现状态（2026-08-20）

本页描述的是独立 `NightCourse` 闭环，已经接入：

- `UNightCourseRuleData::BaseRoute`、`BranchRoutes`、`ForkAfterBaseAtomIndex`；
- `BaseAtomCount` / `FNightRuleAtomQueue::TargetAtomCount`：分别控制基础段和分支段的生成总数；
- `FNightRuleAtomEntry::Weight`：同一队列内的 Atom+Actions 模板按权重、Seed 确定性抽样；
- `UNightCourseRuleData::bAutoSelectAtomKeys`：空 `AtomKey` 按有效 Seed 从 `DA_Atoms` 稳定选择，非空值是美术/回归锁定；
- `UNightCourseDirector` 的 `ForkChoice → BranchEnterBuffer → BranchSegment → ExitBuffer`；
- `UNightForkController` 的 AB/AC/BC、Q/E 选路和超时策略；
- `UNightRouteRulesAsset` 的可见块、扣魂倍率、DoT、进场掉落、节奏、循环和带出；
- 魂归零失败、重复启动清理、真实 `RouteTaken`、HUD 和控制台调试。

原先本页的 `DistanceFade`、`BranchA/BBeatCount` 等字段不是当前 C++ 契约，不能按旧字段配置；可见性目前由 `FNightRouteRuleRow::VisibleBlockCount` 控制。资产引用、BP 编译和 Save All 仍由用户手动完成。

## Atom 队列：总数 + 权重

`BaseRoute` 和 `BranchRoutes` 现在都是**模板池**，不是按数组顺序逐项消费：

```json
{
  "seed": 1001,
  "baseAtomCount": 30,
  "baseRoute": [
    { "atomKey": "A", "actions": ["Jump", "Jump", "Kill"], "weight": 5 },
    { "atomKey": "A", "actions": ["Kill", "Kill", "Kill"], "weight": 3 }
  ],
  "branchRoutes": {
    "A": {
      "targetAtomCount": 20,
      "atoms": [
        { "atomKey": "A", "actions": ["Jump", "Kill", "Kill"], "weight": 2 },
        { "atomKey": "B", "actions": ["Kill", "Kill", "Jump"], "weight": 1 }
      ]
    }
  },
  "forkAfterBaseAtomIndex": 30
}
```

- `BaseAtomCount=30`：无岔路时生成 30 个基础 Atom；`0` 为兼容旧配置，使用 `BaseRoute.Num()`。
- `TargetAtomCount=20`：选中 A 分支后生成 20 个分支 Atom；`0` 使用该分支模板数。
- 每个模板的 `Weight` 必须大于 0。上例中基础段两个模板的概率为 `5:3`，同一个 `AtomKey` 可以重复出现，因为 Actions 模板可以不同。
- `forkAfterBaseAtomIndex` 表示岔口前生成多少个基础 Atom；启用岔路时它优先于 `BaseAtomCount` 作为岔口前长度。若希望基础段正好 30 个，填 `30`。
- 同一个 `Seed` 会生成相同的模板序列、Atom 选择和变换；修改 Seed 才会得到另一组加权序列。

在 UE 中直接编辑 `DA_Rules`：基础段设置 `BaseAtomCount`，分支队列设置
`TargetAtomCount`，然后在 `BaseRoute` 或分支 `Atoms` 数组中维护
`AtomKey + Actions + Weight`。不需要为 30 个 Atom 手工填 30 行。

## 流程

```text
BaseSegment → ForkChoice(2.4s) → BranchEnterBuffer(1.2s 自动跳上分支首石)
  → BranchSegment → ExitBuffer → Finished(RouteTaken=A|B)
```

| 相位 | 玩家 | 说明 |
|---|---|---|
| BaseSegment | Q/E 跳砍 | 与 G1 相同 |
| ForkChoice | **Q=左路 / E=右路** | 不计失误；超时默认左牌（AB→A） |
| BranchEnterBuffer | 无操作 | 自动前进到分支首石；满 1.2s 后开分支拍 |
| BranchSegment | Q/E | 应用 A/B 规则（雾/扣魂倍率/蚀火 DoT） |
| ExitBuffer | — | 带出加成后打包 Result |

G2 白模以 **AB** 为主。**AC/BC 与 C 规则 / 换键** 见 `Docs/G3_Course_Usage.md`。

---

## 可见性裁剪（当前实现）

当前 Director 不使用距离淡出材质；路线规则通过
`FNightRouteRuleRow::VisibleBlockCount` 对当前石块之后的可见范围进行硬裁剪，
并同步隐藏/禁用对应的石块、桥和 Atom 视觉 Actor。

```text
LastVisibleStone = CurrentStoneIndex + max(1, VisibleBlockCount)
```

因此，A/B/C 的 `VisibleBlockCount` 是当前唯一生效的可见性旋钮。距离淡出材质
属于后续表现接入，不应当作为当前 Config 字段配置。

---

## A / B 白模数表

| 字段 | A 清途 | B 瘴途 |
|---|---|---|
| VisibleBlockCount | 8 | 3（只压可见，不改速度/窗） |
| SoulPenaltyScale | 1.0 | 1.25 |
| DotSoulPerSecond | 0 | 2.0 |
| EnterDropCount | 0 | 1 |
| CarryOutBonus | 0 | +20%（分支掉落向上取整） |

代码：`Config->RouteRules.Rows[A/B/C]`。分支运行前必须有对应的 RouteRules 行；缺失会失败，不回退硬编码规则。

---

## PIE 步骤

1. 打开 `/Game/Night/Course/Maps/L_Night_G1`，手动确认 `DA_Course`、`DA_Rules`、`DA_Atoms`、BranchRoutes 和 `DA_RouteRules` 已绑定
2. 在 `DA_Rules` 设置 `BaseAtomCount`；启用岔路时设置 `ForkAfterBaseAtomIndex`，并为 A/B/C 队列设置 `TargetAtomCount`
3. 确认 `bEnableFork=true` 且 `ForkAfterBaseAtomIndex` 不超过生成的基础段长度
4. 打完基础段 → HUD 出现 `FORK` 与左右牌 → **Q 选左** 或 **E 选右**
5. 走 B：前方石更少、魂持续掉、进分支多 1 料、结束带出更多
6. Log：`[NightCourseHost] Finished ... route=1|2`（Host 记录真实 `RouteTaken`）

### 控制台

| 命令 | 作用 |
|---|---|
| `Night.Course.Dump` | 相位 / Route / Fork |
| `Night.Course.Validate` | 校验 Config、规则队列和 RouteRules，不修改资产 |
| `Night.Course.ChooseLeft` / `ChooseRight` | 强制选路 |
| `Night.Course.SkipFork` | 默认左路 |
| `Night.Course.ForceKeySwap` / `Reset` | 强制换键 / 清理运行时实体 |
| `Night.Course.SkipToExit` / `Finish` | 同 G1 |

### Config 关键字段（`UNightG1CourseConfig`）

- `bEnableFork`、`ForkTimeoutSeconds`、`BranchEnterBufferSeconds`
- `ForkAfterBaseAtomIndex`、`PreviewRoute`
- `CourseRuleData.BaseAtomCount`、`CourseRuleData.BaseRoute`
- `CourseRuleData.BranchRoutes[A/B/C].TargetAtomCount`、各队列 `Atoms[].Weight`
- `RouteRules.Rows[A/B/C]`
- `DA_Course` 不再提供旧 BeatCount/Proc fallback；缺少 canonical 引用会直接校验失败
- `FNightBootstrap.GiftBuffs.bGuideKite` 开启岔路优势提示；其余礼物效果见 G3

---

## 与 R1 分界

| R2（本阶段） | R1 |
|---|---|
| 岔口状态机、路线牌语义、雾可见块 | 跳/砍判定窗公式 |
| A/B 扣魂倍率与 DoT **调用** `ApplySoulPenalty` | 魂灯数值实现 |
| `RouteTaken` 写入 Result | 不改岔口逻辑 |

岔口期 **不** 走 `TryResolveInput`；选路不算 Wrong/Miss。

---

## 代码锚点

- `UNightCourseDirector` — 相位与 Append 分支石链  
- `UNightForkController` — 2.4s 超时  
- `UNightRouteRulesAsset` — A/B 表  
- `ANightCoursePawn` — Fork 时 Q/E 改选路  
- `ANightCourseHUD` — 左右牌 Canvas  
