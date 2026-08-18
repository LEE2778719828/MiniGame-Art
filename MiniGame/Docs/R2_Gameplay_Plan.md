# R2 跑酷 Gameplay 计划（≈ Gameplay / 关卡内容）

**主责：** 你（本仓 Gameplay）  
**对照：** `FullFeature_ThreeDev_Split.md` 中的 **R2**  
**不负责：** R1 判定窗公式/3C 手感本体；S 白天 Merge/库存/选礼 UI/总 Flow（只消费 Bootstrap、回传 Result）

---

## 0. 目标与边界

### 目标（夜关可玩闭环）

在竖屏夜关内完成：

`基础段 → 唯一岔口（AB/AC/BC）→ 所选分支 → 出口缓冲 → FNightResult`

含：节拍刷怪障、A/B/C 规则、雾可见、换键**时点与停刷**、掉落与带出增益、谢礼夜生效、打包结果给 S。

### 硬边界

| 归 R2 | 不归 R2（联调接口） |
|---|---|
| 轨道/节拍生成、岔路状态机 | 跳/劈算不算 → **R1** |
| 怪/障 Spawn、掉落数量结算 | 灶魂加减公式实现 → **R1**（R2 上报事件） |
| 路线牌 UI、夜关内 HUD（岔口/雾提示） | 左右键触控壳可共用，换键**映射** → **R1** |
| `DT_NightLevels` / `DT_Foes` 执行 | `FNightBootstrap` 由 **S** 注入；库存规则由 **S** |
| 读 `GiftBuffState` 夜生效 | 选哪两件礼 → **S** |

### 与现有骨架

当前 `Source/MiniGame/.../Runner/*` 是 **R1+R2 早期揉在一起** 的白模。R2 计划以 **抽离 Course 模块** 为主，不一次性推翻工程：

- 保留可编译的 `RunnerCharacter` 等给 R1 演进  
- R2 新增 `Night/Course/` 下类型与组件，逐步把「刷事件/岔路」从 `RunnerFlowComponent` 迁出  

Content 根：`/Game/Night/Course/`  
代码根：`Source/MiniGame/Public|Private/Night/Course/`

---

## 1. 交付物清单

### 1.1 运行时（C++ 优先）

| 模块 | 职责 |
|---|---|
| `UNightCourseDirector` | 夜局状态机：Bootstrap → Running → Fork → Branch → Exit → Result |
| `UNightTrackGenerator` | 按 `T_beat`/段参数生成可操作节点（怪/障交替约束） |
| `UNightForkController` | 唯一岔口、左右选路、超时保底、不计失误 |
| `UNightRouteRules` | A/B/C：可见块、扣血修正、DoT、掉料、掉落节奏、带出增益 |
| `ANightFoeBase` + M01–M05 | 一击、掉落 ID、命中回调 |
| `ANightHazardBase` | 断路/地刺等障碍 |
| `UNightGiftApplier` | 纸鸢信息、借命、定键、饕餮前 4 怪 |
| `FNightBootstrap` / `FNightResult` | 与 S 的唯一跨昼夜结构（可放 Shared） |

### 1.2 数据

| 资产 | 内容 |
|---|---|
| `DT_NightLevels` | T0/L1/L2/L3 时长、岔口时点、速度、窗宽（供 R1 读的配置行）、节点估算、Seed |
| `DT_Foes` | M01–M05、掉落、权重、颜色锚点、解锁 |
| `DT_RouteRules` | A/B/C 表（可见块、扣血、DoT、倍率、掉落循环） |
| `DA` 或行内 | 评审固定：T0 AB、L1 AB、L2 AC、L3 BC |

### 1.3 内容 / UI（薄 BP）

- `BP_Foe_M0x`、`BP_Hazard_*`  
- `WBP_ForkCards`（左右路线牌）  
- 夜关 Map：`L_Night_T0` 白模轨（可先一条测全流程）  

---

## 2. 阶段计划（建议 3 周可评审向）

### Phase G0 — 目录与契约（0.5–1 天）

