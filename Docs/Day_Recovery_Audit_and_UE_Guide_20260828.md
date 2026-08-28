# 白天餐厅改动恢复与 UE 编辑器操作指南（2026-08-28）

> 范围：复核 2026-08-27 16:30–22:30 的相关会话与提交，并恢复白天订单、顾客表现、餐盘贴图、食材箱反馈和 VFX 接入。
>
> 约束：不要调整摄像机、相机 Actor、CameraComponent 或任何 `SDayCamera` 标签。

## 1. 已核对的改动与当前状态

| 模块 | 昨天确定的目标 | 当前 C++ 状态 | 仍需 UE 编辑器处理 |
|---|---|---|---|
| 订单生成 | 最大可售价值必须遵守 `MaxDishLevel`；生成队列不足时同步降低正式营业额目标 | 已恢复 | 用不同库存/最高等级跑 PIE 验收 |
| 顾客超时 | 超时未服务订单排回队尾，避免库存仍有但目标永远不可达 | 已恢复 | PIE 等待一次顾客超时，确认同一菜品稍后重新出现 |
| 特殊 NPC 队列 | NPC Director 未就绪时不能吞掉已出队的 NPC 槽位 | 已恢复 | 验收首个特殊 NPC 不丢失 |
| 顾客表现 | 入场 → 循环摇摆；成功交付 → 吃饭 → 离场；超时 → 直接离场 | C++ 两阶段离场桥接仍存在 | 检查 `BP_SDayCharacterStandIn` Timeline 接线 |
| 金币飞行 | 从顾客贴图/锚点飞到 `RevenueCurrent`，并调用 `WBP_FlyingItem.Init` | 已恢复 | Refresh All Nodes，检查曲线与材质 |
| 顾客餐盘所需食物 | 入座事件携带 `IngredientId + Level + GiftId`，显示对应菜品贴图 | C++ 参数仍存在 | 检查蓝图显示/清理逻辑和 DataAsset 图标映射 |
| 食材箱 | 箱体上不显示静态食材图标；点击成功后食材图标飞到实际生成格 | 已恢复 | 删除旧静态图标组件；替换/修复 Niagara 资源 |
| 合成反馈 | 成功合成事件携带食材、等级和目标格；`Min_FoodMerge_001` 作为一次性反馈 | C++ 回调仍存在 | 在 Presenter 蓝图中接入美术特效 |
| 鬼火 | 材质闪烁 + 四锚点循环 Niagara，不把角色 Debuff 特效直接常驻到鬼身上 | 设计方案保留 | 创建/重导美术资源并挂接四个锚点 |

## 2. 第一次打开项目前

1. 关闭 Unreal Editor。
2. 构建 `MiniGameEditor Win64 Development`，不要只依赖旧 DLL 或旧 Live Coding Patch。
3. 打开项目后，在 Content Browser 中对 `/Game/Day` 执行一次 **Fix Up Redirectors in Folder**。
4. 打开下面三个蓝图，依次执行 **File → Refresh All Nodes**，然后 Compile：
   - `/Game/Day/Board/BP_SDayBoardPresenter`
   - `/Game/Day/Board/BP_SDayCharacterStandIn`
   - `/Game/Day/Board/BP_SDayIngredientBinVisual`
5. 如果 `Ingredient Bin Clicked` 出现旧引脚，删除旧事件节点并重新添加。现在只应有：
   - `BinIndex`
   - `IngredientId`
   - `bSpawnSucceeded`

不要再连接旧的 `SpawnedCellIndex` 蓝图引脚；实际生成格现在由 C++ 内部取得并用于飞行动画。

## 3. 恢复顾客餐盘上的所需食物贴图

目标蓝图：`/Game/Day/Board/BP_SDayCharacterStandIn`

### 3.1 入座时显示

使用事件 `OnSeatOccupied(OccupantKey, bSpecialNpc, IngredientId, Level, GiftId)`：

1. 用 `IngredientId + Level` 选择对应菜品贴图。
2. 把贴图设置到餐盘上的 Image、Billboard 或 Plane 材质。
3. 显示餐盘订单图标。
4. 普通顾客和特殊 NPC 都走同一套菜品图标逻辑；`GiftId` 只用于特殊 NPC 礼物表现，不替代订单菜品。
5. 不要把订单图标挂到食材箱 Actor。

图标资源应来自：

- `/Game/Day/Art/food/food_rice_V0...V4`
- `/Game/Day/Art/food/food_egg_V0...V4`
- `/Game/Day/Art/food/food_fish_V0...V4`
- `/Game/Day/Art/food/food_hand_V0...V4`
- `/Game/Day/Art/food/food_leg_V0...V4`

推荐在 `/Game/Day/Data/DA_SDayBoardVisualConfig` 中检查：

- `bUseDishIcons = true`
- `DishIconMesh` 有效
- `DishIconMaterial = /Game/Day/Materials/M_SDayDishIcon`
- `DishIconTextureParameter = Tex`
- `DishIconTune = /Game/Day/Data/DT_SDayDishIconTune`
- `DishArtStemByIngredient` 或 `DishIconOverrides` 覆盖五条食材链

