# MiniGame-Art（夜跑白模工程 · 外网）

从内网 `MiniGame` 同步的 UE 5.8 工程，给美术 / 外网协作打开关卡与白模。

- 工程入口：`MiniGame/MiniGame.uproject`
- 默认夜关：`/Game/Night/Course/Maps/L_Night_G1`
- 玩法说明：`MiniGame/Docs/`（G1 石链、G2 岔口、G3 换键）

## 打开方式

1. 用支持 **Git LFS** 的客户端拉仓（UGit 已带 LFS）。
2. 用 Unreal Engine **5.8** 打开 `MiniGame/MiniGame.uproject`。
3. 首次会编译 C++ 模块；不要提交 `Binaries/` / `Intermediate/` / `Saved/`。

## 应进仓库

`MiniGame/Content/`、`Config/`、`Source/`、`Docs/`、`*.uproject`

## 不应进仓库

`Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`、本地插件与反馈目录。
