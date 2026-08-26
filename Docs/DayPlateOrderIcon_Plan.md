# 餐盘订单图标 — 纯蓝图方案（尽量不改 C++）

> 目标：在每个顾客/ NPC 的餐盘上显示 TA 点的那一道菜（一个图标，参考原画 `T_CookingUI_Concept`）。  
> 适用阶段：prototype。本方案**不修改任何 C++**，全部在 UE 编辑器里用蓝图完成。  
> 状态：方案已核对代码可见接口，待在编辑器里照步骤搭建。

---

## 1. 为什么纯蓝图可行（已核对的代码事实）

| 依据                                                                                   | 位置                                  | 说明                                             |
| ------------------------------------------------------------------------------------ | ----------------------------------- | ---------------------------------------------- |
| `ASDayCharacterStandIn` 是 `Blueprintable`                                            | `SDayBoardPresentation.h:278`       | 可创建/编辑蓝图子类                                     |
| 项目已有 `BP_SDayCharacterStandIn.uasset`                                                | `Content/Day/Board/`                | 直接改这个蓝图即可                                      |
| `OnSeatOccupied(OccupantKey, bSpecialNpc)` 是 `BlueprintImplementableEvent`           | `SDayBoardPresentation.h:302`       | C++ 顾客入座时会调用，蓝图可**实现**它                        |
| `OnSeatVacated()` 同上                                                                 | `SDayBoardPresentation.h:306`       | 顾客离开时调用，用于清图标                                  |
| `SeatIndex` 是 `VisibleAnywhere, BlueprintReadOnly`                                   | `SDayBoardPresentation.h:343`       | 蓝图里能读当前座位号                                     |
| `VisualRoot` 是 `VisibleAnywhere, BlueprintReadOnly`                                  | `SDayBoardPresentation.h:359`       | 蓝图里能拿到挂载点                                      |
| `ASCustomerDirector::TryGetCustomerAtSeat(SeatIndex, OutCustomer)` 是 `BlueprintPure` | `SStandaloneSandbox.h:1295`         | 蓝图可调用，返回 `FSCustomerState`                     |
| `FSCustomerState.Order` 是 `BlueprintType`                                            | `SStandaloneSandbox.h:426`          | 蓝图可拆出 `Order` 字段                               |
| `FSOrderRequest.IngredientId`(FName) / `Level`(int32) 均 `BlueprintReadWrite`         | `SStandaloneSandbox.h:390-405`      | 蓝图直接读订单的食材与等级                                  |
| `ASSpecialNpcDirector::GetNpcs()` 返回 `TArray<FSSpecialNpcState>`                     | `SStandaloneSandbox.h`              | NPC 订单从这条取                                     |
| 食物纹理命名约定                                                                             | `SDayBoardPresentation.cpp:875,952` | `/Game/Day/Art/food/food_<stem>_V<level>`      |
| 图标材质                                                                                 | `SDayBoardPresentation.cpp:1230`    | `/Game/Day/Materials/M_SDayDishIcon`，参数名 `Tex` |

**关键结论**：蓝图能拿到「谁坐哪个座、点了什么」，也能自己拼出纹理路径并套材质。唯一不能复用的是 C++ 私有的 `ResolveDishIcon()`，但 5 种食材的 stem 是固定的，蓝图用 `Switch on Name` 硬编即可，无需调用它。

食材链 → stem 映射（来自 `DefaultDishArtStem()`，`SDayBoardPresentation.cpp:878`）：

| IngredientId | stem | 食物   |
| ------------ | ---- | ---- |
| LingGu       | rice | 米饭   |
| YinShanJun   | egg  | 蛋    |
| ChiYanJiao   | hand | 手（爪） |
| YueLinYu     | fish | 鱼    |
| XuanYuQin    | leg  | 腿    |



---

## 2. 前置：在 `BP_SDayCharacterStandIn` 里加一个 Plane 组件