- [ ] 建 `Night/Course` 代码目录与 `/Game/Night/Course`  
- [ ] 定稿 `FNightBootstrap` / `FNightResult` / `EIngredientId` / `EFoeId` / `ERouteId`（与 S 对齐枚举）  
- [ ] 写 `INightCourse`：`StartNight` / `OnNightFinished`  
- [ ] 文档：接口字段表（本计划 §4）冻结一版  

**完成标准：** S（或本地 Mock）能调用空夜局并立刻收到空 Result。

### Phase G1 — 直线节拍关（2–3 天）✅ 已落地（石头链语义）

- [x] 无岔口：只跑「基础段」+ 出口缓冲  
- [x] **刃心石头链**：落脚石序列；`空`→当前石 Jump；`下一石带怪`→当前石 Attack；成功后前进到目标石，怪从目标石消失  
- [x] 禁止「每个判定自带一对互不相关双平台」的旧白盒模型  
- [x] 与 R1 约定：`INightFeelBridge` + G1 `UNightFeelStubComponent`（Jump↔Hazard，Attack↔Enemy）  
- [x] 成功劈怪 → 累加掉落；失败 → Soul 惩罚  
- [x] 出口缓冲 → `FNightResult`  
- [x] 调试：`Night.Course.Dump|SkipToExit|Finish`；Director 多播  
- [x] 引擎资产：`/Game/Night/Course/**` + `L_Night_G1`  
- [x] 后上方第三人称；**action-driven**（不按键不推进）  

**完成标准：** PIE `L_Night_G1`，站石按 Q/E 前进；结束 Log 有 Ingredients。  
用法：`Docs/G1_Course_Usage.md`  
规则记忆：`.cursor/rules/blade-heart-stone-chain.mdc`  

### Phase G2 — 唯一岔口 + A/B 规则（3–4 天）✅ 已落地（石链 + AB）

- [x] Fork：左右牌（HUD Canvas）、左键左路右键右路、2.4s 超时保底  
- [x] 进入分支后不再二次岔口；1.2s 纯跑缓冲（自动跳上分支首石）  
- [x] A 清途 vs B 瘴途：可见块、扣血倍率、蚀火 DoT、进分支掉 1 份料、+20% 带出  
- [x] 雾：只改 VisibleBlockCount，不改速度/窗  
- [x] `FNightResult.RouteTaken` 填 A/B；控制台 `ChooseLeft|ChooseRight|SkipFork`  

**完成标准：** T0/L1 风格 AB 可玩；走 B 掉料与带出肉眼有别于 A。  
用法：`Docs/G2_Course_Usage.md`  

### Phase G3 — C 倒途 + 换键编排（2–3 天）✅ 已落地

- [x] C 规则：逆火（前进中 DoT）、掉 2 份、掉落循环、+30% 带出  
- [x] 换键时点表（L2 两次、L3 三次）；安全窗停开拍；调用 R1 `SetControlScheme`  
- [x] AC / BC 组合按 `LevelRows`；关卡布局/规则全面可调  
- [x] KeyCoin 跳过首次换键钩子；`Night.Course.ForceKeySwap`  

**完成标准：** L2 推荐 C 路径可演示完整换键；与 R1 联调 Feel remap。  
用法：`Docs/G3_Course_Usage.md`  

### Phase G3.5 — 桥段程序化 + HTML 设计器（插入；先于 G4）✅ 已落地

- [x] WrongButton 不推进、不消耗拍子（跳/砍语义正确）
- [x] `FNightProcCourseParams` + `UNightTrackGenerator`（Seed、Yaw0.1°、岔路区间、换键频率）
- [x] 石 WorldPose + `ANightBridgeSegmentActor` 桥板拼接
- [x] 美术 SoftRef：Bridge/Hero/Foe（分类后的 `ArtSubmit/` → `/Game/Night/Course/Art/**`）
- [x] `UNightProcParamsAsset` JSON 导入；`Night.Course.ImportParams`
- [x] `Tools/NightCourseDesigner.html` 俯视预览 / 导出

**完成标准：** 同 Seed 可复现；HTML 导出 JSON 可被 UE 导入。
文档：`Docs/G3_5_ProcCourse_Plan.md`、`Docs/NightProcParams_Schema.md`

