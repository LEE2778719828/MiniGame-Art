# S 日间合成面板——小鬼火焰特效接入指南

> 适用项目：MiniGame  
> 适用关卡：`/Game/Day/Maps/L_S_DayWhitebox`  
> 目标资产：`/Game/Day/Blueprints/BP_SDayCanguan`  
> 特效来源：`/Game/Day/VFX`  
> 面向读者：第一次接触 Unreal Engine、Blueprint 和 Niagara 的开发者

---

## 1. 最终要做成什么

原画里的四只小鬼已经属于餐厅场景资产，不需要重新建模。我们要在它们现有的红色火焰轮廓上增加动态效果：

1. 火焰贴着每只小鬼的外轮廓轻微翻动。
2. 火焰以红色为主，局部带橙黄高亮。
3. 少量火星从火焰尖端飘出。
4. 四只小鬼的动画不能完全同步。
5. 火焰不能遮挡棋盘上的菜品和操作区域。
6. 不调整现有摄像机。

推荐使用两层表现：

- **基础层**：四个 Niagara Component，负责火舌和火星。
- **增强层**：可选的动态材质，负责让原画中已经画好的红黄火焰区域产生轻微明暗闪烁。

先完成 Niagara 基础层，确认四个位置正确后，再做动态材质。不要一开始同时修改所有内容。

---

## 2. 开始前先认识项目里的相关资产

### 2.1 小鬼在哪里

小鬼不属于单独的角色 Blueprint。它们位于：

```text
/Game/Day/Blueprints/BP_SDayCanguan
```

其中负责显示小鬼的组件名是：

```text
guai
```

它使用的静态网格是：

```text
/Game/Day/Art/canguan/guai
```

四只小鬼被做在同一个静态网格里，因此不能把它们当作四个独立角色处理。正确做法是在这个餐厅 Blueprint 中建立四个独立的特效锚点。

### 2.2 特效目录里最有用的资产

```text
/Game/Day/VFX/Niagara/Min_Customer_001
/Game/Day/VFX/Niagara/Min_FireDebuff_Character
/Game/Day/VFX/Niagara/Min_FoodMerge_001
/Game/VFX_Merge/Niagara/BOX/Min_BoxOpen_FoodBurst
```

用途分别是：

- `Min_Customer_001`：已经有循环 Sprite、Spawn Rate 和 SubUV，最适合当作新系统的结构模板。
- `Min_FireDebuff_Character`：可以参考颜色、亮度和角色周围燃烧的节奏，但不要原样挂到小鬼上。
- `Min_FoodMerge_001`：适合作为合成成功时的一次性爆发。
- `Min_BoxOpen_FoodBurst`：其中使用了火焰序列帧材质，可用来核对 Sprite Renderer 的 SubUV 设置。

推荐复用的材质实例：

```text
/Game/Day/VFX/MF/MiniGame/M_Base_Simple_001_Inst4
```

该实例引用火焰序列帧：

```text
/Game/Day/VFX/Tex/Sub/MiniGame/Sarge_FireFB_C_optimized
```

### 2.3 为什么不直接使用 `Min_FireDebuff_Character`

它包含两个外部骨骼网格参数：

```text
User.Skeletal MeshA
User.Skeletal MeshB
```

而小鬼是合并后的静态网格，不是骨骼角色。直接使用可能出现以下情况：

- 粒子完全不显示。
- 特效采样错误的角色轮廓。
- 出现与小鬼形状无关的 `KAT_001` 网格。
- 四只小鬼被同一个大特效覆盖。

所以本指南会新建一个专用的 Sprite Niagara System。

---

## 3. 本指南会创建哪些资产

最终建议得到以下结构：

```text
/Game/Day/VFX/Niagara/DayBoard/
    NS_SDayGhostFire_Loop

/Game/Day/VFX/MF/DayBoard/
    MI_SDayGhostFire

/Game/Day/Art/canguan/VFX/                 （可选增强层）
    M_SDayGhostAnimated
    MI_SDayGhostAnimated
```