1. 打开 `Content/Day/Board/BP_SDayCharacterStandIn`。
2. 在 Components 面板点 **Add Component → Static Mesh**，命名为 `OrderIconPlane`。
3. 把它 **Attach 到 `VisualRoot`**（不要挂根，挂到 VisualRoot 下才能随座位移动）。
4. 设置 `OrderIconPlane`：
   - Static Mesh：`/Engine/BasicShapes/Plane`（或项目里已有的薄面片）
   - Collision：`No Collision`
   - Cast Shadow：`false`
   - Visibility：默认 **取消勾选**（有订单才显示）
   - 相对位置（相对 VisualRoot，原型值，按美术实测微调）：`X=0, Y=0, Z=-55`（落到餐盘高度）
   - 相对缩放：先设 `1.2`（约 120cm，比锅里的菜小一点；原画每个盘只放一道）
5. 编译保存。

> 朝向说明：UE 的 Plane 默认法线朝 +Z（朝上）。要让图标正对 portrait 相机，二选一：
>
> - 在 `OnSeatOccupied` 里设 `SetRelativeRotation` 的 Yaw = `90`（与现有 `PieceIcon` 的默认 Yaw=0 + CameraPush 推相机方向一致思路，试试看）；
> - 或加一个 Event Tick → `Find Look At Rotation`（Plane → Camera）→ `SetWorldRotation`，确保永远正对屏幕。  
>   原型阶段以「看着正对相机」为准，哪种方便用哪种。

---

## 3. 事件图 1：`OnSeatOccupied` 逐节点手册

> 已核对：下面用到的 C++ 方法都带 `UFUNCTION(BlueprintCallable/Pure)`，蓝图里**能直接搜到**。  
> `ASCustomerDirector::FindDirector`（h:1315）、`ASSpecialNpcDirector::FindDirector`（h:1373）、  
> `TryGetCustomerAtSeat`（h:1295，BlueprintPure）、`GetNpcs`（h:1364，BlueprintPure）。

### 3.0 先实现这个事件

- 打开 `Content/Day/Board/BP_SDayCharacterStandIn` → Event Graph。
- 左侧 **My Blueprint → Events** 列表里找到 `OnSeatOccupied` → **双击**（或右键 Override）。
- 事件节点进来有两个输入引脚：`OccupantKey`（字符串）、`bSpecialNpc`（布尔）。后续都从它的执行引脚（白色三角）拉线。

### 3.1 节点搜索速查表

| 作用             | 蓝图右键搜索框输入                                 | 备注                                                                          |
| -------------- | ----------------------------------------- | --------------------------------------------------------------------------- |
| 分支             | `Branch`                                  | Condition 接 `bSpecialNpc`                                                   |
| 取顾客 Director   | `Find Director`                           | 出现**两个**，选 **ASCustomerDirector** 那个（类别 S Customers）                        |
| 按座位取顾客         | `Try Get Customer At Seat`                | 属于 ASCustomerDirector                                                       |
| 取 NPC Director | `Find Director`                           | 选 **ASSpecialNpcDirector** 那个（类别 S NPC）                                     |
| 取 NPC 数组       | `Get Npcs`                                | 属于 ASSpecialNpcDirector                                                     |
| 遍历数组           | `For Each Loop`                           | 从 Get Npcs 输出引脚拖出会自动出现（普通版**无 Break 引脚**；要提前退出用 `For Each Loop With Break`） |
| 拆顾客结构          | `Break FSCustomerState`                   | 从 Out Customer 引脚**右键**选 Split Struct Pin（Break 不在搜索框）                      |
| 拆订单结构          | `Break FSOrderRequest`                    | 从 Order 引脚**右键**选 Split Struct Pin（Break 不在搜索框）                             |
| 按名分派           | `Switch on Name`                          | 输入接 IngredientId；手动加 5 个 case                                               |
| 拼字符串           | `Append` 或 `Build String`                 | 拼路径                                                                         |
| 加载纹理           | `Load Asset`                              | 搜 `Load`；路径接拼好的字符串                                                          |
| 建动态材质          | `Create Dynamic Material Instance`        | Parent 选 M_SDayDishIcon                                                     |
| 设纹理参数          | `Set Texture Parameter Value`             | Parameter Name 填 `Tex`                                                      |
| 设组件材质          | `Set Material`                            | Target=OrderIconPlane，Element Index=0                                       |
| 设可见            | `Set Visibility`                          | Target=OrderIconPlane，New Visibility=true                                   |
| 建局部变量          | (My Blueprint → Variables → New Variable) | 类型 Name/int32/String，面板里建，不搜                                                |
| Get/Set 变量     | 搜变量名（如 `Get IngredientId`）                | 从变量拖出自动出现；Set 的 Value 引脚可直接打字                                               |
| 调用自定义事件        | 搜自定义事件名（如 `ShowDishIcon`）                 | 两条分支结尾各调用一次，汇合到同一段逻辑                                                        |

