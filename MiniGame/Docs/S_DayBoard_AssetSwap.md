# 白天食肆棋盘：美术资产替换契约

参考原画：`Docs/ArtReference/DayBoard_Concept.png`

## 1. 不可改的逻辑契约

- `ASMergeBoard` 是棋盘数据唯一权威；蓝图只显示状态和转发输入。
- 逻辑格固定为 4×4 去四角，共 12 格：`1,2,4,5,6,7,8,9,10,11,13,14`。
- 不得在表现蓝图中复制 Merge、扣库存、交付或闭店回收规则。
- `IngredientId` 固定为：`LingGu`、`YinShanJun`、`ChiYanJiao`、`YueLinYu`、`XuanYuQin`。
- 菜品仍由 `IngredientId + Level(0–4)` 组成；替换模型不能修改 `RecipeId`。
- 白天与夜晚仍只通过 `FNightBootstrap` / `FNightResult` 交接。

## 2. 运行时资产

| 资产 | 职责 | 后续替换入口 |
|---|---|---|
| `BP_SDayBoardPresenter` | 竖屏构图、相机、棋盘框、柜台和各锚点 | `DA_SDayBoardVisualConfig` |
| `BP_SDayCellVisual` | 可玩孔位、点击碰撞、棋子挂点 | `CellMesh` / `PieceMesh` |
| `BP_SDayIngredientBinVisual` | 底部五类食材箱；五箱及间隙共同构成高级食材分解区 | `IngredientBinMesh` |
| `BP_SDayCustomerSeat` | 场景内可见的顾客/NPC 座位；负责立绘预览、交付碰撞和后续吃饭/谢礼表现 | 关卡 Transform / `PortraitWorldHeight` / `PortraitLocalOffset` |
| `BP_SDayCharacterStandIn` | 缺少场景座位时的白盒回退壳 | `CustomerMesh` / `NpcMesh` |
| `WBP_SDayHUD` | 顶部订单/交付、营业额与开店倒计时、库存、谢礼页签、流程按钮 | Widget Blueprint 外观 |
| `DT_SDayBoardLayout` | `/Game/Day/Data/` | 12 个逻辑格的世界位置与视觉半径 | 仅允许调 Transform/VisualRadius |
| `DT_GameStages` | `/Game/Shared/Data/` | 关卡时长/目标/`CustomerConcurrentMax`（真实座位数）/刷客间隔/NPC 规则/ Night `DefaultRoute` + `ForkPair` | Day/Night 共用 |
| `DT_SDayBalance` | `/Game/Shared/Data/` | 结转单价、最高菜级、订单槽与等级权重、饕餮怪权重、座位上限 | 单行 `Default` |
| `DT_Recipes` | `/Game/Shared/Data/` | 菜品售价 | Day/Night 共用 |
| `DT_Ingredients` / `DT_SpecialNpcs` / `DT_Gifts` / `DT_CustomerNames` | `/Game/Shared/Data/` | 食材名；特殊 NPC 委托规则/对白/谢礼；谢礼文案与下局数值（`EffectTrigger`/`EffectValue`）；普通顾客名池 | Day/Night 共用配置 |
| `DA_SDayBoardVisualConfig` | `/Game/Day/Data/` | 全部软引用的集中替换表 | 美术交付后只改这里 |

即使上述资产缺失，原生 C++ 会使用 Engine BasicShapes 生成可玩的白模。

### 餐馆环境绑定

餐馆网格和构图相机现在是两个独立蓝图：`BP_SDayCanguan` 与
`BP_SDayCompositionCamera`。关卡中分别是 `ENV_Canguan` 和 `Day_CompositionCamera`；
相机可以独立移动，不会带动餐馆模型。餐馆实例位置仍为 `(44.81, -600.89, -1276.70)`、
Yaw 180、**缩放 1**。层级：

```
BP_SDayCanguan
└─ DefaultSceneRoot
   ├─ Stall  (SceneComponent, 等比 3.89192)
   │  └─ 12 个 StaticMeshComponent（相对单位变换，带 SDay.* 组件 Tag）
```