Blueprint 中会增加：

```text
BP_SDayCanguan
└─ Stall
   ├─ FX_Ghost_TopLeft
   │  └─ NC_Ghost_TopLeft
   ├─ FX_Ghost_TopRight
   │  └─ NC_Ghost_TopRight
   ├─ FX_Ghost_Right
   │  └─ NC_Ghost_Right
   └─ FX_Ghost_BottomLeft
      └─ NC_Ghost_BottomLeft
```

所有新资产都放在新的子目录中，不直接改美术交付的 Niagara 和材质原件。

---

## 4. 第一阶段：检查资产引用

这一阶段不要编辑特效，只确认资源没有丢失。

### 4.1 打开 Content Browser

1. 启动项目并打开 Unreal Editor。
2. 如果底部没有 Content Drawer，按 `Ctrl + Space`。
3. 在路径栏输入：

```text
/Game/Day/VFX
```

### 4.2 检查关键 Niagara

依次双击打开：

```text
/Game/Day/VFX/Niagara/Min_Customer_001
/Game/Day/VFX/Niagara/Min_FireDebuff_Character
/Game/VFX_Merge/Niagara/BOX/Min_BoxOpen_FoodBurst
```

每次打开后检查：

1. Niagara 编辑器右上角没有红色编译错误。
2. Emitter 列表中没有黄色或红色警告标记。
3. Sprite Renderer 的 Material 不是 `None`。
4. 预览窗口不是默认白方块或棋盘格。

### 4.3 检查迁移后的路径

目前部分依赖记录仍可能显示旧路径：

```text
/Game/VFX/...
```

实际资产路径是：

```text
/Game/Day/VFX/...
```

操作方法：

1. 回到 Content Browser。
2. 右键 `/Game/Day` 文件夹。
3. 选择 `Fix Up Redirectors in Folder`。
4. 再次打开上面的三个 Niagara。
5. 如果 Renderer Material 为 `None`，手动重新指定 `/Game/Day/VFX` 下对应材质。

> 注意：如果菜单中没有 Fix Up Redirectors，说明该目录没有可修复的 Redirector，可以继续下一步。

### 第一阶段检查点

只有满足以下条件才进入下一阶段：

- 三个 Niagara 都能打开。
- 没有红色编译错误。
- `M_Base_Simple_001_Inst4` 可以正常打开。
- `Sarge_FireFB_C_optimized` 可以正常显示纹理内容。

---

## 5. 第二阶段：复制火焰材质实例

### 5.1 创建目录

1. 在 Content Browser 中进入：

```text
/Game/Day/VFX/MF
```

2. 在空白处右键。
3. 选择 `New Folder`。
4. 命名为：

```text
DayBoard
```

### 5.2 复制材质实例

1. 找到：

```text
/Game/Day/VFX/MF/MiniGame/M_Base_Simple_001_Inst4
```

2. 右键该资产，选择 `Duplicate`。
3. 命名为：

```text
MI_SDayGhostFire
```

4. 把它移动到：

```text
/Game/Day/VFX/MF/DayBoard
```

5. 双击打开 `MI_SDayGhostFire`。
6. 检查它的父材质和 `Main_Tex_001` 参数没有丢失。
7. 点击工具栏 `Save`。

暂时不要大幅修改颜色。先确保原材质能在 Niagara 中显示，之后再调红、橙、黄比例。

### 第二阶段检查点

确认新资产存在：

```text
/Game/Day/VFX/MF/DayBoard/MI_SDayGhostFire
```

并且打开后没有报错。

---

## 6. 第三阶段：创建循环火焰 Niagara

### 6.1 从结构模板复制

1. 在 Content Browser 中找到：

```text
/Game/Day/VFX/Niagara/Min_Customer_001
```

2. 右键，选择 `Duplicate`。
3. 命名为：

```text
NS_SDayGhostFire_Loop
```

