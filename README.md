# MiniGame（UE + GitHub / UGit 模板）

本目录已初始化为 **Unreal Engine 项目用的 Git 仓库模板**（含 `.gitignore`、Git LFS `.gitattributes`）。

本地 Git 客户端请使用 **UGit**（当前机器路径）：

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

---

玩法提醒：关卡是一串**落脚石**（空档=跳，下一石带怪=砍），不是一对对装饰平台。错键不前进。

策划可用浏览器打开 `MiniGame/Tools/NightCourseDesigner.html` 俯视调参并导出 JSON；PIE：`Night.Course.ImportParams <json>`。

### 1. 用 Epic Games Launcher 创建项目到本目录

1. 打开 **Epic Games Launcher** → **Unreal Engine** → **库** → 启动你要用的引擎版本（建议团队统一，例如 5.4 / 5.5）。
2. 选 **游戏** → 模板（入门可用 **空白** / **第三人称**，蓝图即可）。
3. 项目设置：
   - **项目位置**：选 `C:\Users\moonyfli\Desktop`
   - **项目名称**：填 `MiniGame`（必须与现有文件夹名一致）
   - 若提示文件夹非空：选择 **使用现有文件夹** / 继续（保留已有的 `.git`、`.gitignore`、`.gitattributes`、`README.md`）
4. 创建完成后，本目录应出现：
   - `MiniGame.uproject`
   - `Content/`
   - `Config/`
   - （C++ 项目还会有 `Source/`）

更细的玩法说明在 `MiniGame/Docs/`（G1 石链 / G2 岔口 / G3 换键 / G3.5 程序化）。
