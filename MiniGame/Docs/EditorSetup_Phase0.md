# Phase 0 编辑器操作清单（双键跑酷 Demo）

配合文档：`Docs/ParkourDemo_Spec.md`  
C++ 骨架已在 `Source/MiniGame/Public|Private/Runner/`。

首次打开若提示编译模块：选 **Yes**，用 VS/Rider 编过后再进编辑器。

---

## 1. 启用插件

1. **编辑 → 插件**
2. 确认 **Enhanced Input** 已启用
3. 确认 **NexusLink**（Developer）已启用，并在 **编辑器偏好设置 → Plugins → NexusLink** 勾选 **Enable MCP Server**

---

## 2. 创建内容文件夹

内容浏览器新建：

```
Content/Runner/
  Data/
  Blueprints/
  Input/
  UI/
  Maps/
```

---

## 3. Enhanced Input（仅两键，不要摇杆）

1. 在 `Content/Runner/Input/` 右键 → **输入 → 输入操作**
   - `IA_RunnerJump`
   - `IA_RunnerAttack`
2. 右键 → **输入 → 输入映射上下文** → `IMC_Runner`
3. 打开 `IMC_Runner`：
   - 添加映射：`IA_RunnerJump` → Keyboard **Q**（或 Left）
   - 添加映射：`IA_RunnerAttack` → Keyboard **E**（或 Right）
4. **不要**添加 WASD / 摇杆 Move（本玩法禁止自由移动）

触控：Phase 4 用 Widget 按钮调用 `RequestJump` / `RequestAttack`，不必用 Default Virtual Joysticks。

---

## 4. 蓝图薄壳

1. 右键 `ARunnerCharacter`（C++ 类）→ **基于 RunnerCharacter 创建蓝图类** → `BP_RunnerCharacter`
2. 在 Class Defaults 指定：
   - `Default Mapping Context` = `IMC_Runner`
   - `Jump Action` = `IA_RunnerJump`
   - `Attack Action` = `IA_RunnerAttack`
3. 同理可选：`BP_RunnerGameMode`（基于 `ARunnerGameMode`）、`BP_RunnerPlayerController`

---

## 5. 第一条测试轨 DataAsset

1. 内容浏览器右键 → **杂项 → 数据资产** → 选 `RunnerTrackData` → `DA_RunnerDemo_01`
2. 按距离填若干事件，例如：

| Distance | Type |
|---|---|
| 200 | Enemy |
| 500 | Gap |
| 800 | Enemy |
| 1100 | Gap |
| 1400 | Goal |

3. 数值保持 Spec 初值，或按手感微调 JumpForward / AttackForward

---

## 6. 关卡与 GameMode

1. **文件 → 新建关卡 → 基础** → 存为 `Content/Runner/Maps/L_RunnerDemo_Whitebox`
2. **窗口 → 世界设置**：
   - GameMode Override = `BP_RunnerGameMode`（或 `RunnerGameMode`）
   - Default Track Data = `DA_RunnerDemo_01`
3. 放置 `BP_RunnerCharacter`（或依赖 GameMode 默认 Pawn）
4. 用立方体摆一条白模「路」+ 坑（视觉占位即可，判定以 Data 为准）

---

## 7. 竖屏预览

1. **设置 → 项目设置 → 平台 → Android**：Orientation = Portrait（Config 已写默认）
2. PIE 时可用 **移动预览** / 固定视口比例 9:16 看构图
3. PC 调试用键盘 Q/E；手机用后续 HUD 双钮

---

## 8. 与 Agent 协作时怎么说

少说「帮我弄一下界面」，多说可执行句，例如：

- 「用 Nexus `search_asset` 找 `DA_RunnerDemo_01`，把 JumpForward 改成 700」
- 「在 `BP_RunnerCharacter` 上绑定 IMC / IA」
- 「开 PIE，按 Jump，看 Output Log 有没有 Judge」

项目内 Skill：`.cursor/skills/ue-runner-editor-ops/SKILL.md`  
Nexus Rule：`.cursor/rules/nexuslink-workflow.mdc`