4. 移动到：

```text
/Game/Day/VFX/Niagara/DayBoard
```

如果 `DayBoard` 文件夹不存在：

1. 进入 `/Game/Day/VFX/Niagara`。
2. 右键空白处。
3. 选择 `New Folder`。
4. 命名为 `DayBoard`。

### 6.2 重命名 Emitter

1. 双击打开 `NS_SDayGhostFire_Loop`。
2. 左侧应看到一个名为 `Minimal001` 的 Emitter。
3. 右键 Emitter 名称。
4. 选择 `Rename`。
5. 改为：

```text
E_GhostFlame_Main
```

### 6.3 设置循环模式

在 Emitter Stack 中找到：

```text
Emitter Update
└─ Emitter State
```

选中 `Emitter State` 后，在右侧 Details 中设置：

```text
Life Cycle Mode     = Self
Loop Behavior       = Infinite
Loop Duration Mode  = Fixed
Loop Duration       = 1.0
```

不同 UE 版本中枚举名称可能显示为 `Infinite`、`Once` 或 `Multiple`。目标是让 Emitter 无限循环，而不是只播放一次。

### 6.4 设置生成数量

在 Stack 中找到：

```text
Emitter Update
└─ Spawn Rate
```

把 Spawn Rate 设为：

```text
5.0
```

如果 Stack 中还有 `Spawn Burst Instantaneous`：

- 第一版建议先禁用它。
- 点击模块左侧的启用勾选框即可。

这样进入游戏时不会突然爆出一大团火焰。

### 6.5 设置粒子生命周期和大小

找到：

```text
Particle Spawn
└─ Initialize Particle
```

设置：

```text
Lifetime Mode = Random
Lifetime Min  = 0.45
Lifetime Max  = 0.80
```

Sprite Size 建议从以下范围起调：

```text
X Min = 14
X Max = 24
Y Min = 24
Y Max = 42
```

这里让火焰在竖直方向比水平方向更长。因为 `Stall` 组件本身带有缩放，进入 Blueprint 后还需要通过 Niagara Component 的整体 Scale 再做一次视觉校准。

### 6.6 设置火焰材质

1. 在 Emitter Stack 最下方找到 `Sprite Renderer`。
2. 选中它。
3. 在右侧 Details 找到 `Material`。
4. 指定：

```text
/Game/Day/VFX/MF/DayBoard/MI_SDayGhostFire
```

5. 将 `Facing Mode` 设置为：

```text
Face Camera
```

或当前版本中等价的 `Camera Facing`。

6. 把 `Sort Mode` 设置为：

```text
View Depth
```

### 6.7 核对 SubUV 设置

不要猜测火焰贴图是几行几列。请从美术现有系统中复制正确设置：

1. 保持当前 Niagara 窗口打开。
2. 在另一个编辑器标签页打开：

```text
/Game/VFX_Merge/Niagara/BOX/Min_BoxOpen_FoodBurst
```

3. 检查两个 Sprite Renderer 分别使用什么材质。
4. 找到使用以下材质的 Renderer：

```text
/Game/Day/VFX/MF/MiniGame/M_Base_Simple_001_Inst4
```

5. 记录它的：

```text
Sub Image Size X
Sub Image Size Y
Alignment
Facing Mode
Sort Mode
```

6. 回到 `NS_SDayGhostFire_Loop`。
7. 在 Sprite Renderer 中填入相同的 `Sub Image Size X/Y`。

原模板中已经存在：

```text
Particle Update
└─ Sub UVAnimation
```

保留该模块。建议设置：

```text
Playback Mode     = Infinite 或 Linear
Random Start Frame = True
```

如果没有 `Random Start Frame` 选项，可以先保持默认，之后用不同的 Niagara Component 起始延迟打散同步。

### 6.8 设置火焰尺寸曲线

找到：

```text
Particle Update
└─ Scale Sprite Size
```

目标曲线：

