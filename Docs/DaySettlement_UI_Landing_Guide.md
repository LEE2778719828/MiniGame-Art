# Day 白天结算 UI 落地指南

## 0. 先让新 C++ 类型进入编辑器

本次改动新增了反射类型 `ESDaySettlementOutcome`、`FSDaySettlementData` 和
`USDaySettlementWidget`，不能依赖 Live Coding 完成首次加载。

1. 保存并关闭 Unreal Editor。
2. 在项目根目录执行：
   `D:\UE_5.8\Engine\Build\BatchFiles\Build.bat MiniGameEditor Win64 Development -Project=E:\UEProjects\MiniGame\MiniGame\MiniGame.uproject -WaitMutex -NoHotReloadFromIDE`
3. 编译成功后重新打开 `MiniGame.uproject`。
4. 打开 `L_S_DayWhitebox`，不要调整关卡摄像机。

## 1. 建议的 Content 目录

在 Content Browser 中依次创建：

```text
/Game/Day/UI/Settlement/
    WBP_DaySettlement
    /Art/
```

当前四张谢礼按钮是与背景同尺寸的透明定位图层，不需要创建
`WBP_DaySettlementGiftCard`。只有以后改成裁切后、可自由排列的图标时才需要动态卡片 Widget。

`WBP_DaySettlement` 这个名字和路径应保持准确；C++ 在 HUD 未显式配置类时会按
`/Game/Day/UI/Settlement/WBP_DaySettlement` 自动查找。

## 2. 导入美术资源

1. 不要直接从系统 Temp 目录长期引用图片；先把成功、失败和对话框源图复制到稳定的美术源目录。
2. Content Browser → `/Game/Day/UI/Settlement/Art` → **Import**。
3. 每张 UI Texture 打开后设置：
   - Texture Group：`UI`
   - Compression Settings：`Uncompressed (RGBA8)`（UE 5.8 中旧称 `UserInterface2D (RGBA)`）
   - Mip Gen Settings：`NoMipmaps`
   - Address X/Y：`Clamp`
   - 保持 `sRGB` 开启（遮罩图例外）
4. 推荐把合成稿拆成标题、纸张框、钱币、谢礼栏、继续按钮、装饰线和暗色遮罩。
   原型阶段可以先用整张成功/失败稿做底图，但营业额文字、谢礼和按钮必须另做真实控件覆盖。
5. `【对话】对话框2.png` 已经包含底板、头像和“K易斯”姓名牌，保持整张导入即可；只在右侧空白纸面上叠加动态谢礼名称和效果 TextBlock。

### 2.1 当前五张按钮图按全画布定位图层使用

`【厨房成功】道具按钮1～4.png` 和 `【厨房成功】继续按钮.png` 都保持原始 `3146×6980`，
不裁透明边。每张图作为一个铺满设计画布的 Image，因此图案会自动与成功背景中的预定位置对齐。
在图案实际所在区域上方另盖一个透明 Button；不要直接把整张 PNG 设置成 Button Brush，否则整屏都会成为点击区域。

建议导入后命名：

- `T_DayResult_Gift_BlessedAmulet_Full`
- `T_DayResult_Gift_BossPie_Full`
- `T_DayResult_Gift_WildMilk_Full`
- `T_DayResult_Gift_WindfallWealth_Full`
- `T_DayResult_Continue_Full`

需要注意资源成本：单张 `3146×6980` 的 RGBA8 解压后约 84 MiB，五张理论合计约 419 MiB，
还不含成功/失败背景。原型和 PC 验证可以先这样做；若后续目标包含移动端或显存紧张的平台，
再考虑裁切、Texture Atlas 或改用合适的平台压缩。不要为了省显存在 UMG 中缩小图片尺寸；那不会减少源纹理占用。

## 3. 创建结算根 Widget

### 3.1 新建正确父类的 Widget Blueprint

1. Content Browser 打开 `/Game/Day/UI/Settlement`。
2. 空白处右键 → **User Interface → Widget Blueprint**。
3. 弹出 Pick Parent Class 时展开 **All Classes**，搜索 `SDaySettlementWidget`。
4. 选择 `SDaySettlementWidget`，不要选择普通 `UserWidget`。
5. 命名为 `WBP_DaySettlement`，双击打开。
6. 右上角 **Class Settings** → Parent Class 应显示 `SDaySettlementWidget`。如果不是，先修正再继续。

### 3.2 设置竖屏设计预览

1. 切到 **Designer**。
2. Designer 顶部的 Screen Size 下拉框选择 **Custom**。
3. Width 填 `1080`，Height 填 `2400`。这只改变设计预览，不会调整关卡摄像机。
4. Hierarchy 中通常已有一个 Canvas Panel；选中并按 `F2`，改名 `Canvas_Root`。
5. 选中 `Canvas_Root`，Details → Behavior → Visibility 设为 **Visible**。

