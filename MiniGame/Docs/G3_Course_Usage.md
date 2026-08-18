# G3 C 倒途 + 换键编排 — 使用说明

在 G2 岔口之上：启用 **AC/BC**、完整 **C 路线规则**（逆火 / 掉 2 / 掉落循环 / +30% 带出），以及按关卡表触发的 **换键**（预警 → 安全窗停拍 → `SetControlScheme`）。

对照：`Docs/G2_Course_Usage.md`、`Docs/R2_Gameplay_Plan.md` Phase G3

---

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
| 换键 | L2 默认 2 次、L3 默认 3 次；仅 C（可配） |
| 定键钩子 | `GiftBuffs.bKeyCoin` + `bHonorKeyCoinSkipFirstSwap` 跳过首次换键 |

---

## 关卡完全可调参数（Config）

`UNightG1CourseConfig` 主要旋钮：

| 分类 | 字段 |
|---|---|
| 基础段 | `BeatCount` / `JumpGapCm` / `KillGapCm` / `PatternOverride` / `JudgeWindowSeconds` / `AdvanceSpeed` |
| 岔口 | `bEnableFork` / `ForkTimeoutSeconds` / `bForkTimeoutPickLeft` / `BranchEnterBufferSeconds` / `BranchEntryGapCm` |
| 分支布局 | `BranchA/B/C`（`FNightBranchLayoutSettings`：拍数、Pattern、独立间距、掉落） |
| 路线规则 | `RouteRules` 或默认 `MakeDefaultRule`（可见块、倍率、DoT、逆火、进分支掉料、循环、带出） |
| 关卡表 | `LevelRows`：`ForkPair` + `KeySwaps[]` + `bKeySwapOnlyOnRouteC` |
| 换键全局 | `bEnableKeySwap` / `DefaultKeySwapWarningSeconds` / `DefaultKeySwapSafetySeconds` |
| 雾 | `DistanceFade` + `DistanceFadeMaterial` |

`FNightKeySwapCue`：`TriggerAfterBranchBeats` / `WarningSeconds` / `SafetyHoldSeconds` / `bToggle` / `TargetScheme`

默认关卡行：

| Level | ForkPair | KeySwaps |
|---|---|---|
| T0 / L1 | AB | 无 |
| L2 | AC | 分支拍 ≥1、≥3 |
| L3 | BC | ≥1、≥2、≥4 |

---

## PIE 演示（推荐 L2 + C）

1. Host：`Bootstrap.LevelId = L2`，`ForkPair = AC`（或勾选 `bApplyLevelTableOnStart`）
2. PIE → 打完基础段 → **E 选 C**
3. 分支中出现 `KEY SWAP WARN` → Hold → 键位变为 **Q=Attack / E=Jump**（再触发会 toggle 回来）
4. Log：`KeySwap APPLIED`；结束 `route=3`（C）

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

- `UNightG1CourseConfig::LevelRows` / `BranchC`
- `UNightCourseDirector` 换键状态机
- `UNightFeelStubComponent::SetControlScheme`
- `UNightForkController` 真 AC/BC