| Normalized Age | Scale X | Scale Y |
|---:|---:|---:|
| 0.00 | 0.15 | 0.10 |
| 0.15 | 0.90 | 0.75 |
| 0.40 | 1.00 | 1.00 |
| 0.75 | 0.65 | 1.15 |
| 1.00 | 0.05 | 0.20 |

新手操作方法：

1. 点击 `Scale Sprite Size`。
2. 将模式改为 `Non-Uniform Curve`。
3. 展开 X、Y 曲线。
4. 使用 `+` 添加关键点。
5. 按表格填写时间和值。

如果暂时不会编辑曲线，可以保留模板曲线，只要火焰能够正常出现即可。曲线属于第二轮调优内容。

### 6.9 设置颜色与透明度

找到：

```text
Particle Update
└─ Scale Color
```

推荐颜色变化：

| Normalized Age | 颜色 | Alpha |
|---:|---|---:|
| 0.00 | `#FFF1B8` | 0.00 |
| 0.10 | `#FFD85A` | 1.00 |
| 0.45 | `#FF742F` | 0.90 |
| 0.80 | `#F33D32` | 0.55 |
| 1.00 | `#8A1018` | 0.00 |

先把 Alpha 曲线做好，再调颜色。若材质是 Additive，颜色值和 Emissive 会共同影响亮度，过亮时优先降低颜色值或材质 Emissive，不要通过关闭后处理解决。

### 6.10 设置局部空间

1. 点击 Emitter 名称 `E_GhostFlame_Main`。
2. 在右侧 Emitter Properties 中找到 `Local Space`。
3. 勾选：

```text
Local Space = True
```

这会让特效跟随餐厅 Blueprint 的位置、旋转和缩放。

### 6.11 设置 Fixed Bounds

如果 Niagara 编辑器提供 System Properties：

1. 选中 `System Properties`。
2. 启用 `Fixed Bounds`。
3. 第一版可以设置一个较宽松的范围，例如：

```text
Min = (-100, -100, -100)
Max = ( 100,  100,  100)
```

接入 Blueprint 后再缩小。Fixed Bounds 太小会导致火焰靠近屏幕边缘时突然消失。

### 6.12 编译和保存

1. 点击 Niagara 编辑器工具栏的 `Compile`。
2. 确认右上角没有红色错误。
3. 点击 `Save`。

### 第三阶段检查点

在 Niagara 自己的预览窗口中，应看到：

- 火焰持续循环。
- 不是只播放一次。
- 不是白方块。
- 火焰能逐帧变化。
- 单个火舌不会大到占满整个预览窗口。

如果此时还看不到火焰，不要进入 Blueprint 接入阶段，先查看本文第 12 节的排错表。

---

## 7. 第四阶段：先在一只小鬼上做 MVP

不要一次添加四个组件。先完成左上角一只小鬼，确认显示、遮挡和缩放都正确。

### 7.1 打开餐厅 Blueprint

1. 在 Content Browser 中找到：

```text
/Game/Day/Blueprints/BP_SDayCanguan
```

2. 双击打开。
3. 切换到左上角 `Viewport` 标签。
4. 在 Components 面板找到：

```text
Stall
```

> 不要修改 Blueprint 中的 Camera，也不要调整关卡中的 `BP_DayCamera`。

### 7.2 添加锚点

1. 选中 `Stall`。
2. 点击 Components 面板顶部 `Add`。
3. 搜索 `Scene Component`。
4. 添加后命名为：

```text
FX_Ghost_TopLeft
```

5. 确认它显示为 `Stall` 的子组件。

如果它出现在错误层级：

1. 在 Components 面板拖动 `FX_Ghost_TopLeft`。
2. 放到 `Stall` 上。
3. 看到缩进后松开鼠标。

### 7.3 添加 Niagara Component

1. 选中 `FX_Ghost_TopLeft`。
2. 点击 `Add`。
3. 搜索 `Niagara Particle System` 或 `Niagara Component`。
4. 添加后命名为：