> ⚠️ **Break / Split 节点不在空白搜索框搜不到**，必须**右键点击结构体引脚本身**（如 `Out Customer`、`Order`）→ 菜单里选 `Split Struct Pin`（或 `Break FSCustomerState`）。右键也没有 → 先编译项目 C++ 再试（结构体是 C++ 定义的，需编译进蓝图）。

> 所有 `Find Director` 的 **World Context Object** 引脚连 `self`（拖入当前蓝图自身引用）即可。

### 3.2 顾客分支（bSpecialNpc = False）逐节点

1. 从 `OnSeatOccupied` 的 `bSpecialNpc` 引脚拖出 → 搜 `Branch` → 连到 Branch 的 **Condition**。
2. 从 Branch 的 **False** 执行引脚拉线，开始这条链：
3. 搜 `Find Director` → 选 **ASCustomerDirector** 那个 → **World Context Object** 连 `self` → 输出 `Customer Director`。
4. 从 `Customer Director` 拖出 → 搜 `Try Get Customer At Seat` →
   - **Seat Index** 引脚：连 `self` 的 `Seat Index`（蓝图里 self 有这个只读属性，搜 `Seat Index` 选 self 的）。
   - 输出 **Out Customer**（FSCustomerState）留作下一步。该节点是纯节点（无执行引脚），放数据链即可。
5. **右键 `Out Customer` 引脚**（不是搜 Break）→ 选 `Split Struct Pin`（或 `Break FSCustomerState`）→ 引脚拆成多个字段，拖出其中的 `Order`（FSOrderRequest）。
   - 右键菜单也没有 Break/Split → 先编译项目 C++ 再回来试。
6. **右键 `Order` 引脚** → `Split Struct Pin` → 拖出 **IngredientId**（Name）、**Level**（整数）。
7. 把拆出的 `IngredientId` / `Level` 写进局部变量并触发显示：
   - `Set IngredientId`（值 = 刚拆出的 IngredientId）
   - `Set Level`（值 = 刚拆出的 Level）
   - 从 `Set Level` 的执行引脚拉出 → 搜自定义事件 `ShowDishIcon` → **调用它**（搜不到就见 3.4 顶部「如何调用自定义事件」）。

### 3.3 NPC 分支（bSpecialNpc = True）逐节点

1. 从 Branch 的 **True** 执行引脚拉线，开始这条链：
2. 搜 `Find Director` → 选 **ASSpecialNpcDirector** 那个 → **World Context Object** 连 `self` → 输出 `NPC Director`。
3. 从 `NPC Director` 拖出 → 搜 `Get Npcs` → 输出 `Npcs`（数组）。
4. 从 `Npcs` 拖出 → 选 `For Each Loop` → 循环体有 `Loop Body`（执行）、`Array Element`（单个 FSSpecialNpcState）。
   - 普通 `For Each Loop` **没有 Break 引脚**，会遍历整个数组；不匹配的元素走 Branch 的 False 分支（什么都不做），不退出也完全没问题。想「找到第一个就停」用 **`For Each Loop With Break`**（多一个 Break 输入引脚）。
