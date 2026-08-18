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