```text
NC_Ghost_TopLeft
```

5. 在右侧 Details 找到 `Niagara System Asset`。
6. 指定：

```text
/Game/Day/VFX/Niagara/DayBoard/NS_SDayGhostFire_Loop
```

7. 勾选：

```text
Auto Activate = True
```

### 7.4 粗调缩放

先把 `NC_Ghost_TopLeft` 的 Relative Transform 设为：

```text
Location = (0, 0, 0)
Rotation = (0, 0, 0)
Scale    = (0.25, 0.25, 0.25)
```

`Stall` 本身有较大的缩放，因此 Niagara Component 从 `0.25` 左右开始更安全。如果火焰仍然很大，可以降到 `0.10`；如果完全看不清，可以升到 `0.40`。

### 7.5 把锚点移到火焰根部

移动的是 `FX_Ghost_TopLeft`，不要移动 `guai`，也不要移动餐厅 Blueprint。

操作：

1. 选中 `FX_Ghost_TopLeft`。
2. 使用移动工具，快捷键 `W`。
3. 把它移到左上小鬼红色火焰的根部。
4. 只通过锚点的 Location 调整位置。
5. 如果特效被小鬼网格遮住，将锚点沿朝向摄像机的方向前推少量距离。

由于组件坐标受 `Stall` 缩放和 Blueprint 朝向影响，不建议直接照抄一个未经现场验证的 XYZ 数值。以最终游戏摄像机中的画面为准。

### 7.6 在关卡中检查

1. 点击 Blueprint 工具栏 `Compile`。
2. 点击 `Save`。
3. 打开关卡：

```text
/Game/Day/Maps/L_S_DayWhitebox
```

4. 不移动摄像机。
5. 点击 `Play`。
6. 检查左上小鬼的火焰。

第一次只判断四件事：

- 是否显示。
- 大小是否大致正确。
- 是否位于小鬼外轮廓，而不是棋盘中央。
- 是否在小鬼前面显示，但没有覆盖菜品。

### 第四阶段检查点

左上小鬼必须已经有一个位置正确、持续循环的火焰。没有达到这个结果前，不复制另外三个组件。

---

## 8. 第五阶段：复制到另外三只小鬼

### 8.1 复制锚点组件

回到 `BP_SDayCanguan`：

1. 在 Components 面板选中 `FX_Ghost_TopLeft`。
2. 按 `Ctrl + W` 复制。
3. 连续复制三次。
4. 分别重命名为：

```text
FX_Ghost_TopRight
FX_Ghost_Right
FX_Ghost_BottomLeft
```

5. 它们的 Niagara 子组件分别重命名为：

```text
NC_Ghost_TopRight
NC_Ghost_Right
NC_Ghost_BottomLeft
```

### 8.2 分别定位

依次移动四个 Scene Component 锚点：

| 锚点 | 目标位置 | 火焰主要方向 |
|---|---|---|
| `FX_Ghost_TopLeft` | 左上小鬼火焰根部 | 向左上、向外 |
| `FX_Ghost_TopRight` | 右上小鬼背部火焰根部 | 向右上、向外 |
| `FX_Ghost_Right` | 画面右侧小鬼火焰根部 | 向右、略向上 |
| `FX_Ghost_BottomLeft` | 左下或下缘小鬼火焰根部 | 向左下边缘、略向上 |

每放好一个就 `Compile → Save → Play` 检查一次。

### 8.3 打散同步

优先方案是在 Niagara 的 `Sub UVAnimation` 中开启：

```text
Random Start Frame = True
```

如果仍然明显同步，可以在每个 Niagara Component 中设置不同的随机种子：

```text
TopLeft    = 11
TopRight   = 29
Right      = 47
BottomLeft = 83
```

属性名称可能显示为 `Random Seed Offset`、`Random Seed` 或位于 Niagara Override Parameters 中。若当前系统没有该参数，可以先通过不同 Rotation、Scale 和位置让四处视觉节奏产生差异。

