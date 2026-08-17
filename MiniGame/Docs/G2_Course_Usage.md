# G2 唯一岔口 + A/B 规则 — 使用说明

在 G1 刃心石头链之上：基础段结束后进入**唯一岔口**，选 A 清途 / B 瘴途，再跑分支石链，出口缓冲回传 `FNightResult.RouteTaken`。

对照：`Docs/G1_Course_Usage.md`、`Docs/R2_Gameplay_Plan.md` Phase G2、`Docs/R1_TrackContract.md`

---

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

## 距离淡出（雾白模，锚点=角色）

石块 / 怪相对**跑者 Pawn** 越远越透明（半透明材质 `M_NightUnlitFade`）。

可调：`UNightG1CourseConfig::DistanceFade`（Category `Night|Art|DistanceFade`）

| 参数 | 默认意图 |
|---|---|
| `bEnabled` / `bUpdateEveryTick` | 开 / 每帧刷新（冲刺时更顺） |
| `DistanceSpace` | `TrackDistance`（沿轨）；可选 World3D / HorizontalXY |
| `FadeStartCm` / `FadeEndCm` | 此内不透明 → 外端趋近 MinOpacity |
| `SoftFalloffExtraCm` / `FadePower` | 额外软边 / 曲线指数 |
| `MinOpacity` / `MaxOpacity` / `OpacityMul` | 透明度范围与总强度 |
| `bScaleEndByVisibleBlocks` | B 路 VisibleBlock 少时拉近 FadeEnd |
| `bCombineWithVisibleBlockCull` / `SoftCullExtraBlocks` | 与 G2 可见块硬窗叠加 |
| `HideBelowOpacity` / `bHideWhenBelowThreshold` | 低于阈值直接 Hidden（省 draw） |
| `bKeepPastStonesOpaque` | 身后落脚石保持不透明 |
| `bAffectPlatform` / `bAffectFoe` | 平台 / 怪胶囊分别开关 |
| `OpacityParamName` / `FadeAlphaParamName` / `ColorParamName` | MID 参数名（对接美术材质） |
| `bWriteAnchorToMpc` | 预留：以后把锚点写入 MPC |

材质：`/Game/Night/Course/Materials/M_NightUnlitFade`（`Color` / `Opacity` / `FadeAlpha`）。脚本：`Tools/create_night_fade_material.py`。

---

## A / B 白模数表

| 字段 | A 清途 | B 瘴途 |
|---|---|---|
| VisibleBlockCount | 8 | 3（只压可见，不改速度/窗） |
| SoulPenaltyScale | 1.0 | 1.25 |
| DotSoulPerSecond | 0 | 2.0 |
| EnterDropCount | 0 | 1 |
| CarryOutBonus | 0 | +20%（分支掉落向上取整） |

代码：`UNightRouteRulesAsset::MakeDefaultRule` / 可选 `Config->RouteRules`。

---

## PIE 步骤

1. 打开 `/Game/Night/Course/Maps/L_Night_G1`（同图；`bEnableFork=true` 默认开）
2. 打完基础段 → HUD 出现 `FORK` 与 A/B 牌 → **Q 选 A** 或 **E 选 B**
3. 走 B：前方石更少、魂持续掉、进分支多 1 料、结束带出更多
4. Log：`[NightCourseHost] Finished ... route=1|2`

### 控制台

| 命令 | 作用 |
|---|---|
| `Night.Course.Dump` | 相位 / Route / Fork |
| `Night.Course.ChooseLeft` / `ChooseRight` | 强制选路 |
| `Night.Course.SkipFork` | 默认左路 |
| `Night.Course.SkipToExit` / `Finish` | 同 G1 |

### Config 关键字段（`UNightG1CourseConfig`）

- `bEnableFork`、`ForkTimeoutSeconds`、`BranchEnterBufferSeconds`
- `BranchABeatCount` / `BranchBBeatCount`（B 默认更密 Attack）
- `BeatCount` = **仅基础段**拍数

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