canguan.fbx 的每块网格都以同一原点为枢轴导出，所以 12 个网格保持相对单位变换即可复现原
摆放。相机和 CookingUI 位于独立的 `BP_SDayCompositionCamera` 中，根节点位置就是相机位置。
重建/提取用 `Tools/ExtractDayCompositionCameraBlueprint.py`。

绑定 Tag 因此下移到**组件**上：`SDay.Environment` 留在 Actor 上（整座餐馆一次性关碰撞），
各功能位挂在对应组件的 `ComponentTags` 上。C++ 侧 `FindDayArtPiece` 会先按 Actor Tag 再按
组件 Tag 解析，所以白盒和早期「一块一个 Actor」的摆法都仍然可用。

- `SDay.Board`：组件 `pan`（网格 `/Game/Day/Art/canguan/pan`），作为原画坐标基准。模型里的
  孔是美术构图，不要求与逻辑格数量相同；25 个逻辑格仍由 `DT_SDayBoardLayout` 决定，并映射
  到 `pan` 的当前边界。
- `SDay.Bin.0`～`SDay.Bin.4`：依次绑定组件 `box6`、`box7`、`box8`、`box9`、
  `polySurface6`，对应灵谷、阴山菌、赤焰椒、月鳞鱼、玄羽禽。
- `SDay.Counter`：绑定组件 `tai1`，当前仅用于环境语义，不承担交互。
- `SDay.CustomerPlates`：绑定组件 `kepan`（顶部四个顾客餐盘），仍是座位美术对齐基准。
  `L_S_DayWhitebox` 已直接摆放 `SDay_Seat_0`～`SDay_Seat_3` 四个
  `BP_SDayCustomerSeat`；它们在编辑器里显示预览立绘，因此位置和大小直接改关卡 Actor 的
  Transform、`PortraitWorldHeight`、`PortraitLocalOffset`，不再改 C++ 常数，也不再由
  Presenter 在运行时移动。`AuthoredSeatSlot` 按画面从左到右为 0～3，逻辑仍按
  `CustomerConcurrentMax` 只启用需要的前 N 个座位，其余座位运行时隐藏但编辑器内保留。
  若关卡提供的场景座位少于当前阶段需求，Presenter 会隐藏这些不完整座位并回退到旧的运行时
  StandIn 生成方式，保证白盒和测试关卡仍可玩。
- `BP_SDayCustomerSeat` 的 `VisualRoot` 承载立绘/未来骨骼表现；`EatEffectAnchor` 和
  `GiftEffectAnchor` 预留给吃饭与谢礼特效。`OnSeatOccupied`、`OnSeatVacated`、
  `OnServeSucceeded` 是蓝图表现事件，不能在其中复制扣库存、订单匹配或谢礼数值逻辑。
  普通顾客贴图仍来自 `DT_CustomerNames.Portrait`，特殊 NPC 来自
  `DT_SpecialNpcs.Portrait`。
- `SDay.Environment`：`ENV_Canguan`（Actor Tag，覆盖整个蓝图，含分层平面）；Presenter 在运行时
  统一关闭其碰撞，防止家具先于逻辑代理截获点击。
- 组件 `Mesh_0` 暂不认定为厨师，不配置角色 Tag；餐馆模式不生成白盒厨师。

找到 `SDay.Board` 时，Presenter 会关闭旧棋盘方块、柜台方块和装饰圆柱；逻辑格、
食材箱和座位保留不可见的 `Visibility` 查询碰撞，座位的查询代理会放大到覆盖立绘，
保证点到看得见的顾客就能交付。座位的世界文字标签在餐馆模式下隐藏（它是为旧俯视机位
做的平躺文字，在当前机位近乎侧视），订单信息仍在 HUD 上。
缺少上述 Tag 时仍回退为完整可玩的白盒。Tag 现在随蓝图一起交付，重建蓝图用
  `Tools/ExtractDayCompositionCameraBlueprint.py`。`PlaceCanguanPreview.py` 与