最终推荐层级如下。成功和失败页使用 Canvas Panel，而不是 Overlay，这样更容易按参考稿摆放元素：

```text
Canvas_Root
├─ Image_DimBackground             ZOrder 0
├─ Button_InputBlocker             ZOrder 1
├─ SafeZone_Result                 ZOrder 10
│  └─ ScaleBox_Result              Scale To Fit
│     └─ SizeBox_Design            3146 × 6980
│        └─ WidgetSwitcher_Result
│           ├─ Canvas_Success      Active index 0
│           │  ├─ Image_SuccessArt       ZOrder 0，全画布
│           │  ├─ Text_RevenueEarnedSuccess  ZOrder 10，斜杠一侧
│           │  ├─ Text_RevenueTargetSuccess  ZOrder 10，斜杠另一侧
│           │  ├─ Text_NoGift            ZOrder 10
│           │  ├─ Canvas_Gift1           ZOrder 20，全画布 Image + 局部透明 Button
│           │  ├─ Canvas_Gift2           ZOrder 21，全画布 Image + 局部透明 Button
│           │  ├─ Canvas_Gift3           ZOrder 22，全画布 Image + 局部透明 Button
│           │  ├─ Canvas_Gift4           ZOrder 23，全画布 Image + 局部透明 Button
│           │  ├─ Canvas_Continue        ZOrder 24，全画布 Image + 局部透明 Button
│           │  └─ Canvas_GiftDialog      ZOrder 100，初始 Collapsed
│           └─ Canvas_Failure            Active index 1
│              ├─ Image_BackgroundFailure ZOrder 0，全画布
│              ├─ Image_FailureArt        ZOrder 10，全画布
│              ├─ Image_RetryArt          ZOrder 20，全画布
│              └─ Button_Retry            ZOrder 21，局部透明热区
```

### 3.3 创建暗色背景

1. Palette 搜索 `Image`，拖到 `Canvas_Root`，改名 `Image_DimBackground`。
2. 选中它，Details → Slot (Canvas Panel Slot) → Anchors，点击锚点按钮。
3. 选择右下角那个“四边铺满”的 Anchor 预设。
4. Offsets 的 Left、Top、Right、Bottom 全部填 `0`。
5. ZOrder 填 `0`。
6. Details → Appearance → Color and Opacity 设为黑色，Alpha 先填 `0.60`。
7. Details → Behavior → Visibility 设为 **Not Hit-Testable (Self & All Children)**。

它的作用是把仍在场景中的白天经营画面压暗，不负责接收点击。

### 3.4 创建全屏输入拦截按钮

1. Palette 搜索 `Button`，拖到 `Canvas_Root`，改名 `Button_InputBlocker`。
2. Anchors 选择 Full Screen，Offsets 四项全部为 `0`。
3. ZOrder 填 `1`。
4. Details → Interaction → Is Focusable 取消勾选。
5. Details → Style：把 Normal、Hovered、Pressed 三个 Brush 的 Tint Alpha 都设为 `0`；按钮视觉上应完全透明。
6. 不给它绑定 OnClicked。它只负责吞掉成功/失败主体外的点击，防止点击穿透到棋盘。

注意：`Button_InputBlocker` 的 ZOrder 必须低于 `SafeZone_Result`。如果把它放到最上层，继续和重试按钮也会点不到。

### 3.5 创建 Safe Zone、固定设计画布和 Widget Switcher

1. Palette 搜索 `Safe Zone`，拖到 `Canvas_Root`，改名 `SafeZone_Result`。
2. Anchors 选择 Full Screen，Offsets 四项为 `0`，ZOrder 填 `10`。
3. Visibility 设为 **Not Hit-Testable (Self Only)**。这表示 Safe Zone 自己不挡点击，但里面的 Button 仍可点击。

`Safe Zone`、`Scale Box` 和 `Size Box` 都是**单子项容器**，每个控件只能有一个直接子控件。因此，如果你已经按旧步骤把 `WidgetSwitcher_Result` 放进了 `SafeZone_Result`，再把 `Scale Box` 拖进去时会提示“控件无法接受额外子项”。此时不要把 Scale Box 放成并列项，按下面的方法包裹现有控件：