### Phase G4 — 五怪五料 + 优势池（2–3 天）

- [ ] M01–M05 白模差异（色/缩放）+ 掉落 ID  
- [ ] 分支优势食材 70/30；基础段基础权重  
- [ ] 带出增益打在分支段掉落总量上  
- [ ] 预留 TA：命中 Niagara、瘴雾材质槽  

**完成标准：** 同 Seed 下 A/B/C 期望结构可对表（允许近似）。

### Phase G5 — 谢礼夜生效 + 关卡表灌满（2 天）

- [ ] 引路纸鸢：岔口显示优势/长度/倍率（无纸鸢则隐藏）  
- [ ] 借命：B/C 首次失误免扣与免掉料  
- [ ] 定键：选 C 时取消首次换键；预警 0.8→1.2  
- [ ] 饕餮：分支前 4 怪定向  
- [ ] `DT_NightLevels` 填 T0–L3 评审行 + 固定 Seed  

**完成标准：** Mock Bootstrap 带两件礼，夜侧行为可测；Result 交给 S Mock。

### Phase G6 — 打磨与失败态（1–2 天）

- [ ] 魂灭停生成、焦纸（调 R1）、Result `bFailedMidway`  
- [ ] 生成约束自检（同类连续、同线怪障不同时）  
- [ ] 移动端预览帧时：生成与 Actor 池化（简单复用）  

---

## 3. 每周节奏（示例）

| 周 | 焦点 | 联调 |
|---|---|---|
| W1 | G0–G1 | 与 R1 对接判定线事件（每日一次短联调） |
| W2 | G2–G3 | 与 R1 换键；S 尚未就绪则本地 Mock Bootstrap |
| W3 | G4–G6 | 与 S 真 Bootstrap/Result；TA 接雾/命中 |

---

## 4. 冻结接口草案

### FNightBootstrap（S → R2）

| 字段 | 说明 |
|---|---|
| `LevelId` | T0/L1/L2/L3 |
| `ForkPair` | AB/AC/BC |
| `GiftBuffState` | 四礼开关与参数 |
| `FoeWeightOverride` | 可选定向 |
| `Seed` | 评审种子 |

### FNightResult（R2 → S）

| 字段 | 说明 |
|---|---|
| `bSuccess` | 是否出口 |
| `RouteTaken` | A/B/C/None |
| `Ingredients` | 最终入库候选列表 |
| `SoulLeft` | 收口灶魂 |
| `bFailedMidway` | 中途魂灭 |

### R2 ↔ R1（夜内）

| 方向 | 事件 |
|---|---|
| R2→R1 | `JudgeWindowOpen(Gap\|Enemy, Window)` |
| R1→R2 | `JudgeResolved(Success\|Wrong\|Miss)` |
| R2→R1 | `SetControlScheme` / `RequestSoulPenalty` / `PlayCharredPaper` |
| R1→R2 | `GetSoul()` 只读 |

---

## 5. 目录约定（清理后目标）

```text
Source/MiniGame/
  Public/Night/Course/     # R2 主责
  Public/Night/Feel/       # R1 主责（你只读）
  Public/Night/Shared/     # Bootstrap/Result/枚举
  Public/Day/              # S（你不写）
Content/Night/Course/
  Data/ Blueprints/ UI/ Maps/ FX/
```

现有 `Runner/`：**过渡期共存**；迁完 Course 后在计划里勾「删除 Flow 内生成逻辑」，避免双生成器。

---

## 6. TA 请你开口的时机

| 时机 | 请 TA |
|---|---|
| G2 完 | 瘴雾可见 |
| G4 中 | 五怪命中 Niagara 模板 |
| 不提前 | 勿阻塞 G0–G1 白模 |

---

## 7. 今日即可开始的第一件事

1. 冻结 §4 三个结构体字段（与 R1/S 各确认 15 分钟）。  
2. 落地空 `UNightCourseDirector::StartNight` Mock。  
3. 从当前 `RunnerFlowComponent` 列出「生成/岔路」职责清单，标迁出顺序（不要先大改 R1 角色）。  

需要 Agent 下手写代码时：从 **G0 契约 + Director 空壳** 开始即可。