### 第五阶段检查点

四只小鬼都应有火焰，并满足：

- 大小不完全相同。
- 帧动画不完全同步。
- 火焰根部贴近原画红色区域。
- 火焰没有伸到棋盘主要操作区。

---

## 9. 第六阶段：添加火星 Emitter

火星属于增强效果。火焰主体没有问题后再做。

### 9.1 复制主 Emitter

1. 打开 `NS_SDayGhostFire_Loop`。
2. 右键 `E_GhostFlame_Main`。
3. 选择 `Duplicate`。
4. 重命名为：

```text
E_GhostEmbers
```

### 9.2 调整火星参数

建议设置：

```text
Spawn Rate     = 1.5
Lifetime Min   = 0.40
Lifetime Max   = 0.90
Sprite Size X  = 2 ～ 5
Sprite Size Y  = 5 ～ 10
```

保留很少的粒子，火星不应形成第二团火焰。

### 9.3 添加速度

在 `Particle Spawn` 中：

1. 点击 `+`。
2. 搜索 `Add Velocity`。
3. 添加该模块。
4. 使用较小随机速度，例如：

```text
X = -8 ～ 8
Y = -5 ～ 5
Z = 20 ～ 45
```

具体哪个轴对应屏幕向上，需要在 Blueprint 中观察。如果火星横向飞出，可以调整速度轴，不要调整摄像机。

### 9.4 添加 Drag

在 `Particle Update` 中：

1. 点击 `+`。
2. 搜索 `Drag`。
3. 添加模块。
4. 设置为：

```text
Drag = 1.5 ～ 3.0
```

### 9.5 设置颜色

火星建议：

```text
出生：浅黄白
中段：橙色
消失：深红且 Alpha = 0
```

点击 `Compile` 和 `Save` 后进入 PIE 检查。若火星比火焰更显眼，优先降低 Spawn Rate 和 Sprite Size。

---

## 10. 第七阶段：可选的原画火焰闪烁材质

这一阶段需要编辑 Material Graph。Niagara 基础层已经满足需求时，可以暂时不做。

### 10.1 复制原材质

不要直接改：

```text
/Game/Day/Art/canguan/foe
```

操作：

1. 在 `/Game/Day/Art/canguan` 下新建文件夹 `VFX`。
2. 复制 `foe`。
3. 命名为：

```text
M_SDayGhostAnimated
```

### 10.2 材质目标

材质仍然采样：

```text
/Game/Day/Art/canguan/T_foe
```

`T_foe` 没有 Alpha，因此不能简单把它当透明贴图叠一层。应该保持小鬼原图不变，只在同一个材质内增强红黄区域。

### 10.3 建立红色火焰 Mask

Material Graph 逻辑：

```text
Texture RGB
  ├─ R
  ├─ G
  └─ B

Max(G, B)
  → Multiply 0.65
  → R - Result
  → Multiply 4.0
  → Saturate
  = FireMask
```

新手添加节点方法：

1. 在图表空白处右键。
2. 搜索节点名称，例如 `Max`、`Multiply`、`Subtract`、`Saturate`。
3. 点击添加。
4. 从节点输出圆点拖线到下一个节点输入圆点。

### 10.4 添加慢速闪烁

最简单的闪烁，不需要额外噪声贴图：

```text
Time
 → Multiply 5.0
 → Sine
 → Multiply 0.12
 → Add 1.0
 = Flicker
```

然后：

```text
FireMask × Flicker × FireColor × EmissiveStrength
```

建议参数：

```text
FireColor       = (1.0, 0.18, 0.04)
EmissiveStrength = 1.5
```

最终输出：

```text
OriginalTextureRGB
  + FireMask × Flicker × FireColor × EmissiveStrength
  → Emissive Color
```

### 10.5 创建实例并替换

1. 右键 `M_SDayGhostAnimated`。
2. 选择 `Create Material Instance`。
3. 命名：