5. 循环体内：**右键 `Array Element` 引脚** → `Split Struct Pin`（或 `Break FSSpecialNpcState`）→ 拖出 `Seat Index`、`b Served`、`Order`。
6. 加 `Branch`：条件 =（Array Element 的 Seat Index `==` self.Seat Index）**且**（`b Served == false`）。
   - 相等用 `==`（整数），与运算用 `AND` 节点。
7. 该 Branch 为 True 时：**右键 `Array Element` 的 `Order` 引脚** → `Split Struct Pin`（或 `Break FSOrderRequest`）→ 得 **IngredientId**、**Level** → 在 Branch 为 True 时 `Set IngredientId` + `Set Level`（值取刚拆出的），再**调用自定义事件 `ShowDishIcon`**（无需退出循环，逻辑共用）。

### 3.4 共用显示段：用「局部变量 + 自定义事件」汇合两条分支

**先解释你踩的两个坑（这是关键，不是你操作错）：**

> **坑 1：两个 `IngredientId` 接同一个 `Switch on Name` 会「接一个断一个」。**  
> 蓝图规则：**一个输出引脚只能连一根线，一个输入引脚也只能收一根线**。所以顾客分支和 NPC 分支的 `IngredientId` 没法"并线"塞进同一个 `Switch`。  
> **解法**：用**局部变量**当汇合点。两条分支各自把值 `Set` 进同一个 `IngredientId` / `Level` 变量，然后各自去"调用同一个自定义事件"。自定义事件的执行引脚是独立的，等于两条执行链走到同一段逻辑，而不是把线硬并到一起。

> **坑 2：case 后面搜不到 `Make Literal String`。**  
> 搜索框默认开「上下文相关（Context Sensitive）」，case 执行引脚后面还没有"需要字符串"的上下文，所以被过滤掉。  
> **解法（三种任选）**：① 直接用 `Append` / `Build String` / `Set` 节点的**值输入框打字**——点进引脚直接输入 `rice`，UE 会自动生成隐藏的 literal，根本不必搜 `Make Literal String`；② 搜 `Make Literal String` 时**取消勾选「Context Sensitive」**&#x5C31;能搜到；③ 用 `Select` 节点替代 `Switch`+拼路径（见第 6 节更稳方案）。

**前置准备（在 My Blueprint 面板建变量和事件）：**

1. **Variables** → New Variable：`IngredientId`（类型 `Name`）、`Level`（类型 `int32`）、`Stem`（类型 `String`）。
2. **Events** → Event Graph 空白处右键 → **Add Custom Event**，命名 `ShowDishIcon`（它就是两条分支共用的"显示逻辑"，本身不带输入引脚，靠读上面三个变量工作）。

**改 3.2 / 3.3 的结尾（拿到 IngredientId、Level 后）：**

- `Set IngredientId`（值 = 拆出的 IngredientId）→ `Set Level`（值 = 拆出的 Level）→ 从 `Set Level` 的执行引脚拉出 → 搜 `ShowDishIcon` → **调用它**。
- 如何调用自定义事件：搜索框直接输入事件名 `ShowDishIcon` 即可；或在 My Blueprint → Events 里把它拖进图里。它显示为入口节点，从右侧执行引脚（红色三角）拉线到后续节点。

**`ShowDishIcon` 自定义事件内部逐节点：**

1. 搜 `Switch on Name` → **输入接 `Get IngredientId`**（从 `IngredientId` 变量拖出，不是从某条分支的引脚）。选中节点，细节面板点 **+** 加 5 个 case，名分别为 `LingGu` / `YinShanJun` / `ChiYanJiao` / `YueLinYu` / `XuanYuQin`。
   - 每个 case 里 `Set Stem`，值**直接打进 Set 节点的 Value 输入框**：`rice` / `egg` / `hand` / `fish` / `leg`（点引脚输入即可，无需搜 `Make Literal String`）。
   - 所有 case 都 `Set Stem` 后，退出 Switch（Default case 留空不显示）统一从 `Stem` 变量继续拼路径——这样只有一处拼路径。
