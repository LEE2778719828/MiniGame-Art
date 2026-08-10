# MiniGame（UE + GitHub / UGit 模板）

本目录已初始化为 **Unreal Engine 项目用的 Git 仓库模板**（含 `.gitignore`、Git LFS `.gitattributes`）。

本地 Git 客户端请使用 **UGit**（当前机器路径）：

`C:\Users\moonyfli\AppData\Local\UGit\app-1.0.9\resources\app\git\cmd\git.exe`

---

## 你需要在 UE / Epic 里完成的步骤

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

### 2. 第一次在编辑器里保存

1. 双击 `MiniGame.uproject` 打开。
2. 若提示缺少模块 / 编译：按提示用 Visual Studio 编译（纯蓝图项目通常可直接进）。
3. **文件 → 保存全部**（确保默认关卡 / 资源落盘到 `Content/`）。
4. 关闭编辑器（可选，方便 UGit 扫文件更干净）。

### 3. 用 UGit 做首次纳入版本库

1. 打开 **UGit** → **打开仓库** → 选 `C:\Users\moonyfli\Desktop\MiniGame`。
2. 确认左侧能看到新增的 `MiniGame.uproject`、`Content/`、`Config/` 等。
3. **不要**提交 `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`（应被 `.gitignore` 挡住）。
4. 暂存全部应跟踪文件 → 提交，例如信息：`chore: add initial UE project files`。
5. 在 UGit 里添加 GitHub 远程（或网页建好空仓库后粘贴 URL）→ **Push**。

> 若 GitHub 上还没有远程仓库：先在 GitHub 新建空仓库（不要勾选自动 README），再在 UGit 里填 `origin` URL 后推送。

### 4. 协作注意（LFS）

- `.uasset` / `.umap` 走 **Git LFS**；同事也必须用支持 LFS 的客户端（UGit 已带 LFS）。
- 多人改同一蓝图/关卡前，建议在 UGit / Git LFS 里对文件 **Lock**，改完再 Unlock。

---

## 目录约定（应进仓库）

| 路径 | 说明 |
|------|------|
| `*.uproject` | 工程入口 |
| `Content/` | 资源与关卡 |
| `Config/` | 默认配置 |
| `Source/` | C++ 源码（若有） |
| `Plugins/` | 项目插件（若有） |
| `.gitignore` / `.gitattributes` | 忽略规则与 LFS |

## 不应进仓库

`Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`、`.vs/`、本地 `*.sln` 等。
