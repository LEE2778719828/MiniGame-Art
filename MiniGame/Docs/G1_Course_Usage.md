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
| `/Game/Night/Course/Maps/L_Night_G1` | G1 关卡 |
| `/Game/Night/Course/Blueprints/BP_NightCourseGameMode` | 开局刷 Host |
| `/Game/Night/Course/Blueprints/BP_NightCoursePawn` | 后上方相机 + Feel stub；Q/E |
| `/Game/Night/Course/Blueprints/BP_NightCourseHost` | Director 宿主 |
| `/Game/Night/Course/Blueprints/BP_NightCourseStone`（或运行时 C++ 石） | 落脚石；可选怪胶囊 |
| `/Game/Night/Course/Data/DA_NightG1_T0` | 间距/石链配置 |
| `/Game/Night/Course/Input/IA_NightJump` / `IA_NightAttack` + `IMC_NightCourse` | Q=跳，E/LMB=劈 |

默认图：`L_Night_G1`（`DefaultEngine.ini`）。

## 怎么玩（PIE）

1. 打开 `L_Night_G1` → PIE  
2. 看 HUD：`NOW: JUMP` / `NOW: ATTACK` → 按 Q / E  
3. 控制台：`Night.Course.Dump` / `SkipToExit` / `Finish 1`

## 数据语义（C++）

- `FNightStoneSpec`：一块石的轨距、是否带怪、掉落  
- `FNightBeatSpec`：站在 From 石、按 Jump/Attack、前进到 To 石  
- Generator：按「跳空 / 砍怪」模式拉出石链（跳=大间距无怪；砍=小间距+目标石带怪）

## 美术对接

| 对象 | 接口 |
|---|---|
| 石 Actor | 平台 Mesh；`bHasFoe` 时显示胶囊；Success 砍怪播消失/受击 |
| `BP_NightCoursePawn` | `ArtRoot` 下挂角色 |
| `DA_NightG1_T0` | `JumpGapCm` / `KillGapCm` / 石间距 |

## R1 替换

实现 `INightFeelBridge`，`Director->BindFeelBridge`；Jump↔Hazard，Attack↔Enemy。

## 文档 / 规则

- 本文件 + `Docs/R2_Gameplay_Plan.md` Phase G1  
- Cursor 规则：`.cursor/rules/blade-heart-stone-chain.mdc`
