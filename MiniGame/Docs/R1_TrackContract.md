# R1 交接：夜跑轨道设计思路 / 数据结构 / 示范轨构造

**给谁看：** R1（3C：角色 / 相机 / 控制 / 判定窗手感）  
**谁维护轨：** R2 Gameplay（本仓）  
**联调面：** 只通过 `INightFeelBridge` + `FNightJudgeRequest` / `NotifyFeelResolved`；昼夜只交 `FNightBootstrap` / `FNightResult`

对照：`Docs/G1_Course_Usage.md`、`Docs/FullFeature_ThreeDev_Split.md`、`.cursor/rules/blade-heart-stone-chain.mdc`

---

## 1. 设计思路（刃心石头链）

关卡不是「怪/障自己冲向角色」的卷轴，也不是「每个判定自带一对装饰平台」。

**实体是一串落脚石（石头）**，玩家站在当前石上，用 **Jump / Attack** 前进到下一石；不按键则角色、镜头、石头都不推进（action-driven）。

### 示例

`石头1` → `空` → `石头2` → `石头带怪3`

| 站位 | 前方 | 玩家按键 | 成功后 |
|---|---|---|---|
| 石头1 | 空档到石头2 | **Jump** | 落到石头2 |
| 石头2 | 石头3上有胶囊怪 | **Attack** | 冲到石头3，目标石上怪消失 |
| 石头3 | 末石 | — | 出口缓冲 → `FNightResult` |

### 语义映射（给 3C）

| 轨语义 | Feel 输入 | `ENightNodeKind`（历史名） |
|---|---|---|
| 空档 / 跳 | `ENightFeelInput::Jump` | `Hazard`（DisplayName=Jump） |
| 下一石带怪 / 砍 | `ENightFeelInput::Attack` | `Enemy`（DisplayName=Attack） |

相机约定：角色**后上方**第三人称跟拍（R1 可替换手感，但不要改成侧视卷轴）。

---

## 2. 实际数据结构（代码为准）

头文件：

- `Source/MiniGame/Public/Night/Course/NightCourseTypes.h`
- `Source/MiniGame/Public/Night/Course/NightG1CourseConfig.h`
- `Source/MiniGame/Public/Night/Course/NightFeelBridge.h`
- `Source/MiniGame/Public/Night/Shared/NightSharedTypes.h`

### 2.1 石头 `FNightStoneSpec`

| 字段 | 类型 | 含义 |
|---|---|---|
| `TrackDistance` | `float` | 沿 `TrackForward` 的轨距（cm） |
| `bHasFoe` | `bool` | 此石上是否站怪（胶囊白模） |
| `FoeId` | `EFoeId` | 怪 ID（砍石用） |
| `DropId` / `DropCount` | 掉落 | 成功 Attack 进此石时结算 |

### 2.2 拍子 `FNightBeatSpec`（站在 From → 按键 → 到 To）

| 字段 | 类型 | 含义 |
|---|---|---|
| `FromStoneIndex` | `int32` | 当前站立石 |
| `ToStoneIndex` | `int32` | 目标石 |
| `Action` | `ENightNodeKind` | `Hazard`=Jump，`Enemy`=Attack |

**关系：** `Stones.Num() == Beats.Num() + 1`。第 `i` 个拍子连接石 `i` → 石 `i+1`。

### 2.3 判定请求 `FNightJudgeRequest`（R2 → R1 Feel）

Director 打开拍子时 `Execute_NotifyJudgeRequest`：

| 字段 | 含义 |
|---|---|
| `NodeIndex` | **拍子下标**（BeatIndex，不是 StoneIndex） |
| `Kind` | 本拍要求的 `Jump/Attack`（Hazard/Enemy） |
| `FoeId` | 若目标石有怪则带上 |
| `NodeActor` | 目标石 Actor（可取表现锚点） |
| `WindowOpenTime` / `WindowCloseTime` | G1 stub 可长期开窗；R1 可改真窗宽 |

结算：Feel 里 `TryResolveInput` → 广播结果 → Host `NotifyFeelResolved(BeatIndex, Outcome)`。  
**注意：** BlueprintNativeEvent 接口禁止直接调 `TryResolveInput()`，用 `TryResolveInput_Implementation` 或 `Execute_TryResolveInput`。