4. 在 Hierarchy 中右键 `WidgetSwitcher_Result`，选择 **Wrap With... → Scale Box**。
5. 新生成的 Scale Box 会自动插在 Safe Zone 和 Switcher 之间；选中它，按 `F2` 改名为 `ScaleBox_Result`。
6. 再右键 `WidgetSwitcher_Result`，选择 **Wrap With... → Size Box**，把新生成的 Size Box 改名为 `SizeBox_Design`。
7. 检查层级必须是 `SafeZone_Result → ScaleBox_Result → SizeBox_Design → WidgetSwitcher_Result`，四者逐层嵌套，不是并列关系。
8. 选中 `ScaleBox_Result`，Slot 的 Horizontal/Vertical Alignment 都设为 **Fill**；Stretch 设为 **Scale To Fit**，Stretch Direction 设为 **Both**。
9. 选中 `SizeBox_Design`，Width Override 填 `3146`，Height Override 填 `6980`。这正是背景和五张按钮定位图的原始尺寸。
10. 选中 `WidgetSwitcher_Result`，Slot 的 Horizontal Alignment 和 Vertical Alignment 都设为 **Fill**，Visibility 设为 **Not Hit-Testable (Self Only)**。

如果右键菜单里没有 **Wrap With...**：先选中 `WidgetSwitcher_Result` 按 `Ctrl+X`；此时 Safe Zone 变为空，再依次把 `Scale Box` 拖入 Safe Zone、把 `Size Box` 拖入 Scale Box；最后选中 `SizeBox_Design` 按 `Ctrl+V` 放回 Switcher。已有的成功页和失败页会跟随 Switcher 一起保留。

Widget Switcher 一次只显示一个直接子控件：索引 `0` 是成功页，索引 `1` 是失败页。

`ScaleBox_Result + SizeBox_Design` 很重要：它让所有 `3146×6980` 图层始终用同一比例一起缩放，
透明图层中的道具和继续按钮才能与背景保持像素级对齐。

### 3.6 创建成功页

1. Palette 搜索 `Canvas Panel`，拖到 `WidgetSwitcher_Result`，改名 `Canvas_Success`。
2. 再拖一个 Image 到 `Canvas_Success`，改名 `Image_SuccessArt`。
3. `Image_SuccessArt` 设置 Full Screen Anchor、Offsets 全 0、ZOrder 0。
4. Brush → Image 选择成功页美术；Visibility 设为 **Not Hit-Testable (Self & All Children)**。
5. 原画已经包含中间的 `/`，所以拖入两个独立 TextBlock，分别命名为 `Text_RevenueEarnedSuccess` 和 `Text_RevenueTargetSuccess`，两者都勾选 **Is Variable**，初始文字均写 `0`。
6. 把 `Text_RevenueEarnedSuccess` 放到实际营业额数字位置，把 `Text_RevenueTargetSuccess` 放到目标营业额数字位置。不要在任一 TextBlock 中再写 `/`，也不要使用一个 `0 / 0` 文本覆盖原画斜杠。
7. 拖一个 Text，改名 `Text_NoGift`，文字填“本日无谢礼”，勾选 **Is Variable**，初始设为 `Collapsed`。
8. 向 `Canvas_Success` 拖入 5 个 Canvas Panel，命名为 `Canvas_Gift1`～`Canvas_Gift4` 和 `Canvas_Continue`；它们都设置 Full Screen Anchor、Offsets 全 0。
9. 五个 Canvas 的 ZOrder 依次设为 `20`～`24`，Visibility 设为 **Not Hit-Testable (Self Only)**。四个 Gift Canvas 勾选 **Is Variable**，初始设为 `Collapsed`；`Canvas_Continue` 保持 Visible。
10. 在每个 Canvas 内放一个 Image，分别命名为 `Image_Gift1`～`Image_Gift4` 和 `Image_Continue`；全部设置 Full Screen Anchor、Offsets 全 0，并选择对应的原始 `3146×6980` 纹理。
11. 五个 Image 的 Visibility 都设为 **Not Hit-Testable (Self & All Children)**。这样透明区域和图案本身都不会截获点击。
12. 在每个 Canvas 内再放一个 Button，分别命名为 `Button_Gift1`～`Button_Gift4` 和 `Button_Continue`。Button 使用左上 Anchor、Alignment `(0,0)`、关闭 Auto Size，ZOrder 高于同层 Image。
13. Button 的 Normal/Hovered/Pressed Brush Tint Alpha 全设为 `0`，Content Padding 全部为 `0`；它只是覆盖图案的透明点击热区。
14. 按下表设置各 Button 的 Canvas Panel Slot Position 和 Size：

| Button | Position X/Y | Size X/Y |
|---|---:|---:|
| `Button_Gift1` | `2273, 2290` | `566, 652` |
| `Button_Gift2` | `941, 2812` | `608, 566` |
| `Button_Gift3` | `1644, 2567` | `566, 604` |
| `Button_Gift4` | `316, 2872` | `576, 728` |
| `Button_Continue` | `2140, 3575` | `570, 568` |

15. 五个 Button 都勾选 **Is Variable**。运行时显示/隐藏的是 `Canvas_Gift1～4`，这样美术 Image 和透明 Button 会同步显隐，不会出现“图没显示但仍能点击”的隐形热区。