`ConfigureDayArtBindings.py` 是合并前「一块一个 Actor」时代的工具，只在需要重新从 FBX
铺一遍散件时使用，之后仍需跑一次 `ExtractDayCompositionCameraBlueprint.py` 收回蓝图。

### 画面背景 / 前景（cookingUI 分层）

cookingUI 原画是按整幅（3146×6980，比例 0.45，与相机 `AspectRatio` 一致）切成的 7 层：
4 张背景压在 3D 摊子之后，3 张前景（Overlay）盖在摊子之前。现在使用两个控件蓝图：
`WBP_SDayCookingBackground` 和 `WBP_SDayCookingForeground`，每个控件蓝图内部用
CanvasPanel + Image 控件铺满整个画布，直接引用对应 PNG 贴图。

两个控件蓝图分别由 `CookingUI_Background` 和 `CookingUI_Foreground` 这两个
`WidgetComponent` 挂在 `StageCamera` 下。WidgetComponent 使用 World Space，并按相机的深度
和取景框尺寸缩放，因此相机移动/旋转时，控件蓝图整体跟随且不再依赖 StaticMesh 平面材质。

- **背景（组件 Tag `SDay.Backdrop`）**：`CookingUI_Background`，深度压在
  所有 `SDay.Environment` 之后。order 0（`Background_01` 街景）最远，往后每层朝相机迈一
  小步（`DayArtBackdropLayerStep`）：01 街景 → 02 人群 → 03 店面 → 04 内景。半透明按距离
  排序即可正确叠加。
- **前景（组件 Tag `SDay.Foreground`）**：`CookingUI_Foreground`，深度压在
  最近一件 `SDay.Environment` 之前（`DayArtForegroundDepthMargin`）。order 0（`Overlay_01`
  铜钱）最靠后，02（`Overlay_03` 红绳）最靠前，压在铜钱上。中央镂空透明，不遮挡锅台/
  食材箱；可玩 HUD 是 UMG，永远在这些世界平面之上，点击不受影响。
- 深度包围盒按餐馆各网格边界解算，不用 Actor 的合并边界——整座餐馆现在是一个 Actor，合并
  边界会松得没有意义。WidgetComponent 不参与餐馆包围盒测量。
- 餐馆带有几乎伸到相机脚下的地面/墙面平面，所以"最近一件装饰"可能只有两米远。前景基准
  深度会保底到 `DayArtForegroundMinDepth + 最大 order × 层间距`，否则整叠前景会一起夹到
  近裁剪同一深度上打架。
- 取景框宽度按投影方式解算（`DayCameraFrameWidthAtDepth`）：正交是 `OrthoWidth`（与距离
  无关），透视是 `2 * 深度 * tan(FOV/2)`——项目锁横向 FOV（`MAINTAIN_XFOV`），所以
  `FieldOfView` 是横向角；高度一律是宽度 / `AspectRatio`。每层只在单一深度上，所以透视下
  也能严丝合缝。
- `T_CookingUI_Concept` 仅作参考，不进运行时。
- 控件蓝图和 WidgetComponent 由 `Tools/ConvertDayCookingUIToWidgets.py` 生成；Presenter 在
  `BeginPlay` 用 `FitDayArtLayers` 重新计算 WidgetComponent 的深度和缩放，所以改 FOV /
  比例 / 投影方式不会留下过期的贴合。

### 相机

- 相机是独立蓝图 `/Game/Day/Blueprints/BP_DayCamera` 的 `Camera` 组件（组件 Tag
  `SDayCamera`）。关卡实例名为 `Day_Camera`。Presenter 找到它就把**它的 Owner** 设为 PIE
  视角（`AActor::CalcCamera` 会自动采用第一个激活的 CameraComponent），不覆盖投影、位置或
  缩放。移动这个 Actor 只会移动相机和挂在相机下的 CookingUI，不会移动 `ENV_Canguan`。