### 2.4 Feel 契约 `INightFeelBridge`（R1 替换点）

```
NotifyJudgeRequest / ClearJudgeRequest
TryResolveInput(Jump|Attack) -> ENightJudgeOutcome
GetSoul / ApplySoulPenalty
PlaySuccessFeedback / PlayFailFeedback
```

G1 现用 `UNightFeelStubComponent`：Attack 对 Enemy、Jump 对 Hazard；错误键 = `WrongButton`。

### 2.5 配置 `UNightG1CourseConfig`

| 字段 | 默认意图 |
|---|---|
| `BeatCount` | 拍子数（石数 = BeatCount+1） |
| `JumpGapCm` | 跳拍中心距（大空档，如 420） |
| `KillGapCm` | 砍拍中心距（近石，如 160） |
| `AdvanceSpeed` | 按键后冲到下一石的插值速度 |
| `PatternOverride` | 空则 Jump/Attack 交替；可手填序列 |
| `TrackOrigin` / `TrackForward` | 轨坐标系 |
| `BuildCourse(OutStones, OutBeats)` | 生成石链 |

运行时：`UNightCourseDirector` 从 `DA_Course.FoeActorMap` 直接生成对应的
`ANightCourseStoneActor` Blueprint，按拍子开窗、前进。

---

## 3. 构造一条示范轨的简易方法

### 方法 A — 最快（改 DA / CDO，推荐联调）

1. 打开 `/Game/Night/Course/Config/DA_Course`（Host 不创建运行时默认 Config）
2. 在 `DA_Rules` 中配置动作队列，并确认 `DA_Course.FoeActorMap` 的 M01–M05
   都指向 `ANightCourseStoneActor` 子类 Blueprint。
3. PIE `/Game/Night/Course/Maps/L_Night_G1_ForkTest`：站石0 看 HUD
   `NOW: JUMP` 按 **Q** → 石1 `NOW: ATTACK` 按 **E** → …

对应你口头例子「石头1，空，石头2，石头带怪3」最少只要 **2 拍**：

- `BeatCount = 2`
- `PatternOverride = [Hazard, Enemy]`  
  → 石0(起) —空— 石1 —近+怪— 石2

### 方法 B — 代码里手写石链（自定义示范）

在临时调试处或 Config 子类里直接填数组（示意）：

```cpp
// Stones: 1 --gap-- 2 --close+foe-- 3
TArray<FNightStoneSpec> Stones;
TArray<FNightBeatSpec> Beats;

FNightStoneSpec S0; S0.TrackDistance = 0.f;                 Stones.Add(S0);
FNightStoneSpec S1; S1.TrackDistance = 420.f;               Stones.Add(S1);
FNightStoneSpec S2; S2.TrackDistance = 580.f; S2.bHasFoe = true;
S2.FoeId = EFoeId::M01; S2.DropCount = 1;                 Stones.Add(S2);

FNightBeatSpec B0; B0.FromStoneIndex = 0; B0.ToStoneIndex = 1; B0.Action = ENightNodeKind::Hazard; Beats.Add(B0); // Jump
FNightBeatSpec B1; B1.FromStoneIndex = 1; B1.ToStoneIndex = 2; B1.Action = ENightNodeKind::Enemy;  Beats.Add(B1); // Attack
```

也可用现成 `UNightG1CourseConfig::BuildCourse`，只改 `PatternOverride` / Gap。

### 方法 C — 编辑器点检

1. PIE，不按键：确认怪/石都不自己挪  
2. `Night.Course.Dump` 看 Host 状态  
3. Output Log 搜 `[NightCourse] OpenBeat` / `ResolveBeat`

---

## 4. R1 接入清单（最小）

1. 实现 `INightFeelBridge`（替换 FeelStub）  
2. `Director->BindFeelBridge(YourFeel)`（Host 已对 Pawn FeelStub 绑定，换组件即可）  
3. 输入：窗开时 Jump/Attack → `Execute_TryResolveInput` → 结果回 `NotifyFeelResolved`  
4. 窗宽/魂灯/相机手感归 R1；**石距与拍子序列归 R2**（读 Config，勿在 Feel 里改轨距）  
5. 判定成功/失败反馈可挂 `PlaySuccessFeedback` / `PlayFailFeedback`

有问题对：`Night/Course/` + 本文 + `G1_Course_Usage.md`。