2. 搜 `Build String`（推荐，一个节点能放多段；`Append` 只能拼两段、要嵌套）拼出纹理路径字符串：
   - 在 `Build String` 的输入引脚依次填：**A** = `/Game/Day/Art/food/food_`（直接打字）、**B** = `Get Stem`（从 `Stem` 变量拖出）、**C** = `_V`（直接打字）、**D** = `Level`（整数，接字符串引脚 UE 自动变成 `2`；想显式控制就先过 `To Text (int)`）。
   - `Separator`（分隔符）引脚**留空**。
   - 输出 `Result` 就是完整路径，`Level` 可取 **1-5**（5 个等级）：例 `LingGu Lv3` → `/Game/Day/Art/food/food_rice_V3`。全量共 **5 种食物 × 5 等级 = 25 张纹理**，一条路径代码全部覆盖。
3. **把路径变成真正的纹理对象**（关键概念：`Build String` 吐出的只是「文字路径」，UE 不会自动按文字去读文件，必须显式加载）。
   - 一共 **5 种食物 × 5 个等级 = 25 张纹理**（`food_rice_V1`…`food_rice_V5`、`food_egg_V1`… 等），等级由第 1 步的 `Level` 变量（取值 1-5）决定。
   - **推荐：路径加载（方案 A）**——搜 `Load Asset` → **Path** 接上一步 `Build String` 的 `Result`（其中已含 `_V` + `Level`）→ 输出就是那张纹理（Texture2D 对象），直接接第 5 步 `Set Texture Parameter Value` 的 **Value**。**一条代码路径自动覆盖全部 25 张**，无需手动连 25 个引用、也无需管等级分支。
   - 注意：`Load Asset` 是**同步加载**，触发瞬间会极短暂卡一帧；本方案只在"有人坐下"时触发一次，原型可忽略。
   - 关于上一版提到的「拖纹理 + Select」：那只适用于"一种食物一张图"。现在有 25 张（还要按 Level 选），用 Select 得先按 IngredientId 选食物、再按 Level 选等级——要么嵌套两层 Select、要么手动拖 25 个引用，反而更麻烦。**所以本方案改用路径加载更划算**。若以后只想显示每个食物的固定一张（忽略等级），再退回"拖 5 张 + Select by IngredientId"也行。
   - 若版本搜不到 `Load Asset`，用 `Async Load Asset`（异步、带完成回调，较复杂）走同一条路径。
4. 搜 `Create Dynamic Material Instance`（**务必在事件图空白处右键自己搜一个独立的**，而**不是**右键 `OrderIconPlane` 组件选"Create Dynamic Material Instance"——后者会自动沿用组件当前材质、不显示 Parent 引脚，不适合本方案）。独立节点带一个空的 **Parent** 输入引脚：
   - `Parent` 不是 Details 面板里的按钮，是节点左侧的一个**材质类型输入引脚**。给它填值：在 Content Browser 找到 `/Game/Day/Materials/M_SDayDishIcon`，**直接拖进蓝图图**生成"材质引用"节点，再从该引用节点的输出引脚拉线连到 `Create Dynamic Material Instance` 的 **Parent** 引脚。
   - 节点右侧输出 `Return Value` = 动态材质实例，接第 5 步。
5. 从动态材质实例拖出 → 搜 `Set Texture Parameter Value` → **Parameter Name** 填 `Tex`（不带引号），**Value** 接第3步的 `Texture`。
6. 搜 `Set Material` → **Target**=OrderIconPlane，**Material**=动态材质实例，**Element Index**=`0`。
7. 搜 `Set Visibility` → **Target**=OrderIconPlane，**New Visibility**=勾选 true。
8. 朝向：接在 `Set Visibility(true)` 之后（见第 2 节 / 3.5），推荐单次设置。