```text
MI_SDayGhostAnimated
```

4. 打开 `BP_SDayCanguan`。
5. 选中组件 `guai`。
6. 在 Details 的 Materials 中记录当前材质。
7. 将槽位临时替换为 `MI_SDayGhostAnimated`。
8. 编译、保存并进入 PIE 检查。

如果小鬼颜色、背景或轮廓明显变化，立即换回原材质。动态材质只能增强红黄火焰区域，不能改变黑色主体。

---

## 11. 第八阶段：合成成功时增强火焰

这是可选的玩法反馈，不属于常驻火焰的必要部分。

现有一次性合成特效：

```text
/Game/Day/VFX/Niagara/Min_FoodMerge_001
```

推荐表现：

1. 菜品合成成功时，在目标格子播放一次 `Min_FoodMerge_001`。
2. 四只小鬼的火焰亮度在 0.15 秒内提高。
3. 保持约 0.1 秒。
4. 再用 0.2 秒恢复常态。

C++ 合成成功逻辑位于：

```text
MiniGame/Source/MiniGame/SStandaloneSandbox.cpp
ASMergeBoard::TryDropPiece
```

对于第一次接触 UE 的开发者，建议先不要改 C++。先完成常驻火焰，随后让程序开发者增加以下接口：

```text
BP_SDayCanguan::PulseGhostFire()
```

Blueprint 内的接口职责：

```text
收到 PulseGhostFire
 → 四个 Niagara Component 提高 User.Intensity
 → Delay 或 Timeline
 → 恢复 User.Intensity
```

为了支持它，可以在 Niagara System 中暴露：

```text
User.Intensity (Float，默认 1.0)
```

然后把 `Particle Color × User.Intensity` 接到最终颜色或材质 Dynamic Parameter。

---

## 12. 常见问题排查

### 12.1 完全看不到特效

按顺序检查：

1. Niagara Component 的 `Auto Activate` 是否勾选。
2. `Niagara System Asset` 是否为 `NS_SDayGhostFire_Loop`。
3. System 和 Emitter 是否已启用。
4. `Spawn Rate` 是否大于 0。
5. `Emitter State` 是否为无限循环。
6. Sprite Renderer 是否启用。
7. Renderer Material 是否为 `MI_SDayGhostFire`。
8. Component Scale 是否过小。
9. Fixed Bounds 是否过小。
10. Niagara 编辑器预览窗口中是否能看到火焰。

如果 Niagara 预览中也看不到，问题在 Niagara；如果预览中有、关卡中没有，问题通常在组件位置、缩放、Bounds 或遮挡。

### 12.2 显示为白色方块

通常原因：

- Renderer Material 是 `None`。
- 材质没有正确读取火焰纹理。
- Sub Image Size 设置错误。
- 旧的 `/Game/VFX` 路径没有修复。

重新指定：

```text
/Game/Day/VFX/MF/DayBoard/MI_SDayGhostFire
```

并核对 `Min_BoxOpen_FoodBurst` 中原 Renderer 的 Sub Image Size。

### 12.3 火焰只播放一次

检查：

```text
Emitter State → Loop Behavior = Infinite
Spawn Rate 已启用
Spawn Burst 不是唯一生成方式
```

### 12.4 火焰尺寸巨大

原因通常是 `Stall` 的父级缩放被继承。

处理顺序：

1. 降低 Niagara Component Scale，例如 `0.25 → 0.10`。
2. 再降低 `Initialize Particle` 的 Sprite Size。
3. 不要缩放整个 `BP_SDayCanguan`。

### 12.5 火焰在小鬼后面

1. 选中特效锚点 Scene Component。
2. 沿朝向摄像机的方向前推少量距离。
3. 检查 Sprite Renderer 的 Sort Mode。
4. 必要时提高 Translucency Sort Priority。

不要通过移动摄像机解决遮挡问题。

### 12.6 火焰覆盖棋盘或菜品