原始全画布 PNG 只放在 Image 上，透明 Button 始终保持为上表所列的小范围点击区域。

### 3.7 创建失败页

1. 再拖一个 `Canvas Panel` 到 `WidgetSwitcher_Result`，确保它与 `Canvas_Success` 是同级，改名 `Canvas_Failure`。
2. 在其中创建 `Image_BackgroundFailure`，Full Screen Anchor、Offsets 全 0、ZOrder `0`，选择失败页背景图。
3. 如果“玩完了/失败”等内容是独立的同尺寸图层，创建 `Image_FailureArt`，同样 Full Screen、Offsets 全 0、ZOrder `10`。
4. 如果重试/继续图案也是独立的同尺寸图层，创建 `Image_RetryArt`，同样 Full Screen、Offsets 全 0、ZOrder `20`。不要把它命名为 `Button_Continue_Failure`，因为它只是 Image，不负责点击。
5. 上述所有 Image 的 Visibility 都设为 **Not Hit-Testable (Self & All Children)**。
6. 创建真正的 `Button_Retry`，勾选 Is Variable，只覆盖原画中的播放三角区域，ZOrder 填 `21`；Normal/Hovered/Pressed Brush Tint Alpha 全设为 `0`。
7. 当前失败参考稿没有动态营业额数字，因此不必创建 `Text_RevenueFailure` 或 `Text_RevenueGap`。如果后续美术要求显示数字，也应像成功页一样将原画分隔符两侧做成独立 TextBlock。

### 3.8 检查 Switcher 子项顺序

Hierarchy 应显示：

```text
WidgetSwitcher_Result
├─ Canvas_Success
└─ Canvas_Failure
```

如果顺序相反，拖动调整，或者后续 Graph 中相应交换 Active Widget Index。本文后续统一约定：

- `0` = Success
- `1` = Failure

现在点击 **Compile → Save**。此时还没有连接数据和按钮，继续执行第 4、5 节。

## 4. 连接 `Settlement Presented` 事件

### 4.1 先检查 Designer 控件变量

回到 Designer，在 Hierarchy 中逐个选中并确认勾选 **Is Variable**：

- `WidgetSwitcher_Result`
- `Text_RevenueEarnedSuccess`
- `Text_RevenueTargetSuccess`
- `Text_NoGift`
- `Canvas_Gift1`～`Canvas_Gift4`
- `Button_Gift1`～`Button_Gift4`
- `Button_Continue`
- `Button_Retry`

点击一次 **Compile**。只有编译后，这些控件才会出现在 Graph 左侧的 Variables 中。

### 4.2 复用已经从 Data 提升的变量

如果你已经对 `Break SDaySettlementData` 的输出执行过 **Promote to Variable（提升为变量）**，左侧会已有：

- `Outcome`
- `End Reason`
- `Revenue`
- `Revenue Target`
- `Revenue Gap`
- `Gift Ids`

直接复用这些变量，不要再创建带 `Current` 前缀的第二组。`End Reason` 当前 UI 暂时不用，保留即可。

如果已经照旧版指南创建了 `CurrentOutcome`、`CurrentRevenue`、`CurrentRevenueTarget`、
`CurrentRevenueGap`、`CurrentGiftIds`，并且尚未连接节点，可以在 Variables 列表中选中后删除；
如果已经连接，先把 Graph 中的 Get/Set 节点替换为上面第一组变量，再删除重复项。

只有某个变量确实不存在时，才在对应的 Break 输出引脚上右键选择 **Promote to Variable**。

### 4.3 放置事件并拆开 Data

1. 切换到 `WBP_DaySettlement → Graph → Event Graph`。
2. 在空白处右键，搜索并添加 **Event Settlement Presented**。
3. 该事件应有一个白色执行输出和一个蓝色/结构体 `Data` 输出。
4. 从 `Data` 引脚拖出，搜索 **Break SDaySettlementData**。
5. Break 节点需要使用的输出是：`Outcome`、`Revenue`、`Revenue Target`、`Revenue Gap`、`Gift Ids`。如果没有全部显示，点击节点底部的小箭头展开高级引脚。

如果搜索不到事件，先检查 `WBP_DaySettlement` 的 Parent Class 是否为 `SDaySettlementWidget`；如果父类正确但仍没有事件，关闭编辑器并完成第 0 节的非 Live Coding 编译。

### 4.4 写入已提升的变量，并按成功/失败分流

从左侧 Variables 依次拖出，松开后选择 **Set**，把白色执行线串成：

```text
Event Settlement Presented
  → Set Outcome
  → Set Revenue
  → Set Revenue Target
  → Set Revenue Gap
  → Set Gift Ids
  → Switch on ESDaySettlementOutcome
```

同时连接数据线：