### 3.5 朝向（重要，原型阶段）

图标显示后需正对相机（billboard）。**白天相机固定**，所以只需在入座时算一次朝向，**不必每帧 Tick**。把下面整段接在 `Set Visibility(true)` 的执行引脚之后即可。

> 原理：UE 的 Plane 默认**面法线朝 +Z（朝上）**。`Find Look At Rotation` 算出的旋转是让物体的 **+X（前向）** 指向相机，所以直接套会让图标"侧躺"——必须再绕 Y 轴补 **-90°** 偏移，让平面的 +Z 正对相机。

**`ShowDishIcon` 内逐节点（接在 Set Visibility 之后）：**

1. **拿图标平面世界坐标**：在 Components 面板把 `OrderIconPlane` 拖进图 → 从它拖出搜 `Get World Location`（或 `Get Component Location`）→ 输出 `Plane World Loc`（类型是**向量 Vector**，平面在场景里的位置）。
2. **拿相机世界坐标**：图空白处右键搜 `Get Player Camera Manager` → 这个节点**没有输入引脚**（本身就是"取得相机管理器"的动作）→ 从它的输出引脚拖出搜 `Get Camera Location` → 输出 `Cam Loc`（也是**向量**，相机位置）。
3. **⚠️ 这两条线互不相连**：`Plane World Loc` 和 `Get Player Camera Manager` 是不同类型（位置向量 vs 相机管理器对象），**本来就不能互接**，连不上是对的。它们各自去喂 `Find Look At Rotation` 的两个不同引脚：搜 `Find Look At Rotation` → **Start** 接 `Plane World Loc`（平面在哪），**Target** 接 `Cam Loc`（相机在哪）→ 输出 `Look Rot`（此旋转让平面 +X 指向相机）。
4. **补 -90° 偏移**（关键，否则图标侧躺：Plane 面法线是 +Z，而 Look Rot 让 +X 指向相机，需把 Yaw 减 90 让 +Z 正对相机）：
   - 从 `Look Rot` 拖出 → 搜 `Break Rotator` → 拆出 `Pitch` / `Yaw` / `Roll` 三个浮点。
   - 从 `Yaw` 拖出 → 搜 `-`（减法 `float - float`）→ 第二个数填 `90` → 得到 `Yaw - 90`。
   - 搜 `Make Rotator` → **Pitch** 接 Break 的 `Pitch`，**Yaw** 接 `Yaw - 90`，**Roll** 填 `0` → 输出 `Final Rot`（= 让平面 +Z 正对相机的旋转）。
   - 若 `Break Rotator` / `Make Rotator` 也搜不到，关掉搜索框的「Context Sensitive」再搜；它们是基础数学节点，一定存在。（旧版用的 `Compose Rotators` 在部分版本被上下文搜索隐藏，故改此写法。）
5. **应用旋转**：搜 `Set World Rotation` → **Target**=`OrderIconPlane`，**New Rotation** 接 `Final Rot`。

**调试提示**：若跑起来图标是"侧躺 / 上下颠倒"，把第 4 步的 `Yaw - 90` 改成 `Yaw + 90`，或微调 `Make Rotator` 的 `Roll`——这是纯视觉微调，数值取决于你美术摆的 Plane 初始朝向，试一次就定。

**备选（不推荐，仅相机移动时才用）**：若未来白天相机也会动，就把第 1 步换成在 Event Graph 搜 `Event Tick`，重复 1-5 每帧刷新。本原型相机固定，每帧重算浪费，故用上面"单次设置"。

---