- **全关卡只能有一个带 `SDayCamera` 的相机**，多于一个视角就是抽签。换相机蓝图跑
  `Tools/SwitchDayStageCamera.py`：它把当前带标签相机的取景（Actor 变换 + 相机相对变换 +
  投影/FOV/比例/锁比例）搬进 `BP_DayCamera`，把 `background` 挂到相机下并打
  `SDay.Backdrop`，放好新实例后删掉旧的。旧的 `BP_SDayCompositionCamera` 及其生成脚本
  `Tools/ConvertDayCookingUIToWidgets.py` 已被取代，别再跑，否则会往关卡里塞回第二台相机。
- 当前状态：**Perspective**、`FieldOfView ≈19.6`、`ConstrainAspectRatio` + `AspectRatio 0.45`
  （1440x3200 竖屏），Actor 位置约 `(30.3, -2752.5, 131.9)`、pitch -11 / yaw 90，相机组件相对
  位置 `X ≈ -397`（沿视线向后退）。
- 调构图：在关卡中选中 `Day_Camera`，移动/旋转 Actor；或选中其 `Camera` 组件调整相对参数。
  要把调整写回蓝图，重跑 `Tools/SwitchDayStageCamera.py`。
- 切投影只用 `Tools/SetDayCameraProjection.py perspective|orthographic`：它只改投影模式，
  不动位置和缩放。`Tools/FitDayCameraToResolution.py` 是正交专用的缩放/居中解算，遇到
  透视相机会直接报错退出。
- 当前状态用 `Tools/DumpDayCameraState.py` 查。
- 写 Python 时注意 `unreal.Rotator` 的位置参数是 `(roll, pitch, yaw)`：把 180 写到 pitch 上会
  经四元数归一化成「yaw 180 + roll 180」，等于绕 Y 轴翻转 180°，整座摊子会翻出画面。

### 光照与曝光

- 画面唯一的运行时光源是 Presenter 的 `WhiteboxKeyLight`：顺相机方向从后往前照
  （`AimKeyLightAlongCamera`），颜色接近中性白（`DayArtKeyLightColor`）。餐馆材质目前是
  FBX 带来的灰色占位（DiffuseColor 0.4，无贴图），之前那股「木头黄」其实全来自暖色灯，
  所以灯一中性化画面就回到灰白模。
- 项目关掉了自动曝光（`r.DefaultFeature.AutoExposure=False`），画面亮度只由灯强度决定：
  `DayArtKeyLightIntensity = 0.7` 让灰模落在中灰（截图灰度均值约 137，峰值 149，不过曝）。
  不要在相机上打开 `AutoExposureMethod` 之类的覆盖，否则自动曝光会被重新打开，
  竖屏画面在横向编辑器窗口里的黑边会把直方图带偏、整帧过曝。
- `Tools/AddDayWhiteboxEditorLights.py` 生成的两盏预览灯既是 editor-only 又
  `HiddenInGame`：`is_editor_only_actor` 只在 Cook 时剔除，PIE 里仍然会照，之前 PIE 就是
  被「预览灯 + 运行时灯」照了两遍，比出包更亮更黄。现在 PIE 看到的就是出包的光照。

## 3. 模型规范

- UE 单位：厘米；导入比例 1.0。
- 棋盘、孔位、棋子、箱子均以底部中心为 Pivot，Z+ 朝上。
- 孔位和棋子默认面向顶视相机；孔位 Mesh 不负责逻辑碰撞，碰撞由 `ASDayCellVisual` 管理。
- 棋子模型建议直径 100 cm、高度 40–90 cm；等级尺寸由表现层统一缩放。
- 食材箱建议 120×80×70 cm；母棋子点击仍依赖箱体碰撞，高级食材分解使用覆盖整个食材区的共享屏幕区域。
- 场景座位的原点是**立绘的垂直中心**，即餐盘上沿再加半个 `PortraitWorldHeight`；默认高度
  330 时 Z 比餐盘上沿高 165。`SDay_Seat_0`～`SDay_Seat_3` 可在关卡中直接移动。
  立绘大小改座位实例的 `PortraitWorldHeight`；因为立绘以原点为中心上下对称展开，改完需要把
  Z 补上高度变化量的一半，脚下位置才不变。厘米级修正用 `PortraitLocalOffset`。
