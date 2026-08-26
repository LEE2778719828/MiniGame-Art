# 白天 → 夜晚 地图跳转实现说明

> 目标：让 `L_S_DayWhitebox`（白天）在营业额达标结算后，自动跳转到 `L_Night_G1_ForkTest`（夜晚）。
> 适用阶段：prototype。当前项目已具备完整的昼夜状态机（`USChefGameInstance`）与「夜 → 白天」跳转，本说明只补「白天 → 夜」这一半。
> 状态：代码已施工（2026-08-24），改动落在 `SStandaloneSandbox.h/.cpp`；本机无 UBT 未编译，请在 UE 编辑器/构建脚本编译后按第 5 节验收。

---

## 1. 背景与结论

关卡跳转在 UE 里只有两种本质做法：

| 做法 | 说明 | 本项目是否采用 |
|------|------|----------------|
| **Open Level（硬切换）** | 整张地图卸载、再加载另一张 | 是（夜→白天已用 `UGameplayStatics::OpenLevel`） |
| **Level Streaming（流送）** | 子关卡流式加载，无缝衔接 | 否（prototype 暂不需要） |

**关键发现**：项目里「夜 → 白天」的地图切换已经写好，缺的是反方向「白天 → 夜」的 `OpenLevel`。

- 白天结束的链路：`USChefGameInstance::CloseShopNow → EnterDaySettlement → AdvanceToNextStage`，它只把状态机 `Phase` 推进到 `PrepareNight`，**从不用 `OpenLevel` 去夜地图**。
- 因此白天跑完不会进夜图。补上「白天成功结算 → 打开夜图」这一刀即可。
- 夜图 `L_Night_G1_ForkTest` 中的 `ANightCourseHost`（`bAutoStart=true`）加载时会检测 `Phase==PrepareNight` 并自动起夜；`USChefGameInstance` 状态机跨地图保留——这条后续链路架构已铺好。

---

## 2. 当前代码核对（改动前的现状）

| 文件 | 位置 | 作用 |
|------|------|------|
| `Source/MiniGame/Private/Night/Course/NightCourseHost.cpp` | `TravelToDay()` 第 1084–1153 行 | **夜 → 白天** 跳转已实现，用 `UGameplayStatics::OpenLevel(this, FName(*DayLevelPackage), true, Options)` |
| `Source/MiniGame/Public/Night/Course/NightCourseGameMode.h` | 第 29–38 行 | 夜→白天配置：`bTravelToDayOnSuccess` / `SuccessDayLevel` / `SuccessDayGameMode` |
| `Source/MiniGame/SStandaloneSandbox.cpp` | `AdvanceToNextStage()` 第 1700–1738 行 | **白天结算终点**：非结束分支把 `Phase` 设成 `PrepareNight`，但无地图跳转 |
| `Source/MiniGame/SStandaloneSandbox.cpp` | `EnterDaySettlement()` 第 1673–1698 行 | 白天达标结算，调用 `AdvanceToNextStage()` |
| `Source/MiniGame/SStandaloneSandbox.cpp` | `CloseShopNow()` 第 1861–1884 行 | 营业额达标走 `EnterDaySettlement`；不达标走 `FailDay`（回档重开当日，不跳夜） |
| `Source/MiniGame/SStandaloneSandbox.h` | `OnSandboxStateChanged` 第 690 / 939 行 | `BlueprintAssignable` 委托，蓝图方案可用作触发点 |

夜图包名：`/Game/Night/Course/Maps/L_Night_G1_ForkTest`
夜图 GameMode：`/Game/Night/Course/Blueprints/BP_NightCourseGameMode.BP_NightCourseGameMode_C`

---

## 3. 推荐方案：镜像 `TravelToDay`（最贴合现有架构）

### 3.1 在 `ASChefGameMode` 加配置与方法

文件：`Source/MiniGame/SStandaloneSandbox.h`（在 `ASChefGameMode` 类定义内，与 `ANightCourseGameMode` 的 `bTravelToDayOnSuccess` 字段对称）

```cpp
// Day|Flow 分类下，与夜→白天的字段一一对应
UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Day|Flow")
bool bTravelToNightOnDayEnd = true;

UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Day|Flow")
TSoftObjectPtr<UWorld> SuccessNightLevel;

UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Day|Flow")
TSoftClassPtr<AGameModeBase> SuccessNightGameMode;

UFUNCTION(BlueprintCallable, Category = "Day|Flow")
void TravelToNight();
```

构造里给默认值（`ASChefGameMode::ASChefGameMode()`，与现有 `PlayerControllerClass = ASDayPlayerController::StaticClass();` 并列）：

```cpp
SuccessNightLevel = TSoftObjectPtr<UWorld>(
    FSoftObjectPath(TEXT("/Game/Night/Course/Maps/L_Night_G1_ForkTest.L_Night_G1_ForkTest")));
SuccessNightGameMode = TSoftClassPtr<AGameModeBase>(
    FSoftObjectPath(TEXT("/Game/Night/Course/Blueprints/BP_NightCourseGameMode.BP_NightCourseGameMode_C")));
```

