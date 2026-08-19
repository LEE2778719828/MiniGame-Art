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
| `BP_SDayCharacterStandIn` | 厨师、顾客、NPC 的占位壳；顶部共享座位（数量=`CustomerConcurrentMax`）同时是交付目标 | `ChefMesh` / `CustomerMesh` / `NpcMesh` |
| `WBP_SDayHUD` | 顶部订单/交付、营业额与开店倒计时、库存、谢礼页签、流程按钮 | Widget Blueprint 外观 |
| `DT_SDayBoardLayout` | `/Game/Day/Data/` | 12 个逻辑格的世界位置与视觉半径 | 仅允许调 Transform/VisualRadius |
| `DT_GameStages` | `/Game/Shared/Data/` | 关卡时长/目标/`CustomerConcurrentMax`（真实座位数）/刷客间隔/NPC 规则 | Day/Night 共用 |
| `DT_SDayBalance` | `/Game/Shared/Data/` | 结转单价、最高菜级、订单槽与等级权重、饕餮怪权重、座位上限 | 单行 `Default` |
| `DT_Recipes` | `/Game/Shared/Data/` | 菜品售价 | Day/Night 共用 |
| `DT_Ingredients` / `DT_SpecialNpcs` / `DT_Gifts` / `DT_CustomerNames` | `/Game/Shared/Data/` | 食材名；特殊 NPC 委托规则/对白/谢礼；谢礼文案与下局数值（`EffectTrigger`/`EffectValue`）；普通顾客名池 | Day/Night 共用配置 |
| `DA_SDayBoardVisualConfig` | `/Game/Day/Data/` | 全部软引用的集中替换表 | 美术交付后只改这里 |

即使上述资产缺失，原生 C++ 会使用 Engine BasicShapes 生成可玩的白模。

### 餐馆环境绑定

`L_S_DayWhitebox` 中的餐馆美术通过 Actor Tag 与运行时逻辑绑定：

- `SDay.Board`：`ENV_Canguan_pan`，作为原画坐标基准。模型里的孔是美术构图，
  不要求与逻辑格数量相同；25 个逻辑格仍由 `DT_SDayBoardLayout` 决定，并映射到
  `pan` 的当前边界。
- `SDay.Bin.0`～`SDay.Bin.4`：依次绑定 `box6`、`box7`、`box8`、`box9`、
  `polySurface6`，对应灵谷、阴山菌、赤焰椒、月鳞鱼、玄羽禽。
- `SDay.Counter`：绑定 `tai1`，当前仅用于环境语义，不承担交互。
- `SDay.CustomerPlates`：绑定 `kepan`（顶部四个顾客餐盘）。餐馆模式下座位不再用白盒
  的那一排坐标，而是由这排餐盘解算：横向对齐到餐盘格中心，纵向让立绘**下边框正好落在
  餐盘上沿**，深度压到摊面之后，于是顾客整体站在摊面上沿之上、从餐盘后面露出。
  座位数少于 4 时不再在整排里居中：开局随机抽不重复的餐盘格，之后每次刷新都会把空座
  （不可见）随机挪到没人用的餐盘上，所以下一位客人是随机占座；已入座的客人保持不动，
  只会跟着相机/美术改动重新贴到当前餐盘格。立绘按贴图高度归一化到同一世界高度
  （`DayArtPortraitSpriteHeight`），PNG 共享底边基准，所以角色本身的高矮差异会保留。
  各张 PNG 底部的透明留白并不一样（普通顾客约 12%，特殊 NPC 约 4.5%），只对齐图片边框
  会让一部分顾客悬在摊面上方，所以运行时会扫一遍贴图 alpha（每张只扫一次并缓存），把立绘
  按自己的留白下移，落地的是**画面内容**的下沿而不是图片下沿。因此新立绘不需要统一留白，
  但底部留白必须是完全透明（alpha ≤ 8）。