### 3.2 离座时清理

在 `OnSeatVacated` 中隐藏并清空餐盘订单贴图，但不要直接隐藏顾客 `Portrait`。顾客头像由 C++ 的两阶段离场状态在动画结束后清理。

验收：

- 每个顾客入座后，餐盘显示其订单对应的等级贴图。
- 顾客离场后贴图消失。
- 下一位顾客入座不会残留上一单贴图。
- 特殊 NPC 订单也显示正确菜品。

## 4. 恢复顾客入场、摇摆、吃饭和离场

目标蓝图：`/Game/Day/Board/BP_SDayCharacterStandIn`

现有曲线：

- `/Game/Day/Board/CustomerMotion/C_SDayCustomerEnter`
- `/Game/Day/Board/CustomerMotion/C_SDayCustomerWobble`
- `/Game/Day/Board/CustomerMotion/C_SDayCustomerEat`

所有 Timeline 只修改 `PortraitMotionRoot`，不要移动 Actor、`VisualRoot`、碰撞体或座位位置。

### 4.1 OnSeatOccupied

1. Stop Enter、Wobble、Eat、Exit 四个 Timeline。
2. 将 `PortraitMotionRoot` 的相对位置、旋转、缩放重置。
3. 相对位置设为 `EnterOffset`，缩放设为 `(0,0,0)`。
4. `TL_CustomerEnter → Play From Start`。

### 4.2 TL_CustomerEnter

- Update：
  - Location = `Lerp(EnterOffset, ZeroVector, Alpha)`
  - Scale = `(Alpha, Alpha, Alpha)`
- Finished：
  - Location = `(0,0,0)`
  - Scale = `(1,1,1)`
  - `TL_CustomerWobble → Play From Start`

### 4.3 TL_CustomerWobble

- 勾选 **Looping**。
- `Alpha × WobbleAmplitude` 接 Rotator 的 Pitch。
- 对 `PortraitMotionRoot` 调用 `SetRelativeRotation`。
- 默认幅度 2°；美术需要更明显时可改为 3–4°。

### 4.4 OnDepartureRequested

先停止四个 Timeline并重置摇摆旋转：

- `bServed = true`：播放 `TL_CustomerEat`
- `bServed = false`：直接播放 `TL_CustomerExit`

### 4.5 TL_CustomerEat / TL_CustomerExit

- Eat Update：`Alpha` 直接作为 Y 缩放，X/Z 为 1。
- Eat Finished：恢复 `(1,1,1)`，启动 Exit。
- Exit Update：
  - Location = `Lerp(ZeroVector, ExitOffset, Alpha)`
  - Scale = `1 - Alpha`
- Exit Finished：
  - 调用 `CompletePresentationDeparture`
  - 重置位置、旋转、缩放

## 5. 恢复金币飞行动画

目标 Widget：`/Game/Day/UI/WBP_FlyingItem`

1. 确认公开函数名称为 `Init`。
2. 参数名与类型必须保持：
   - `InStartPos`：Vector
   - `InEndPos`：Vector
   - `InCtrlPos`：Vector
   - `InDuration`：Float/Double
   - `InStartDelay`：Float/Double
   - `InMaxScale`：Float/Double
   - `InPathCurve`：CurveFloat
   - `InScaleCurve`：CurveFloat
3. `Init` 内必须把参数写入实际驱动 Tick 的变量，不能只依赖 Expose on Spawn 默认值。
4. C++ 起点使用 `RevenueFlyAnchor`（无效时才回退到 Portrait/Actor），终点查找前景 Widget 的 `RevenueCurrent`。
5. PIE 验收窗口模式和最大化模式各一次，金币不得从左上角 `(0,0)` 出现。

## 6. 食材箱：删除静态图标并恢复“飞到实际格子”

目标资产：

- `/Game/Day/Board/BP_SDayIngredientBinVisual`
- `/Game/Day/Board/BP_SDayBoardPresenter`

### 6.1 删除不该存在的箱体图标

在 `BP_SDayIngredientBinVisual` 的 Components 和 Construction Script 中删除或禁用旧的：

- `IngredientIcon`
- Sprite/Billboard/Image 形式的常驻食材图
- Construction Script 中根据 `IngredientId` 给箱体加静态图标的逻辑

保留箱体 Mesh、点击碰撞和开合动画。C++ 只在点击成功时创建临时的屏幕空间飞行贴图，不会在箱体上常驻图标。

### 6.2 Niagara 资产的固定路径

C++ 会按以下路径软加载；重新导入时请保持完全一致：

- `/Game/VFX_Merge/Niagara/BOX/Min_BoxOpen_Fog`
- `/Game/VFX_Merge/Niagara/BOX/Min_BoxOpen_FoodBurst`
- `/Game/VFX_Merge/Niagara/BOX/Min_BoxOpen_Trail`

这些是 `BP_SDayBoardPresenter` 的默认值，不再是不可编辑的硬编码。打开：

