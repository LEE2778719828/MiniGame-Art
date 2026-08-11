# 开发计划索引

分工总表：[`FullFeature_ThreeDev_Split.md`](./FullFeature_ThreeDev_Split.md)  
- **R1 ≈ 3C**｜**R2 ≈ Gameplay**｜**S ≈ 经营**

## 本仓当前主责：R2 Gameplay

执行计划：[`R2_Gameplay_Plan.md`](./R2_Gameplay_Plan.md)

## 仓库清理说明（本次）

| 动作 | 说明 |
|---|---|
| 删除 | `Character_ThreeTrack_Split.md`（旧人物三分，已废止） |
| 保留 | `FullFeature_ThreeDev_Split.md`（现行三人分工） |
| 降级参考 | `ParkourDemo_Spec.md` / `EditorSetup_Phase0.md`（早期 Demo） |
| 新增 | `Docs/README.md`、`R2_Gameplay_Plan.md` |
| 代码 | `Source/.../Runner/*` 暂留作 R1+R2 过渡白模，按 R2 计划迁入 `Night/Course` |

## 工程骨架（已有）

| 项 | 路径 |
|---|---|
| 模块 | `Source/MiniGame/` |
| 早期 Runner | `Public|Private/Runner/` |
| 竖屏 Android 默认 | `Config/DefaultEngine.ini` |
| Cursor | `.cursor/rules/`、`.cursor/skills/ue-runner-editor-ops/` |