```text
Break.Outcome        → Set Outcome 的值
Break.Revenue        → Set Revenue 的值
Break.RevenueTarget  → Set Revenue Target 的值
Break.RevenueGap     → Set Revenue Gap 的值
Break.GiftIds        → Set Gift Ids 的值
Outcome (Get)        → Switch 的 Selection
```

添加 Switch 的方法：从 `Outcome` 的 Get 引脚拖出，搜索 **Switch on ESDaySettlementOutcome**。最终只使用 `Success` 和 `Failure` 两个执行输出；`None` 不连接任何结算操作。

### 4.5 连接 Success 执行分支

1. 从左侧拖入 `WidgetSwitcher_Result`，选择 **Get**。
2. 从它的引用引脚拖出，搜索 **Set Active Widget Index**，Index 填 `0`。
3. 把 Switch 的 `Success` 白线接到这个节点。
4. 从左侧拖入 `Text_RevenueEarnedSuccess`，选择 Get；从引用引脚拖出 **Set Text (Text)**。
5. 拖入 `Revenue` 的 Get 节点，从整数引脚拖出搜索 **To Text (Integer)**，再接到第一个 Set Text 的 `In Text`。在 To Text 节点上取消 **Use Grouping**，这样显示 `3000`，不会显示 `3,000`。
6. 同样创建 `Text_RevenueTargetSuccess → Set Text`，把 `Revenue Target → To Text (Integer)` 接入。原画中的 `/` 不连接任何节点。
7. 调用第 6 节创建的 `RefreshGiftLayers`，把 `Gift Ids` 接到它的 `GiftIds` 输入。
8. 白色执行线顺序必须是：

```text
Success
  → Set Active Widget Index (0)
  → Set Text：实际营业额
  → Set Text：目标营业额
  → RefreshGiftLayers(Gift Ids)
```

`RefreshGiftLayers` 内部会同时完成四个谢礼 Canvas 的显隐和 `Text_NoGift` 的空状态显隐，不需要在 Event Graph 再重复判断。

### 4.6 连接 Failure 执行分支

1. 再从 `WidgetSwitcher_Result` 创建一个 **Set Active Widget Index**，Index 填 `1`。
2. 把 Switch 的 `Failure` 白线接入。
3. 当前失败稿没有动态营业额文字，因此这个分支到此结束，不需要设置 Text。
4. `Button_Retry.OnClicked → Confirm Retry` 放在第 5 节单独连接，不要在 `Settlement Presented` 事件中主动回档。

完成后的主 Graph 可以概括为：

```text
Event Settlement Presented
  → 写入已提升的变量
  → Switch on ESDaySettlementOutcome
       ├─ Success → Switcher=0 → 写入两个营业额数字 → RefreshGiftLayers
       ├─ Failure → Switcher=1
       └─ None    → 不处理
```

不要在这个事件中调用发放谢礼、改营业额、Open Level 或 Load Game。它只展示 C++ 已捕获的数据。

## 5. 连接成功和失败按钮

在 `WBP_DaySettlement` 中：

- `Button_Continue.OnClicked` → 调用继承函数 **Confirm Success**。
- `Button_Retry.OnClicked` → 调用继承函数 **Confirm Retry**。

两个函数都有 C++ 单次提交保护：

- 成功只会提交成功结算，然后沿现有 `AdvanceToNextStage → TravelToNight` 进入夜晚。
- 失败只会恢复 `DayStartSnapshot`，保持当前 Stage，并重新开始本次白天。
- Outcome 不匹配或重复点击时返回 `false`，不会重复推进。

可以在点击后立即把对应 Button 设为 Disabled，作为视觉反馈；业务安全不依赖这个蓝图禁用。

## 6. 根据 GiftIds 显示固定谢礼图层并连接点击

当前美术已经把四件谢礼的位置烘焙到与背景相同的透明画布中，因此不要创建 Horizontal Box，
也不要在运行时重新排列它们。

### 6.1 检查需要操作的控件

回到 Designer，确认以下控件已经勾选 **Is Variable**：

- `Canvas_Gift1`～`Canvas_Gift4`
- `Button_Gift1`～`Button_Gift4`
- `Text_NoGift`

四个 `Canvas_Gift` 的初始 Visibility 建议设为 `Collapsed`。每个 Canvas 内同时包含对应的全画布 Image
和局部透明 Button，因此后面只控制 Canvas，就能让美术和点击热区一起显示或隐藏。

### 6.2 创建 `RefreshGiftLayers` 函数

1. 打开 `WBP_DaySettlement → Graph`。
2. 左侧 **My Blueprint → Functions** 右侧点击 `+`。
3. 把函数命名为 `RefreshGiftLayers`。
4. 选中函数入口节点，在 Details → Inputs 点击 `+`。
5. 输入名填 `InGiftIds`，类型选择 **Name**，再把右侧容器类型改为 **Array**。
6. 点击 **Compile**，确认入口节点出现紫色数组引脚 `InGiftIds`。