`/Game/Day/Board/BP_SDayBoardPresenter → Class Defaults → S Day Board → Ingredient Bins → VFX`

可以调整：

- 三个 Niagara System 资产引用
- `IngredientBinVfxStartOffset`、`IngredientBinVfxTargetOffset`
- `IngredientBinVfxViewOffset`：默认 `(0,-80,0)`，沿世界 -Y 向摄像机前移，避免被模型遮挡
- Fog、FoodBurst、Trail 各自的 Scale
- FoodBurst 是否在箱口/目标格播放
- Trail 用户参数名与 DurationTime
- `IngredientTrailBoundsPadding`：扩展整条轨迹的固定边界，避免跨格飞行被裁剪
- 屏幕空间食材图标的 Width、Duration 和 ArcHeight

如果导入后仍在 `/Game/VFX/...`，请在 Content Browser 内移动到上面的正式目录，再 Fix Up Redirectors。不要在文件系统中直接移动 `.uasset`。

### 6.3 Trail 参数

`Min_BoxOpen_Trail` 暴露三个用户参数：

- `User.StartPosition`：Position
- `User.TargetPosition`：Position
- `User.DurationTime`：Float

C++ 使用 `SetVariablePosition` 写入起点和终点，并在写完三个参数后才激活 Niagara Component，避免首帧使用默认终点。Trail Emitter：

1. 关闭 **Local Space**。
2. `Initialize Particle.Lifetime` 使用 `User.DurationTime`。
3. Particle Update 中设置：
   `Particles.Position = Lerp(User.StartPosition, User.TargetPosition, CurveAlpha)`
4. `CurveAlpha` 可由 `Particles.NormalizedAge → Float from Curve` 得到。
5. 当前资产没有 `User.ArcHeight`；如需 Niagara 自身走弧线，请在系统内部使用固定值或另行新增参数。
6. 禁用会与显式 Position 冲突的 Velocity、Gravity、Curl Noise、Vortex 等模块。

说明：食材贴图飞行本身由 C++ 的屏幕空间预览保证，会准确落到本次随机生成格；Niagara 是可替换的美术叠加反馈。

## 7. 合成与鬼火 VFX

### 7.1 合成

在 `/Game/Day/Board/BP_SDayBoardPresenter` 使用事件：

`Ingredient Merge Completed(IngredientId, ResultLevel, TargetCellIndex)`

将 `/Game/Day/VFX/Niagara/Min_FoodMerge_001` 作为一次性合成反馈，位置取 `TargetCellIndex` 对应格子。不要把它当作常驻鬼火。

### 7.2 鬼火

正式资产建议放在 `/Game/Day/VFX/Niagara/DayBoard/`。在 `/Game/Day/Blueprints/BP_SDayCanguan` 新增四个 SceneComponent：

- `FX_Ghost_TopLeft`
- `FX_Ghost_TopRight`
- `FX_Ghost_Right`
- `FX_Ghost_BottomLeft`

每个锚点挂循环 Niagara，位置和朝向由美术在蓝图视口中调整。鬼怪本体的闪烁用材质参数实现。不要把 `Min_FireDebuff_Character` 直接作为常驻鬼火。

## 8. 最终验收清单

- [ ] 完整关闭编辑器后重建成功。
- [ ] 三个目标蓝图 Refresh All Nodes、Compile 均无错误。
- [ ] 顾客入座后显示订单菜品贴图。
- [ ] 入场结束后自动循环摇摆。
- [ ] 正确交付后先吃饭，再离场。
- [ ] 超时顾客直接离场，其订单稍后重新出现。
- [ ] 金币从顾客处飞到 `RevenueCurrent`，不从左上角生成。
- [ ] 食材箱体上没有静态食材图标。
- [ ] 点击成功后，临时食材贴图飞到实际随机生成的格子。
- [ ] 棋盘满或库存不足时不播放成功飞行反馈。
- [ ] 箱口 Fog/Burst/Trail 能正常播放且不显示马赛克。
- [ ] 合成事件只在成功合成时触发。
- [ ] 不同 `MaxDishLevel` 下订单总价值均可达正式营业额目标。
- [ ] 未修改摄像机和相机标签。

## 9. 出现问题时优先检查

- 左上角生成：`WBP_FlyingItem.Init` 参数名/赋值缺失，或仍在运行旧 DLL。
- 餐盘无贴图：`OnSeatOccupied` 未接图标逻辑，或 `DA_SDayBoardVisualConfig` 的图标映射为空。
- 箱子点击但不飞：蓝图事件仍是旧四参数版本、项目未完整重建，或生成失败。
- Trail 不到目标：Niagara 使用 Local Space，或 Force/Velocity 覆盖了 `Particles.Position`。
- 特效马赛克/丢材质：内部仍引用 `/Game/VFX/...`；移动后 Fix Up Redirectors，再逐个重设 Parent Material/Texture。
- 顾客瞬间消失：Exit Timeline 的 Finished 未调用 `CompletePresentationDeparture`，或蓝图仍在 `OnSeatVacated` 直接隐藏 Portrait。