> 也支持在 `Config/DefaultGame.ini` 的 `[/Script/MiniGame.ChefGameMode]` 节覆盖，不改构造函数同样生效（字段已标 `Config`）。

### 3.2 实现 `TravelToNight()`

文件：`Source/MiniGame/SStandaloneSandbox.cpp`（照抄 `TravelToDay` 反过来，建议紧挨 `ASChefGameMode::BeginPlay` 之后）

```cpp
#include "Kismet/GameplayStatics.h"   // 确保 cpp 顶部已包含

void ASChefGameMode::TravelToNight()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (!bTravelToNightOnDayEnd)
    {
        UE_LOG(LogTemp, Display, TEXT("[DayFlow] Night travel disabled on Day GameMode '%s'."), *GetNameSafe(this));
        return;
    }

    FSoftObjectPath NightLevelPath = SuccessNightLevel.ToSoftObjectPath();
    FSoftObjectPath NightGameModePath = SuccessNightGameMode.ToSoftObjectPath();

    const FString NightLevelPackage = NightLevelPath.GetLongPackageName();
    if (NightLevelPackage.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[DayFlow] No Night level configured. Set SuccessNightLevel on the Day GameMode."));
        return;
    }

    FString Options;
    if (!NightGameModePath.IsNull())
    {
        Options = FString::Printf(TEXT("?game=%s"), *NightGameModePath.ToString());
    }

    UE_LOG(LogTemp, Display, TEXT("[DayFlow] Day settled; opening Night level='%s' gameMode='%s'."),
        *NightLevelPackage,
        NightGameModePath.IsNull() ? TEXT("<map default>") : *NightGameModePath.ToString());

    UGameplayStatics::OpenLevel(this, FName(*NightLevelPackage), true, Options);
}
```

### 3.3 在白天结算终点触发

文件：`Source/MiniGame/SStandaloneSandbox.cpp` → `USChefGameInstance::AdvanceToNextStage()` 的 **非结束分支**末尾（约第 1737 行 `AutoSaveChefProfile(TEXT("进入下一关夜晚"));` 之后）

```cpp
    Phase = ESGamePhase::PrepareNight;
    // ……（原有 feedback / NotifyStateChanged / AutoSaveChefProfile 保持不变）……

    // 新增：白天成功结算 → 跳夜图
    if (AGameModeBase* GM = UGameplayStatics::GetGameMode(this))
    {
        if (ASChefGameMode* ChefGM = Cast<ASChefGameMode>(GM))
        {
            ChefGM->TravelToNight();
        }
    }
```

> 只放在 **非 `bEnding` 分支**。`AdvanceToNextStage()` 顶部的 `bEnding` 提前 return 那条路（最终关）不要加，否则最终关也会跳夜。

---

## 4. 备选方案：纯蓝图快速验证（不改 C++）

适合只想先在编辑器里把跳转跑通：

1. 在 `BP_DayGameMode`（或 `BP_FakeNightGateway`）中放节点 **Open Level (by Object Reference)**。
2. `Level` 引脚选 `L_Night_G1_ForkTest`；要显式指定夜 GameMode，在 `Options` 填：
   `?game=/Game/Night/Course/Blueprints/BP_NightCourseGameMode.BP_NightCourseGameMode_C`
3. 触发点（两种）：
   - **绑定委托**：`USChefGameInstance` 上的 `OnSandboxStateChanged`（BlueprintAssignable），事件内读 `Phase`，当它变成 `PrepareNight` 且上一阶段是白天时调用上面的 OpenLevel。
   - **更干净**：在 C++ 给 `USChefGameInstance` 加 `UFUNCTION(BlueprintImplementableEvent) void OnDaySettled();`，在 `AdvanceToNextStage` 非结束分支末尾调用，蓝图里只实现「Open Level 夜图」这一行。

---

## 5. 验收标准

- PIE 运行 `L_S_DayWhitebox`，白天营业额达标 → 自动闭店结算 → **无缝跳到 `L_Night_G1_ForkTest`**，夜晚自动开始跑酷。
- 白天不达标 → `FailDay` 回档重开当日，**不跳夜**。
- 夜晚成功 → 走已有 `TravelToDay` 跳回白天，形成「白天 ⇄ 夜晚」闭环。
- 输出日志出现：
  `[DayFlow] Day settled; opening Night level=/Game/Night/Course/Maps/L_Night_G1_ForkTest gameMode=...`

---

## 6. 风险与注意点

- `OpenLevel` 会卸载当前世界，但 `USChefGameInstance` 跨地图保留，`Phase` 与关卡进度/库存跨地图保存。
- 确保 `L_S_DayWhitebox` 的 World Settings → GameMode Override 指向 `BP_DayGameMode`/`ASDayWhiteboxGameMode`（白天本就在跑，一般已设）；否则 `TravelToNight()` 取不到 Day GameMode 不会触发。
- 若同一进程里既跑真实白天又跑 `ASFakeNightGateway` 的 smoke test，二者共用同一条 `AdvanceToNextStage` 链路，跳转对两条路径都生效。
- 修改 `Config` 字段后会写入 `DefaultGame.ini`；若用构造函数默认值，无需改 ini。