> 上一版概览（仅供对照，以本逐节点手册为准）：
>
> ```
> OnSeatOccupied → Branch(bSpecialNpc)
>   True  → NPC 分支（FindDirector→GetNpcs→ForEach 找本座位→Order）
>   False → 顾客分支（FindDirector→TryGetCustomerAtSeat(self.SeatIndex)→Order）
> → Break FSOrderRequest → Switch on Name → 拼路径 → Load → 动态材质设 Tex → Set Material → Set Visibility
> ```

---

## 4. 事件图 2：`OnSeatVacated`（清图标）

```
OnSeatVacated
  └─ OrderIconPlane.SetVisibility(false)
```

（动态材质实例可保留复用，下次入座再设 Tex 即可；不清除也不会出错。）

---

## 5. 边界与注意

1. **`DishIconOverrides` 特例**：C++ 版 `ResolveDishIcon` 会先查 `USDayBoardVisualConfig::DishIconOverrides`（每链每级显式贴图），查不到才用命名约定。纯蓝图方案直接用命名约定，**会漏掉** `DishIconOverrides` 里配置的特例纹理。
   - 当前 5 种食材原型阶段都走命名约定，不受影响。
   - 若以后美术用 `DishIconOverrides` 替换了某级贴图，蓝图需同步改 `Switch` 或改用下面第 6 节的极小 C++ 暴露函数。
2. **Level 范围**：`Level` 已 `Clamp(0,4)` 在 C++ 侧保证，蓝图直接拼即可；若某座显示了空白图标，先确认该 `food_<stem>_V<level>` 资产确实存在。
3. **加载时机**：`Load Asset` 首次有轻微同步开销，原型可接受；追求无缝可改用 `Async Load Asset` + 完成回调再显。
4. **朝向/大小**：以「正对相机、适配餐盘」为准在编辑器里微调，不写死在 C++。

---

## 6. 可选增强：加 1 个 5 行的 BlueprintCallable 包装（更稳、对美术无感）

如果想完全复用 C++ 的 `ResolveDishIcon`（含 `DishIconOverrides`、缓存、命名约定），只改 **1 个函数**，在 `SDayBoardPresentation.h/.cpp` 暴露：

```cpp
// h（USDayBoardVisualConfig 已包含 stem/override 映射）
UFUNCTION(BlueprintCallable, BlueprintPure, Category = "S Day Board")
static UTexture2D* ResolveDishIconForBP(USDayBoardVisualConfig* Config, FName IngredientId, int32 Level);
```

```cpp
// cpp（直接转发现有私有的 ResolveDishIcon）
UTexture2D* ASDayCharacterStandIn::ResolveDishIconForBP(
    USDayBoardVisualConfig* Config, FName IngredientId, int32 Level)
{
    return DayBoardPresentationPrivate::ResolveDishIcon(Config, IngredientId, Level);
}
```

蓝图里就**不需要** `Switch on Name` 和拼路径，直接 `ResolveDishIconForBP(Config, IngredientId, Level)` → 拿到纹理。代价是动 1 个 C++ 文件、需重新编译。

---

## 7. 验收标准

- PIE 跑白天场景 → 顾客入座，餐盘出现该顾客点的食物图标（如 LingGu Lv2 → rice_V2）。
- NPC 入座 → 对应餐盘出现 NPC 想要的食物图标。
- 顾客被服务离开 / 空座 → 图标消失。
- 图标正对相机、大小适配餐盘，不遮挡顾客头像。
- 多顾客并存时各自餐盘显示各自订单，不串。
- 日志无 `LogStreaming` 找不到纹理的报错（路径拼对）。

---

## 8. 落地方式说明

- **纯蓝图方案**：需在 UE 编辑器里手动连节点（见第 2–4 节），我无法用代码生成 `.uasset` 蓝图文件，只能提供这份逐节点指南。
- **若希望我直接落地**：走第 6 节的「加 1 个 C++ 暴露函数 + 蓝图用 BP 函数」最稳；或沿用上轮给的「纯 C++ 在 `ASDayCharacterStandIn` 加 `OrderIconMesh`」方案，我可直接改 `.h/.cpp`。
