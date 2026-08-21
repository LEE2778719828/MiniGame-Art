# G3 C 倒途 + 换键编排 — 使用说明

在 G2 岔口之上：启用 **AC/BC**、完整 **C 路线规则**（逆火 / 掉 2 / 掉落循环 / +30% 带出），以及按关卡表触发的 **换键**（预警 → 安全窗停拍 → `SetControlScheme`）。

对照：`Docs/G2_Course_Usage.md`、`Docs/R2_Gameplay_Plan.md` Phase G3

---

## 当前实现状态（2026-08-20）

AC/BC 由 `FNightBootstrap.ForkPair` 覆盖，C 路线由同一套 `BranchRoutes[C]` 和
`RouteRules.Rows[C]` 驱动。换键状态机已经是：
`KeySwapWarning → KeySwapSafetyHold → BranchSegment`；SafetyHold 期间不会打开新拍。
`INightFeelBridge::SetControlScheme` 由 Director 调用，HUD 显示真实换键阶段和剩余时间。

当前使用 `UNightG1CourseConfig::LevelRules`（按 `Bootstrap.LevelId` 选择），不再使用旧的
`LevelRows`、`BranchC` 字段。也可以直接编辑
`UNightG1CourseConfig::KeySwapCues`；换键 Cue 不再从临时 Proc 配置生成。

## 流程增量

```text
… → ForkChoice(AB|AC|BC) → Branch…
  若走 C 且关卡表有 KeySwaps：
    分支拍结算后 → KEY SWAP WARN → APPLY SetControlScheme → SAFETY HOLD（停开拍）→ 继续 Branch
```

| 能力 | 说明 |
|---|---|
| AC / BC | Fork 真左右牌；不再强制 AB |
| C 逆火 | `bReverseFire`：DoT 在前进中也扣魂 |
| C 掉料 | `EnterDropCount=2`；`DropCycle` 循环食材 |
| C 带出 | `CarryOutBonus=0.3` |
| 换键 | 示例为 L2 两次、L3 三次；必须在关卡行或 Proc Params 中手动配置 |
| 定键钩子 | `GiftBuffs.bKeyCoin` 跳过首次换键 Cue |
| 引路纸鸢 | `GiftBuffs.bGuideKite` 显示左右路线长度、可见块和扣魂倍率 |
| 借命 | `GiftBuffs.bSpareLamp` 仅保护分支首次失误，不推进错误输入 |
| 饕餮食盒 | `GiftBuffs.bTaotieBox` 令 `FoeWeightOverride` 只作用于前 `TaotieFoeOverrideCount` 个怪，并校验带出锁定食材 |

---

## 关卡完全可调参数（Config）

`UNightG1CourseConfig` 主要旋钮：

| 分类 | 字段 |
|---|---|
| 基础段 | `DA_Rules.BaseAtomCount` + 加权 `BaseRoute` / `DA_Atoms` / `DA_Course.AdvanceSpeed` |
| 岔口 | `bEnableFork` / `ForkTimeoutSeconds` / `bForkTimeoutPickLeft` / `BranchEnterBufferSeconds` / `BranchEntryGapCm` |
| 分支布局 | `DA_Rules.BranchRoutes[A/B/C].TargetAtomCount` + 加权 `Atoms`；空 AtomKey 由 `Seed` 选择 |
| 路线规则 | 必填 `RouteRules.Rows[A/B/C]`（可见块、倍率、DoT、逆火、进分支掉料、循环、带出） |
| 关卡选择 | `Bootstrap.LevelId` → `LevelRules`；`Bootstrap.ForkPair`，或 Config 的 `PreviewRoute` |
| 换键全局 | `bEnableKeySwap` / `DefaultKeySwapWarningSeconds` / `DefaultKeySwapSafetySeconds` |
| 可见性 | `RouteRules.Rows[A/B/C].VisibleBlockCount` 硬裁剪 |
| 礼物 | `TaotieFoeOverrideCount`（默认 4） |

`FNightKeySwapCue`：`TriggerAfterBranchBeats` / `WarningSeconds` / `SafetyHoldSeconds` / `bToggle` / `TargetScheme`

示例关卡行（需在 Config 中手动配置）：

| Level | ForkPair | KeySwaps |
|---|---|---|
| T0 / L1 | AB | 无 |
| L2 | AC | 分支拍 ≥1、≥3 |
| L3 | BC | ≥1、≥2、≥4 |

---

## PIE 演示（推荐 L2 + C）

1. Host：`Bootstrap.ForkPair = AC`，并确认 `DA_Course` 已绑定 `DA_Rules`、`DA_Atoms` 和 A/C Branch Atom queues
2. PIE → 打完基础段 → **E 选 C**
3. 分支中出现 `KEY SWAP WARN` → Hold → 键位变为 **Q=Attack / E=Jump**（再触发会 toggle 回来）
4. 结束结果的 `RouteTaken=3`（C）；控制台可用 `Night.Course.ForceKeySwap`

控制台：`Night.Course.ForceKeySwap` / `Dump` / `ChooseRight`

---

## R1 契约增量

```text
INightFeelBridge::SetControlScheme(Normal|Swapped)
INightFeelBridge::GetControlScheme()
```

Stub 已实现物理键 remap。R1 替换 Feel 时需同样处理。

---

## 代码锚点

- `UNightG1CourseConfig::LevelRules` / `CourseRuleData.BranchRoutes[C]`
- `UNightCourseDirector` 换键状态机
- `UNightFeelStubComponent::SetControlScheme`
- `UNightForkController` 真 AC/BC