### 6.3 为四件谢礼创建 Contains 判断

从函数入口的 `InGiftIds` 数组引脚拖出，搜索 **Contains Item**（部分中文界面显示“包含项目”）。
复制成四个 Contains 节点，每个节点的 Item 输入直接填写一个 Name：

| 检查的 GiftId | 为 True 时显示 |
|---|---|
| `BlessedAmulet` | `Canvas_Gift1` |
| `BossPie` | `Canvas_Gift2` |
| `WildMilk` | `Canvas_Gift3` |
| `WindfallWealth` | `Canvas_Gift4` |

这些字符串必须完全一致，不能填中文显示名，也不要加引号。四个 Contains 都是纯节点，不需要连接白色执行线；
它们各自的 Boolean Return Value 表示本次结算是否包含对应谢礼。

### 6.4 用 Select 控制四个 Canvas 的 Visibility

这里推荐使用 `Select`，不需要为每件谢礼放一个 Branch：

1. 从第一个 Contains 的 Boolean Return Value 拖出，搜索 **Select**。
2. 确认 Select 的返回类型为 `ESlateVisibility`。如果它尚未确定类型，先从 `Canvas_Gift1 → Set Visibility` 的 `In Visibility` 引脚反向拖出创建 Select。
3. Select 的 `False` 填 `Collapsed`，`True` 填 `Visible`。
4. 从左侧 Variables 把 `Canvas_Gift1` 拖入函数图，选择 **Get**。
5. 从 Canvas 引用拖出搜索 **Set Visibility**，把 Select 的返回值连接到 `In Visibility`。
6. 对 `Canvas_Gift2～4` 重复相同操作，各自使用对应 Contains 的结果。
7. 把白色执行线依次串起来：

```text
RefreshGiftLayers Entry
  → Canvas_Gift1.SetVisibility
  → Canvas_Gift2.SetVisibility
  → Canvas_Gift3.SetVisibility
  → Canvas_Gift4.SetVisibility
```

每一组的数据线应为：

```text
InGiftIds → Contains Item(Item=指定 GiftId)
Contains.ReturnValue → Select Condition
Select(False=Collapsed, True=Visible) → Canvas_Gift.SetVisibility.InVisibility
```

不要只隐藏 `Image_Gift` 或只隐藏 `Button_Gift`；必须隐藏外层 `Canvas_Gift`，否则可能留下看不见但仍能点击的热区。

### 6.5 处理“本日无谢礼”文字

1. 放置三个 **OR Boolean**：
   - `Or12 = Gift1Contains OR Gift2Contains`
   - `Or34 = Gift3Contains OR Gift4Contains`
   - `HasAnyKnownGift = Or12 OR Or34`
2. 从 `Text_NoGift` 创建 **Set Visibility**，接在 `Canvas_Gift4.SetVisibility` 的白色执行输出后。
3. 给它创建一个 `Select (ESlateVisibility)`，这次方向相反：
   - Condition = `HasAnyKnownGift`
   - `False` = `Visible`
   - `True` = `Collapsed`
4. 将 Select 返回值连接到 `Text_NoGift.SetVisibility.InVisibility`。

如果设计中不需要“本日无谢礼”文字，可以省略整个 6.5；四个谢礼 Canvas 的逻辑不受影响。

### 6.6 从 Settlement Presented 调用函数

回到 Event Graph，在第 4.5 节 Success 分支的两个营业额 `Set Text` 后：

1. 右键搜索 `RefreshGiftLayers` 并添加调用节点。
2. 把已经保存的 `Gift Ids` 变量以 Get 方式拖入。
3. `Gift Ids` 接到函数的 `InGiftIds`。
4. 白色执行线接成：

```text
Set Active Widget Index (0)
  → Set Text：实际营业额
  → Set Text：目标营业额
  → RefreshGiftLayers(InGiftIds = Gift Ids)
```

### 6.7 连接四个透明按钮

可以在 Designer 中选中按钮，在 Details 最下方 Events 点击 `OnClicked` 右侧的 `+`；也可以在 Graph 中
选中按钮变量后添加 OnClicked。分别建立四个事件：

```text
Button_Gift1.OnClicked → Request Gift Details(Name = BlessedAmulet)
Button_Gift2.OnClicked → Request Gift Details(Name = BossPie)
Button_Gift3.OnClicked → Request Gift Details(Name = WildMilk)
Button_Gift4.OnClicked → Request Gift Details(Name = WindfallWealth)
```

每个事件的具体连接方法相同：

