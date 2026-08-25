# ArtSubmit/_incoming 美术资产清单与项目对应表

> 生成时间：2026-08-25
> 范围：`E:\UEProjects\MiniGame\ArtSubmit\_incoming\`（共 **80** 个文件：4 个 `.umap` 关卡 + 76 个 `.uasset`）
> 类型判定：用脚本读取每个 uasset 内部 FName 类名（非仅看文件名）验证。

---

## 一、重要发现（先看这里）

1. **项目里已有一份未整理的镜像**：`MiniGame/Content/新建文件夹/` 几乎 1:1 复制了 `_incoming`（路径完全一致：`模型和材质/...`、`蓝图/...`）。
   - 镜像**不包含** 3 个关卡 `.umap` 和根目录散落的 `muban1.uasset` / `Lvl_FirstPerson.umap`。
   - 结论：`_incoming` 这批发过来后，已被整包丢进 `Content/新建文件夹/` 但**还没按类别归位**。下面的"正式目标目录"就是下一步该搬去的地方。

2. **与已有正式资产重名 → 覆盖冲突**（导入前必须确认是否覆盖）：
   | _incoming 资产 | 已有正式资产 | 位置 |
   |---|---|---|
   | `SM_fish`（鱼静模） | `fish.uasset` | `Night/Course/Art/Foe/` |
   | `SM_gun`（枪/棍） | `gun.uasset` | `Night/Course/Art/Foe/` |
   | `muban1` / `muban2`（桥板模板） | `muban1.uasset` / `muban2.uasset` | `Night/Course/Art/Bridge/` |
   | `yutouguai`（鱼头怪） | `fish.uasset` / `fish_moneter.fbx`（鱼怪） | `Night/Course/Art/Foe/` —— 概念疑似重叠，疑为同一怪改名重提 |

3. **一批 ~1.4KB 占位桩**：`模型和材质/` 根目录下有多个与子目录同名的微小文件（`muban1`、`muban2`、`houchuli`、`M_houchuli`、`M_dibang`、`SM_fish`、`NewMaterial`）。这些不是真实资产，是**重复导出的桩**，建议只保留子目录里的真实版。

4. **疑似引擎模板遗留**（不建议入正式目录）：`Lvl_FirstPerson.umap`、`BP_ThirdPersonGameMode`、`BP_ThirdPersonPlayerController` —— 命名是 UE 第三人称/第一人称模板默认资产。

5. **用途暂不明，需美术确认**：`NewMaterial.uasset`、`微信图片_2026..._395.uasset`（7 张微信截图参考图）、`T_yueliang`（月亮）、`canguan` 骨骼网格（2.2MB，含动画/骨架/物理，但 Day/Art/canguan 与 Night/Environment 都叫 canguan）。

---

## 二、详细清单

### A. 关卡（Maps / `.umap`）
| 来源路径 | 类型(验证) | 项目镜像 | 正式目标 | 备注 |
|---|---|---|---|---|
| `Lvl_FirstPerson.umap`（根） | World/Level | 无 | 不入正式目录 | 第一人称模板关，疑似遗留 |
| `关卡/Lvl_FirstPerson.umap` | World/Level | 无 | 不入正式目录 | 同上（重复） |
| `关卡/paoku.umap`（14KB） | World/Level（跑酷关） | 无 | `Night/Course/Maps/` | **新增**夜晚跑酷关卡 |
| `关卡/餐厅.umap`（14KB） | World/Level（餐厅/白天关） | 无 | `Day/Maps/` | **新增**白天餐厅关卡 |

### B. 蓝图（Blueprints）
| 来源路径 | 类型(验证) | 项目镜像 | 正式目标 | 备注 |
|---|---|---|---|---|
| `蓝图/ABP_Unarmed.uasset` | AnimBlueprint（徒手/无武器） | ✅ | `Night/Character/Anims/` | 主角徒手动画蓝图 |
| `蓝图/ABP_zhujue.uasset` | AnimBlueprint（主角） | ✅ | `Night/Character/Anims/` | 主角动画蓝图 |
| `蓝图/BP_zhujue.uasset` | Blueprint（主角 Pawn，含 SkeletalMesh） | ✅ | `Night/Course/Blueprints/` | 对应 `BP_NightCoursePawn` 的 ArtRoot |
| `蓝图/BP_canting.uasset` | Blueprint（餐厅 Actor，含 SkeletalMesh） | ✅ | `Day/Blueprints/` | 可能与 `BP_SDayCanguan` 重复 |
| `蓝图/BP_cantingController.uasset` | Blueprint（餐厅 PlayerController） | ✅ | `Day/Blueprints/` | |
| `蓝图/BP_cantingGameMode.uasset` | Blueprint（餐厅 GameMode） | ✅ | `Day/Blueprints/` | 可能与 `BP_DayGameMode` 重复 |
| `蓝图/BP_ThirdPersonGameMode.uasset` | Blueprint（第三人称模板 GM） | ✅ | 不入正式目录 | 引擎模板遗留 |
| `蓝图/BP_ThirdPersonPlayerController.uasset` | Blueprint（第三人称模板 PC） | ✅ | 不入正式目录 | 引擎模板遗留 |

### C. 主角（Hero / 夜形态厨师）
| 来源路径 | 类型(验证) | 项目镜像 | 正式目标 | 备注 |
|---|---|---|---|---|
| `模型和材质/模型/SKM_zhujue.uasset`（2.5MB） | SkeletalMesh | ✅ | `Night/Course/Art/Hero/` | 主角身体骨骼网格；Hero 目前仅有 `zhujue.fbx`，此 uasset 待入 |
| `模型和材质/模型/SK_zhujueSitting_Skeleton.uasset` | Skeleton（坐姿骨架） | ✅ | `Night/Course/Art/Hero/` | 主角坐姿骨架（白天烹饪姿态） |
| `模型和材质/贴图/T_zhujue.uasset`（1.4KB） | Texture2D（占位桩） | ✅ | `Night/Course/Art/Hero/` | 极小，疑似占位 |
| `模型和材质/贴图/T_zhujue1.uasset`（460KB） | Texture2D | ✅ | `Night/Course/Art/Hero/` | 主角贴图 |
| `模型和材质/材质实例/MI_zhujue.uasset` | MaterialInstance | ✅ | `Night/Course/Art/Hero/` | 主角材质实例 |
| （ABP_zhujue / BP_zhujue 见 B） | — | — | — | — |

### D. 鱼头怪 / 鱼怪（Foe）
| 来源路径 | 类型(验证) | 项目镜像 | 正式目标 | 备注 |
|---|---|---|---|---|
| `模型和材质/SM_fish.uasset`（1.4KB 桩） | StaticMesh（占位桩） | ✅ | `Night/Course/Art/Foe/` | 桩，丢弃；真实版见下 |
| `模型和材质/模型/SM_fish.uasset`（419KB） | StaticMesh（鱼） | ✅ | `Night/Course/Art/Foe/` | ⚠ 与已有 `fish.uasset` 冲突 |
| `模型和材质/M_yutouguai.uasset` | MaterialInstance（鱼头怪材质） | ✅ | `Night/Course/Art/Foe/` | ⚠ 与 `fish`/`fish_moneter` 概念重叠 |
| `模型和材质/材质实例/MI_yutouguai.uasset` | MaterialInstance | ✅ | `Night/Course/Art/Foe/` | |
| `模型和材质/贴图/T_yutouguai.uasset` | Texture2D | ✅ | `Night/Course/Art/Foe/` | |

### E. 武器 / 道具（gun）
| 来源路径 | 类型(验证) | 项目镜像 | 正式目标 | 备注 |
|---|---|---|---|---|
| `模型和材质/模型/SM_gun.uasset`（117KB） | StaticMesh（枪/棍） | ✅ | `Night/Course/Art/Foe/`（或 Props） | ⚠ 与已有 `gun.uasset` 冲突 |

### F. 模板 / 白模占位（muban）
| 来源路径 | 类型(验证) | 项目镜像 | 正式目标 | 备注 |
|---|---|---|---|---|
| `muban1.uasset`（根，1.4KB 桩） | StaticMesh（占位桩） | ❌ | `Night/Course/Art/Bridge/` | 桩，丢弃 |
| `模型和材质/muban1.uasset`（1.4KB 桩） | StaticMesh（占位桩） | ✅ | 同上 | 桩，丢弃 |
| `模型和材质/模型/muban1.uasset`（124KB） | StaticMesh（模板1 真实） | ✅ | `Night/Course/Art/Bridge/` | ⚠ 与已有 `muban1.uasset` 冲突 |
| `模型和材质/muban2.uasset`（1.4KB 桩） | StaticMesh（占位桩） | ✅ | 同上 | 桩，丢弃 |
| `模型和材质/模型/muban2.uasset`（117KB） | StaticMesh（模板2 真实） | ✅ | `Night/Course/Art/Bridge/` | ⚠ 与已有 `muban2.uasset` 冲突 |
| `模型和材质/M_muban1.uasset` | MaterialInstance（模板1 材质） | ✅ | `Night/Course/Art/Bridge/` | 注：M_ 前缀实为材质实例 |
| `模型和材质/M_muban2.uasset` | MaterialInstance（模板2 材质） | ✅ | 同上 | |
| `模型和材质/材质/M_muban.uasset`（112KB，含贴图） | Material（模板父材质） | ✅ | `Night/Course/Art/Bridge/` | |
| `模型和材质/材质实例/MI_muban1.uasset` | MaterialInstance | ✅ | `Night/Course/Art/Bridge/` | |
| `模型和材质/材质实例/MI_muban2.uasset` | MaterialInstance | ✅ | 同上 | |
| `模型和材质/贴图/T_muban1.uasset`（163KB） | Texture2D | ✅ | `Night/Course/Art/Bridge/` | 另有一份 `Night/Course/Incoming/TA_20260820/贴图/T_muban_0820_1` |
| `模型和材质/贴图/T_muban2.uasset`（127KB） | Texture2D | ✅ | 同上 | 同上 `_0820_2` |

### G. 后处理 / 环境 / 共享（houchuli、dibang、MPC）
| 来源路径 | 类型(验证) | 项目镜像 | 正式目标 | 备注 |
|---|---|---|---|---|
| `模型和材质/houchuli.uasset`（1.4KB 桩） | Material（占位桩） | ✅ | 丢弃 | 桩 |
| `模型和材质/M_houchuli.uasset`（1.4KB 桩） | Material（占位桩） | ✅ | 丢弃 | 桩 |
| `模型和材质/材质/M_houchuli.uasset`（43KB，含贴图） | Material（后处理父材质） | ✅ | `Night/Course/Art/Environment/` | 后处理体积材质 |
| `模型和材质/材质实例/MI_houchuli_Inst.uasset` | MaterialInstance | ✅ | 同上 | |
| `模型和材质/材质实例/M_houchuli_Inst.uasset` | MaterialInstance（无 MI_ 前缀） | ✅ | 同上 | |
| `模型和材质/M_dibang.uasset`（1.4KB 桩） | Material（占位桩） | ✅ | 丢弃 | 桩 |
| `模型和材质/材质/M_dibang.uasset`（19KB） | Material（底板/地面父材质） | ✅ | `Night/Course/Art/Environment/` | 地面材质 |
| `模型和材质/材质/MPC_.uasset` | MaterialParameterCollection | ✅ | `Night/Course/Materials/`（或 Shared） | 共享材质参数集 |
| `模型和材质/NewMaterial.uasset` | Material（用途不明） | ✅（子目录版） | 待定 | 根目录还有 1 个 1.4KB 桩（无镜像） |

### H. 餐厅 / 白天场景（canguan、canting）
| 来源路径 | 类型(验证) | 项目镜像 | 正式目标 | 备注 |
|---|---|---|---|---|
| `模型和材质/模型/餐厅/canguan.uasset`（2.2MB） | SkeletalMesh（餐馆主体，含 Skeleton+Material） | ✅ | `Day/Art/canguan/`（或 `Night/Course/Art/Environment/`） | 大骨骼网格，需确认是餐馆角色还是场景件 |
| `模型和材质/模型/餐厅/canguan_Anim.uasset` | AnimSequence | ✅ | `Day/Art/canguan/animation/` | |
| `模型和材质/模型/餐厅/canguan_Skeleton.uasset` | Skeleton | ✅ | `Day/Art/canguan/` | |
| `模型和材质/模型/餐厅/canguan_PhysicsAsset.uasset` | PhysicsAsset | ✅ | `Day/Art/canguan/` | |
| `模型和材质/模型/餐厅/blinn1/2/6/7.uasset`（×4） | Material（Maya blinn 导出） | ✅ | `Day/Art/canguan/` | ⚠ `Foe/`、`Environment/` 也有同名 blinn，归位时注意目录隔离 |
| `模型和材质/模型/餐厅/lambert1-6.uasset`（×6） | Material（Maya lambert 导出） | ✅ | `Day/Art/canguan/` | |
| `模型和材质/模型/餐厅/Mesh_0_material.uasset` | Material（含贴图） | ✅ | `Day/Art/canguan/` | |
| `模型和材质/材质实例/餐厅/M_canting1..11.uasset`（×11） | MaterialInstance（餐馆材质变体） | ✅ | `Day/Art/canguan/` | |
| `模型和材质/贴图/T_canting1.uasset`（700KB） | Texture2D | ✅ | `Day/Art/canguan/` | |
| `关卡/餐厅.umap`（见 A） | World/Level | 无 | `Day/Maps/` | |
| `蓝图/BP_canting*`（见 B） | — | — | `Day/Blueprints/` | |

### I. 月亮 / 参考图
| 来源路径 | 类型(验证) | 项目镜像 | 正式目标 | 备注 |
|---|---|---|---|---|
| `模型和材质/贴图/T_yueliang.uasset`（151KB） | Texture2D（月亮） | ✅ | `Day/Art/canguan/`（或 Environment 天空） | 用途待确认 |
| `模型和材质/贴图/微信图片_20260818180628_233_395.uasset` | Texture2D（微信截图参考） | ✅ | 待定（参考/UI） | |
| `..._20260818211503_247_395.uasset` | Texture2D | ✅ | 同上 | 共 7 张微信截图 |
| `..._20260818211504_248_395.uasset` | Texture2D | ✅ | 同上 | |
| `..._20260818211504_249_395.uasset` | Texture2D | ✅ | 同上 | |
| `..._20260818211505_250_395.uasset` | Texture2D | ✅ | 同上 | |
| `..._20260818211506_251_395.uasset` | Texture2D | ✅ | 同上 | |
| `..._20260818211506_252_395.uasset` | Texture2D | ✅ | 同上 | |

---

## 三、项目对应总览（按正式目录归位）

| 正式目标目录 | 来自 _incoming 的资产 | 状态 |
|---|---|---|
| `Night/Course/Art/Hero/` | SKM_zhujue、SK_zhujueSitting_Skeleton、T_zhujue、T_zhujue1、MI_zhujue、BP_zhujue、ABP_zhujue/Unarmed | 主角资产，Hero 已有 fbx+装备，补 uasset |
| `Night/Course/Art/Foe/` | SM_fish、M_yutouguai、MI_yutouguai、T_yutouguai、SM_gun | ⚠ 与已有 fish/gun 冲突 |
| `Night/Course/Art/Bridge/` | muban1、muban2（及 M_/MI_/T_ 配套） | ⚠ 与已有 muban1/2 冲突 |
| `Night/Course/Art/Environment/` | M_houchuli 系列、M_dibang、MPC_ | 后处理/地面/共享参数 |
| `Night/Course/Maps/` | paoku.umap | 新增跑酷关 |
| `Night/Character/Anims/` | ABP_zhujue、ABP_Unarmed | 动画蓝图 |
| `Night/Course/Blueprints/` | BP_zhujue | 主角 Pawn |
| `Day/Art/canguan/` | canguan 系列(骨骼/动画/骨架/物理)、blinn×4、lambert×6、Mesh_0_material、M_canting1-11、T_canting1、T_yueliang(?) | 白天餐厅场景大包 |
| `Day/Blueprints/` | BP_canting、BP_cantingController、BP_cantingGameMode | 可能与已有 BP_SDayCanguan/BP_DayGameMode 重复 |
| `Day/Maps/` | 餐厅.umap | 新增白天关 |
| 不入正式目录 | Lvl_FirstPerson.umap、BP_ThirdPerson*（模板）、各 ~1.4KB 桩、NewMaterial(待定)、微信截图(参考) | 清理/待确认 |

---

## 四、建议动作（下一步）

1. **先处理冲突**：`fish` / `gun` / `muban1` / `muban2` 在 `Foe`/`Bridge` 已有同名资产。导入前让美术确认是**覆盖更新**还是**改名并存**（尤其 `yutouguai` 与 `fish_moneter` 是否同一怪）。
2. **删除占位桩**：`模型和材质/` 根目录下的 ~1.4KB 同名文件（`muban1/2`、`houchuli`、`M_houchuli`、`M_dibang`、`SM_fish`、根 `muban1.uasset`、`NewMaterial` 桩）只保留子目录真实版。
3. **关卡单独导入**：4 个 `.umap` 没进 `新建文件夹`，需手动导入到 `Day/Maps/` 与 `Night/Course/Maps/`。
4. **清理模板遗留**：`Lvl_FirstPerson.umap`、`BP_ThirdPerson*` 建议移出正式流程（或归入 `_archive`）。
5. **模板批量归位**：`新建文件夹/` 全量按上表搬到正式目录后，可删除 `新建文件夹/` 这个临时镜像。
6. **待美术拍板**：`canguan` 骨骼网格归属（Day 角色 vs Night 环境）、`NewMaterial` 用途、7 张微信截图用途、`T_yueliang` 落点。

> 类型验证方法：读取每个 `.uasset`/`.umap` 内部的 FName 导出类名（World / SkeletalMesh / StaticMesh / Material / MaterialInstanceConstant / MaterialParameterCollection / Texture2D / Blueprint / AnimBlueprint / Skeleton / AnimSequence / PhysicsAsset），与文件名前缀（SM_/SKM_/SK_/M_/MI_/MPC_/T_/BP_/ABP_/Lvl_）交叉确认。