- `SDay.Environment`：所有 `ENV_Canguan_*` Actor 与背景平面；Presenter 在运行时统一关闭其
  碰撞，防止家具先于逻辑代理截获点击。
- `Mesh_0` 暂不认定为厨师，不配置角色 Tag；餐馆模式不生成白盒厨师。

找到 `SDay.Board` 时，Presenter 会关闭旧棋盘方块、柜台方块和装饰圆柱；逻辑格、
食材箱和座位保留不可见的 `Visibility` 查询碰撞，座位的查询代理会放大到覆盖立绘，
保证点到看得见的顾客就能交付。座位的世界文字标签在餐馆模式下隐藏（它是为旧俯视机位
做的平躺文字，在当前机位近乎侧视），订单信息仍在 HUD 上。
缺少上述 Tag 时仍回退为完整可玩的白盒。重新摆放环境后运行
`Tools/ConfigureDayArtBindings.py` 恢复 Tag；`PlaceCanguanPreview.py` 新生成的 Actor
也会自动带上这些 Tag。

### 画面背景（取景框参考）

- `SDay.Backdrop`：`ENV_FrameBackdrop`，一块正对相机、**刚好等于相机取景框**的平面，
  深度压在所有 `SDay.Environment` 之后。餐馆美术之前悬在纯黑虚空里，而竖屏画面在横向
  窗口里的黑边也是纯黑，看不出画面边界在哪；现在「背景色 ↔ 黑」的分界线就是出画边界
  （正交构图时在 1920×1151 的 PIE 窗口里量到 x 700..1219，正好是 9:20 的 520px 宽）。
- 取景框宽度按投影方式解算（`DayCameraFrameWidthAtDepth`）：正交是 `OrthoWidth`（与距离
  无关），透视是 `2 * 深度 * tan(FOV/2)`——项目锁横向 FOV（`MAINTAIN_XFOV`），所以
  `FieldOfView` 是横向角；高度一律是宽度 / `AspectRatio`。平面只在单一深度上，所以透视下
  也能严丝合缝。
- 材质 `M_SDayFrameBackdrop` 是 Unlit 深蓝灰：调灯或改曝光都不会改变这条边界的样子，
  也不会跟灰色占位模型混在一起。
- 相机改完构图后要重跑 `Tools/AddDayFrameBackdrop.py` 刷新编辑器视口里的平面；PIE 不
  依赖它，Presenter 在 `BeginPlay` 用实时相机重算一遍平面（`FitDayArtBackdrop`），
  所以运行时看到的边界永远是真的。用 `Tools/DumpBackdropFraming.py` 可以核对平面在
  相机空间的四角。

### 相机

- `Day_CompositionCamera`（Tag `SDayCamera`）是构图的唯一来源：Presenter 找到它就原样
  当作 PIE 视角，不覆盖投影、位置或缩放，所以构图完全在编辑器里手调。
- 当前状态：**Perspective**、`FieldOfView 90`、`ConstrainAspectRatio` + `AspectRatio 0.45`，
  位置 `(10.3, -3000.0, -137.3)`、pitch -6 / yaw 90。这套位置当初是在正交下调的（正交里
  距离不影响构图），换到透视后主体在画面里偏小，需要重新手调距离或 FOV。
- 切投影只用 `Tools/SetDayCameraProjection.py perspective|orthographic`：它只改投影模式，
  不动位置和缩放。`Tools/FitDayCameraToResolution.py` 是正交专用的缩放/居中解算，遇到
  透视相机会直接报错退出——它以前会顺手把相机改回正交，把手动切换的透视覆盖掉。
- 当前状态用 `Tools/DumpDayCameraState.py` 查（含相机 Actor 落在 `__ExternalActors__`
  的哪个包）。

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
- 角色占位建议原点在脚底，朝向 Y-；厨师与顾客/NPC 可分别替换 Mesh 和 AnimClass。
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