1. 从 `OnClicked` 白色执行引脚拖出，搜索继承函数 **Request Gift Details**。
2. 在它的 `Gift Id`/`Name` 输入框中填写上表对应的英文 Name，不加引号。
3. 函数的 Boolean Return Value 可以暂时不接；C++ 已防止查询不属于本次结算的 GiftId。
4. 不要在 OnClicked 中直接显示对话框，也不要自己读取 DataTable；成功查询后，C++ 会触发第 7 节的 **Event Gift Details Requested**，由那个事件刷新并显示对话框。

### 6.8 最终检查

点击 **Compile → Save**，确认没有以下问题：

- `Canvas_Gift1～4` 而不是内部 Image/Button 被 Set Visibility。
- 四个 Contains 的 Item 与表中的 GiftId 完全一致。
- `Text_NoGift` 的 Select 方向与 Gift Canvas 相反。
- 四个 Button 的 OnClicked 分别传入自己的 GiftId，没有复制后忘记修改。
- Button 点击区域只覆盖实际图案，整张透明 PNG Image 均为 Not Hit-Testable。

`Request Gift Details` 会再次校验该 GiftId 确实属于本次结算，再从
`/Game/Shared/Data/DT_Gifts` 读取名称和效果说明；点击只展示说明，不会再次发放谢礼。

这里由 Blueprint 响应 C++ 发出的 `Settlement Presented` 数据并控制表现；结算结果、GiftId 合法性、
继续推进和失败回档仍由 C++ 负责。若以后美术改为裁切小图，再考虑 Horizontal Box + 动态卡片 Widget。

## 7. 创建谢礼作用对话框

`【对话】对话框2.png` 的实际尺寸是 `3146×2344`，带 Alpha；头像、底板和“K易斯”姓名牌都已经画在图中。
因此不要再创建头像 Image、人物姓名 Text，也不需要使用 `SeatPreviewIcon`。动态内容只放在右侧空白纸面。

### 7.1 在成功页建立对话框图层

1. 回到 `WBP_DaySettlement → Designer`。
2. 在 `Canvas_Success` 下创建一个 `Canvas Panel`，命名为 `Canvas_GiftDialog`，勾选 **Is Variable**。
3. 让它覆盖整个 `3146×6980` 成功页设计画布：Full Screen Anchor、Offsets 全 `0`、ZOrder `100`。
4. Visibility 设为 **Collapsed**。不要设成 Hidden；Collapsed 时不会留下布局和点击区域。
5. 在 `Canvas_GiftDialog` 下创建 `Image_GiftDialogArt`，Brush 选择导入后的 `【对话】对话框2` 纹理。
6. `Image_GiftDialogArt` 的 Size 设为 `3146×2344`，X 设为 `0`；Y 根据参考图拖到下方半透明区域。保持宽高比，不要拉伸变形。
7. Image 的 Visibility 设为 **Not Hit-Testable (Self & All Children)**。

对话框图本身含有较大的透明上下边距，这是原资产的一部分。定位时以实际可见的棕色/白色框为准，
不要误以为 Image 边框顶部就是可见对话框顶部。

### 7.2 在右侧白纸区域叠加动态文字

在 `Canvas_GiftDialog` 下创建两个 TextBlock：

| 控件名 | 内容 | 建议用途 |
|---|---|---|
| `Text_GiftDisplayName` | 初始写“谢礼名称” | 显示 `SGiftDefRow.DisplayName` |
| `Text_GiftEffect` | 初始写“谢礼效果说明” | 显示 `SGiftDefRow.EffectText` |

1. 两个 TextBlock 都勾选 **Is Variable**，ZOrder 设为 `2`。
2. 把它们摆在右侧白纸内部，不要覆盖左侧头像、已经画好的“K易斯”姓名牌和底部蓝色装饰线。
3. `Text_GiftDisplayName` 放在白纸上部，字号略大；`Text_GiftEffect` 放在其下方。
4. `Text_GiftEffect` 勾选 **Auto Wrap Text**；其 Canvas Slot 必须给出足够的 Width 和 Height，否则长说明会被裁掉。
5. 两个文字的 Visibility 保持 **Not Hit-Testable (Self & All Children)**。

这里的 `DisplayName` 是“谢礼名称”，例如某件道具的名称，不是人物姓名；原画中的“K易斯”保持不变。

### 7.3 添加关闭点击区域

原画没有单独的关闭图标，推荐让玩家点击对话框纸面即可关闭：

1. 在 `Canvas_GiftDialog` 下创建 `Button_CloseGiftDialog`，勾选 **Is Variable**。
2. 把 Button 覆盖在整个可见对话框区域上，ZOrder 设为 `3`；不要铺满整个 `3146×6980` 画布。
3. Normal/Hovered/Pressed Brush Tint Alpha 全设为 `0`，Is Focusable 取消勾选。
4. 注意 Button 会位于文字上方并接收点击，但它完全透明，不影响文字显示。
5. 在 Graph 中添加 `Button_CloseGiftDialog.OnClicked`：

