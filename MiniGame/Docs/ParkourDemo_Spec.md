# 双键跑酷 Demo 规格（参考刃心）

> 状态：Demo / 白模  
> 视角：第三人称竖屏  
> 操控：仅左跳 / 右攻（无摇杆自由移动）

---

## 1. 一句话目标

玩家在竖屏手机上，只使用**跳跃**与**进攻**两个按钮，沿单轨道推进：遇坑大跳、遇敌轻砍并小幅前移；错键受罚；通关或死亡可重开。

---

## 2. 玩法定稿

| 项 | 定稿 |
|---|---|
| 前进方式 | **无自动奔跑主循环**；仅成功跳/攻产生沿轨道位移 |
| 跳跃 | 大幅向前跃迁（过 Gap） |
| 进攻 | 击杀判定窗内敌人 + 小幅向前 |
| 错键 | Gap 时攻 / Enemy 时跳 → 失误 |
| 漏键 | 越过判定窗未正确操作 → 失误或坠落 |
| 表现 | 白模 + 模板免费动画占位 |
| 本阶段不做 | 摇杆移动、墙跑、能量/符文、多角色、无尽、精美特效 |

### 核心循环

```
读前方事件 → 按跳或攻 → 成功则位移(+连击) → 失败则惩罚 → 直至终点/死亡
```

---

## 3. 数值初值（可调）

| 参数 | 符号 | 初值 | 说明 |
|---|---|---|---|
| 跳跃前进距离 | `JumpForward` | 600 | cm，沿轨道 |
| 跳跃高度 | `JumpHeight` | 280 | cm，抛物线峰值 |
| 进攻前进距离 | `AttackForward` | 120 | cm，沿轨道 |
| 判定窗半宽 | `JudgeHalfWidth` | 80 | cm，事件点前后 |
| 输入缓冲 | `InputBufferTime` | 0.12 | 秒，提前按仍有效 |
| 最大生命 | `MaxHP` | 3 | Demo：错键 -1，到 0 死亡 |
| 连击 | `Combo` | 0+ | 仅 UI；正确操作 +1，失误清零 |

> 落地后进度 `Distance` 对齐到轨道栅格，避免跳距过大穿过多事件。

---

## 4. 事件类型

| Type | 正确键 | 成功效果 | 错误效果 |
|---|---|---|---|
| `Gap` | Jump | 大跳过坑 | Attack → 失误；漏 → 掉落/失误 |
| `Enemy` | Attack | 杀敌 + 小步前进 | Jump → 失误；漏 → 撞敌/失误 |
| `Goal` | （到达即胜） | Win UI | — |

可选后做：`Safe`（空步，无需按键）。

---

## 5. 数据结构（与代码对齐）

- `URunnerTrackData`（DataAsset）：一关的 `Events[]`
- `FRunnerTrackEvent`：`Distance`（cm）、`Type`、可选 `EnemyClass`
- `URunnerFlowComponent`：当前 `Distance`、最近事件、判定状态、HP/Combo
- `ARunnerCharacter`：执行 Jump/Attack 位移，**不绑定 Move**
- `ARunnerPlayerController`：Enhanced Input → Jump/Attack
- UI：左半/左钮 Jump，右半/右钮 Attack（竖屏 9:16）

内容建议路径：

- `/Game/Runner/Data/` — Track DataAsset
- `/Game/Runner/Blueprints/` — BP_RunnerCharacter 等薄壳
- `/Game/Runner/UI/` — WBP_RunnerHUD
- `/Game/Runner/Maps/` — L_RunnerDemo_Whitebox

---

## 6. 竖屏与移动端

- 锁定 Portrait；禁止横屏
- 相机：跟拍、略近略高，保证能看清前方 2–3 个事件
- 白模关卡偏**纵向推进**，少超宽横向
- Demo 可用 PC 预览；验收以竖屏比例 + 触控热区为准

---

## 7. 验收（M5）

1. 仅双钮可玩完一关  
2. 跳明显比攻「窜得更远」  
3. 攻能清敌并微前移  
4. 错键有反馈（闪红/断连击/扣血）  
5. 约 30–45 秒白模关可通关或死亡重开  

---

## 8. 开发阶段（摘要）

| Phase | 内容 |
|---|---|
| 0 | 竖屏、双钮、禁用摇杆 |
| 1 | Track Data + Flow 读事件 |
| 2 | 跳大步 / 攻小步位移 |
| 3 | 判定、敌人、失败 |
| 4 | HUD + 一整关 |
| 5 | 手感与真机 |

---

## 9. 资源策略

- 使用：Third Person / Manny 免费动画占位  
- 可选：Game Animation Sample **单条** Migrate  
- **不买**商城完整 Parkour / 双摇杆模板（本阶段）