- 各张 PNG 底部的透明留白并不一样（普通顾客约 12%，特殊 NPC 约 4.5%），`SetPortrait` 会扫一遍
  贴图 alpha（每张只扫一次并缓存），按各自留白把立绘沿座位局部 -Z 下移，落地的是**画面内容**
  的下沿而不是图片下沿，`PortraitLocalOffset` 叠加在这个补偿之上。因此新立绘不需要统一留白，
  但底部留白必须是完全透明（alpha ≤ 8）。
- 顶部共享座位数量由 `DT_GameStages.CustomerConcurrentMax`（经 GI `GetServiceSeatCount`）驱动，普通顾客与特殊 NPC 到店时按空位入座、订单完成后立即离店空出；每个空座各自维护补客倒计时，归零后独立从统一订单队列补下一位，不等待其他座位完成。Mesh 必须响应 `Visibility` Trace；厨师不接收交付。
- 座位名牌是 `Label` TextRender，挂在角色下方（屏幕方向），最多三行：姓名 / 需求 / 等待状态或谢礼；空座显示座位编号及该座独立补客倒计时。
- 所有可交互组件必须响应 `Visibility` Trace；装饰孔位必须关闭碰撞。

### 世界标签字体

`UTextRenderComponent` 只能用 **离线字体**，引擎默认的 `RobotoDistanceField` 没有中文字形，直接用会显示成方块。
所有世界标签统一走 `DA_SDayBoardVisualConfig.LabelFont`（默认 `/Game/Day/UI/F_SDayLabel`）。

- 该字体由 `-run=SDayWhiteboxAssets` 从系统「Microsoft YaHei」烘出，字符集在 `SDayWhiteboxAssetsCommandlet.cpp` 的 `DayLabelChineseCharacters`。
- **世界标签出现新汉字时，必须把字，加进该字符串并重跑 commandlet**，否则新字是空白。
- 必须保持距离场导入（`bUseDistanceFieldAlpha`）：引擎默认文本材质是 Masked 且取 **R 通道**，普通位图导入把字形写在 Alpha，会渲染成实心方块。
- 可打印 ASCII 需显式列进 `Chars`（`bIncludeASCIIRange` 只建映射不烘字形），否则 `Lv0`、倒计时秒数会消失。
- HUD（UMG/Slate）不受此限制，中文由 Slate 字体回退处理。

## 4. 材质参数

占位材质约定向量参数 `Tint`。五条链建议色：

- 灵谷：红
- 阴山菌：青绿
- 赤焰椒：橙
- 月鳞鱼：蓝
- 玄羽禽：紫

正式材质可以不使用 `Tint`，但需在 `DA_SDayBoardVisualConfig` 中提供对应 Material。

## 5. 替换验收

1. 只替换 `DA_SDayBoardVisualConfig` 中一个 Mesh。
2. PIE 后确认 12 格位置、点击和拖放不变。
3. 取两个同链同级棋子合成，确认模型和 Lv 标签刷新。
4. 把 Lv1–Lv4 食材拖到五个基础食材箱或其间隙，确认撤销全部合成并按 `PaidUnits` 回到对应基础库存。
5. 异链/异级拖放应回弹，不扣库存。
6. 选中棋子必须有明显高亮（孔位变色 + 棋子抬升放大）。
7. 把棋子拖到（或点到）顶部座位可完成交付；普通顾客没有耐心值，可一直等待。
8. 闭店时盘上未交付棋子按 `PaidUnits` 退回库存。
9. 服务阿翎/桑婆后谢礼**立刻**出现在页签且已生效；页签只读，没有勾选与确认。
10. HUD 顶部显示开店剩余秒数；倒计时归零自动闭店（未达标回档日初，达标进日结）。
11. 棋子、食材箱、座位三处标签的中文与 `Lv0` 必须完整显示，不得出现方块或缺字。

PIE 内可用 `S.Day.OpenDay` 开店、`S.Day.Select <格号>` 选中棋子、`S.Day.RunSmoke` 跑全量冒烟。
HUD 底部「流程」按钮按阶段变化：入夜 → 模拟夜间到达终点 → 闭店。

