# MiniGame-Art（美术提交仓）

UE 5.8 夜跑白模工程。美术**只往 `ArtSubmit/` 交源文件**，不要改 `MiniGame/Source/`。

## 你要交到哪里（看这一张表）

| 你做的东西 | 丢进这个文件夹 | 程序最终接到 |
|---|---|---|
| **主角**（夜形态、跳/砍姿势参考） | `ArtSubmit/Character/` | `BP_NightCoursePawn` 的 `ArtRoot` |
| **落脚石**（平台白模→正模） | `ArtSubmit/Stones/` | 石 Actor 平台 Mesh |
| **怪**（胶囊替代、一击受击） | `ArtSubmit/Foes/` | 目标石上的怪；`M01`–`M05` 分文件 |
| **障碍 / 空档表现** | `ArtSubmit/Hazards/` | 大间距跳的视觉（断路、缝） |
| **场景 / 雾 / 瘴气** | `ArtSubmit/Environment/` | B 路「只压可见」雾皮 |
| **特效**（砍中、怪消失、掉料） | `ArtSubmit/VFX/` | 石 Actor：`PlaySlashVFX` / `PlayFoeClearedVFX` / `PlayDropBurst` |
| **UI**（岔口左右牌、键位图标） | `ArtSubmit/UI/` | HUD 岔口牌；换键图标 |
| **还没分类** | `ArtSubmit/_incoming/` | 程序再分拣 |
| **本批已归档白模** | `ArtSubmit/Character/`、`ArtSubmit/Stones/Bridge/`、`ArtSubmit/Foes/`、`ArtSubmit/Environment/` | 导入 `/Game/Night/Course/Art/**`，见 `ArtSubmit/README.md` |

文件名建议：`角色_夜_Idle.fbx`、`石头_落脚_01.fbx`、`怪_M01.fbx`。源文件（fbx / png / psd）直接放对应子目录即可。

玩法提醒：关卡是一串**落脚石**（空档=跳，下一石带怪=砍），不是一对对装饰平台。错键不前进。

策划可用浏览器打开 `MiniGame/Tools/NightCourseDesigner.html` 俯视调参并导出 JSON；PIE：`Night.Course.ImportParams <json>`。

## 打开工程（可选，给要进编辑器的人）

1. 用带 **Git LFS** 的客户端拉仓（UGit 即可）。
2. UE **5.8** 打开 `MiniGame/MiniGame.uproject`。
3. 默认关卡：`/Game/Night/Course/Maps/L_Night_G1`
4. 不要提交 `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`。

更细的玩法说明在 `MiniGame/Docs/`（G1 石链 / G2 岔口 / G3 换键 / G3.5 程序化）。