1. 把锚点移回小鬼外缘。
2. 降低 Sprite Size。
3. 降低 Spawn Rate。
4. 减小速度范围。
5. 缩短 Lifetime。

### 12.7 火焰靠近画面边缘时消失

这是典型的 Bounds 问题：

1. 打开 Niagara System。
2. 启用 Fixed Bounds。
3. 暂时放大 Bounds。
4. 验证不再消失后，再逐渐缩小到合理范围。

### 12.8 四处火焰完全同步

1. 开启 `Random Start Frame`。
2. 为实例使用不同 Random Seed。
3. 为四个组件设置略微不同的 Scale，例如：

```text
0.23
0.25
0.27
0.24
```

4. 让部分组件有 5～15 度的小角度差异。

### 12.9 编辑器正常，打包后丢失

重点检查旧路径引用：

```text
/Game/VFX
```

处理：

1. 对 `/Game/Day` 执行 Fix Up Redirectors。
2. 手动重新指定材质 Parent 和 Texture 参数。
3. 打开并重新保存相关 Niagara。
4. 做一次 Development Cook。
5. 查看 Cook Log 中是否出现 `Can't find file for asset /Game/VFX/...`。

---

## 13. 性能设置建议

目标总量：四个小鬼全部开启时，活跃粒子控制在约 40～60 个。

建议：

```text
Simulation Target = CPU
Local Space       = True
Spawn Rate        = 每个锚点 4～7
火星 Spawn Rate   = 每个锚点 1～2
Niagara Light     = 不使用
Scene Collision   = 不使用
GPU Simulation    = 不需要
Cast Shadow       = 关闭
```

不要为了这类小范围二维效果加入动态点光源。原画风格主要依靠 Emissive 和颜色层级，不依赖真实照明。

---

## 14. 最终验收清单

### 视觉

- [ ] 四只小鬼都有动态火焰。
- [ ] 火焰根部贴着原画红色区域。
- [ ] 黑色小鬼主体没有被染红或变透明。
- [ ] 红色面积最大，橙黄只作为局部高亮。
- [ ] 火星稀疏。
- [ ] 四只小鬼动画不同步。
- [ ] 火焰没有覆盖主要棋盘格和菜品。
- [ ] 没有调整摄像机。

### 技术

- [ ] `NS_SDayGhostFire_Loop` 编译无错误。
- [ ] `BP_SDayCanguan` 编译无错误。
- [ ] Niagara Component 使用 Local Space。
- [ ] Fixed Bounds 不会导致画面边缘裁剪。
- [ ] `/Game/VFX` 旧路径不会在 Cook Log 中报错。
- [ ] PIE 中多次进入退出都能正常激活。
- [ ] Development 打包版本中显示正常。

### 资产安全

- [ ] 没有直接修改美术原始 Niagara。
- [ ] 没有直接修改美术原始材质实例。
- [ ] 新资产都位于 `DayBoard` 或 `VFX` 子目录。
- [ ] 所有修改过的资产都已保存。

---

## 15. 推荐执行顺序摘要

严格按以下顺序执行：

1. 检查并修复 `/Game/Day/VFX` 引用。
2. 复制 `M_Base_Simple_001_Inst4` 为 `MI_SDayGhostFire`。
3. 复制 `Min_Customer_001` 为 `NS_SDayGhostFire_Loop`。
4. 配置循环、Spawn Rate、Lifetime、Sprite Size、Material 和 SubUV。
5. 在 Niagara 预览中确认持续火焰。
6. 在 `BP_SDayCanguan` 中只接左上小鬼。
7. PIE 验证位置、大小和遮挡。
8. 复制到其余三只小鬼。
9. 打散四个实例的同步。
10. 添加少量火星。
11. 可选：增加原画材质闪烁。
12. 可选：增加合成成功时的火焰增强。
13. 做 Cook/Package 验证。

只要每一阶段的检查点都通过，最终接入不会依赖一次性“大调参”，也更容易在出现问题时找到原因。