```text
Button_CloseGiftDialog.OnClicked
  → Canvas_GiftDialog.SetVisibility(Collapsed)
```

如果你以后获得单独的关闭按钮美术，可以缩小这个透明热区并盖到关闭图标上，其他逻辑不用改。

### 7.4 连接 `Event Gift Details Requested`

1. 回到 Event Graph，在空白处右键搜索 **Event Gift Details Requested**。
2. 从事件参数 `Gift` 拖出，创建 **Break SGiftDefRow**。
3. 从 `Text_GiftDisplayName` 引用拖出创建 **Set Text (Text)**，把 Break 的 `Display Name` 接到 `In Text`。
4. 从 `Text_GiftEffect` 引用拖出创建另一个 **Set Text (Text)**，把 Break 的 `Effect Text` 接到 `In Text`。
5. 从 `Canvas_GiftDialog` 引用拖出创建 **Set Visibility**，值选择 `Visible`。
6. 白色执行线按以下顺序连接：

```text
Event Gift Details Requested
  → Text_GiftDisplayName.SetText(Display Name)
  → Text_GiftEffect.SetText(Effect Text)
  → Canvas_GiftDialog.SetVisibility(Visible)
```

`Seat Preview Icon` 不连接，因为头像已经烘焙在当前 PNG 中。若搜索不到 Event，确认
`WBP_DaySettlement` 的父类仍是 `SDaySettlementWidget`。

### 7.5 复用和层级检查

- 每次点击谢礼都只刷新这两个 Text 并显示同一个 `Canvas_GiftDialog`，不要 Create Widget。
- `Canvas_GiftDialog` 必须位于 `Canvas_Success` 中且 ZOrder 高于谢礼和继续按钮。
- 因为它只存在于 Success 页，切到 Failure 页时 Widget Switcher 会自动隐藏它。
- 如果对话框打开时不希望误点继续按钮，关闭热区应覆盖可见对话框，且对话框 ZOrder 保持 `100`。
- 点击关闭只隐藏 `Canvas_GiftDialog`，不能调用 `Confirm Success`、`Confirm Retry` 或 Remove From Parent。

完成后点击 **Compile → Save**。

## 8. 让 Day HUD 找到结算 Widget

推荐显式配置，避免资产改名后失联：

1. 打开 `/Game/Day/UI/WBP_SDayHUD`。
2. 点击 **Class Defaults**。
3. 搜索分类 `Day | Settlement`。
4. 将 **Settlement Widget Class** 设置为 `WBP_DaySettlement`。
5. Compile → Save。

即使不配置，只要 Widget 保持准确路径
`/Game/Day/UI/Settlement/WBP_DaySettlement`，C++ 也会自动加载。

结算出现时 HUD 会自动：

- Add To Viewport，ZOrder 200
- 切为 UI Only 输入
- 保持鼠标可见
- 结算结束后移除 Widget，并恢复 Game And UI 输入

## 9. PIE 验收

### 失败流程

1. 打开 `/Game/Day/Maps/L_S_DayWhitebox` 并 PIE。
2. 如当前未进入白天，在控制台执行 `S.Day.OpenDay`。
3. 营业额未达标时等待倒计时结束，或使用现有“闭店”调试按钮。
4. 应停留在 Failure 结算页，不能继续操作底层棋盘。
5. 点击“重新经营”。
6. 验证：Stage 不变、营业额恢复为 0、棋盘清空、日初库存和订单队列恢复、白天重新计时。

### 成功流程

1. PIE 中打开右上角修改器，将 Revenue 提升到目标；或正常经营至达标。
2. 等待闭店。
3. 应停留在 Success 结算页，不应自动进入夜图。
4. 逐个点击谢礼，确认名称和 `DT_Gifts.EffectText` 正确显示。
5. 点击继续。
6. 验证：只推进一次，并进入 `/Game/Night/Course/Maps/L_Night_G1_ForkTest`。

### 自动烟测

控制台执行 `S.Day.RunSmoke`。更新后的烟测会分别验证：

- 未达标先进入 Failure Settlement，再确认回档。
- 达标先进入 Success Settlement，再确认推进。
- 日初快照恢复、结转目标和夜晚准备状态仍正确。

## 10. 当前存档语义

本次没有升级 `SG_ChefProfile` 版本。玩家在结算页强退后重新读档，仍沿用工程原规则：
白天中途状态回到日初，而不是恢复结算弹窗。若后续产品要求“重启后仍停在结算页”，需要再把
`FSDaySettlementData` 写入 SaveGame 并做 v3 → v4 存档迁移。
Height
