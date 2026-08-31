#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightRouteRules.generated.h"

class UMaterialInterface;

#pragma region K2 moonyfli
/** Per-route modifiers for branch segment (A/B/C fully tunable). */
USTRUCT(BlueprintType)
struct FNightRouteRuleRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "路线ID", ToolTip = "本行对应的分支路线：A、B 或 C；每个路线应有且只有一行。"))
	ENightRouteId RouteId = ENightRouteId::A;

	/** How many stones ahead of the runner stay visible (fog = lower). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "兼容可见区块数量", ToolTip = "旧版按区块控制可见范围的兼容字段；运行时显示/隐藏改用下面的分支Actor可见距离。"))
	int32 VisibleBlockCount = 8;

	/** Longitudinal distance from the runner within which branch Actors stay visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "分支Actor可见距离", ToolTip = "以主角当前位置为中心，沿课程轨道前后保持分支 Actor 可见并参与流式生成的距离，单位 cm。", ClampMin = "1.0"))
	float VisibleDistanceCm = 3000.f;

	/** Optional post-process material instance selected for this branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route|Art", meta = (DisplayName = "分支后处理材质实例", ToolTip = "玩家选择本路线后替换跑酷默认后处理材质。建议绑定材质实例；为空时继续使用 DA_Course 的默认跑酷后处理材质。"))
	TObjectPtr<UMaterialInterface> PostProcessMaterial = nullptr;

	/** Optional floor / Plane material instance selected for this branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route|Art", meta = (DisplayName = "分支地板材质实例", ToolTip = "玩家选择本路线后替换关卡 Plane 的地板材质。建议绑定材质实例；为空时继续使用 DA_Course 的默认地板材质。"))
	TObjectPtr<UMaterialInterface> FloorMaterial = nullptr;

	/** Multiplies Wrong/Miss soul penalty while on branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "灵魂惩罚倍率", ToolTip = "分支期间错误/漏判惩罚的倍率，1 表示不变。"))
	float SoulPenaltyScale = 1.f;

	/** Etch-fire / reverse-fire DoT: soul drained per second on branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "持续扣魂速度", ToolTip = "分支期间每秒额外扣除的灵魂值；0 表示关闭。"))
	float DotSoulPerSecond = 0.f;

	/**
	 * Reverse-fire (C): DoT also ticks while advancing.
	 * Etch-fire (B): typically false (idle windows only).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "前进时持续扣魂", ToolTip = "开启后持续扣魂在角色前进时也生效；通常 C 路开启。"))
	bool bReverseFire = false;

	/** Ingredients granted once when entering this branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "进入分支奖励数量", ToolTip = "进入该分支时一次性发放的食材数量。"))
	int32 EnterDropCount = 0;

	/** None = use course DefaultDropId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "进入分支奖励ID", ToolTip = "进入分支时发放的食材 ID；None 使用 DA_Course.DefaultDropId。"))
	EIngredientId EnterDropId = EIngredientId::None;

	/** Extra fraction applied to branch-segment drops at finish (e.g. 0.2 = +20%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "结算奖励加成", ToolTip = "结算时对分支期间收集的数量额外增加的比例；0.2 表示增加 20%。"))
	float CarryOutBonus = 0.f;

	/** Keep every Nth attack drop (1 = every attack). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "掉落节奏每N次", ToolTip = "每 N 次攻击成功发放一次掉落；1 表示每次攻击都发放。"))
	int32 DropRhythmEveryN = 1;

	/**
	 * Drop cycle for successful Attack beats on this route.
	 * Empty = use stone DropId. Non-empty cycles by branch attack index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "兼容掉落循环", ToolTip = "仅在敌人没有 FoeDropMap 映射时使用的兼容 ID 循环；正式 M01-M05 映射不会被它替换。"))
	TArray<EIngredientId> DropCycle;

	/** Multiplier on stone DropCount when granting branch attack drops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (DisplayName = "分支掉落数量倍率", ToolTip = "分支攻击掉落数量倍率；1 表示不变。", ClampMin = "1"))
	int32 BranchDropCountMul = 1;
};

UCLASS(BlueprintType)
class MINIGAME_API UNightRouteRulesAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Route", meta = (DisplayName = "路线规则行", ToolTip = "为 A、B、C 各配置一行；这里调整分支节奏、倍率、扣魂和结算奖励。"))
	TArray<FNightRouteRuleRow> Rows;

	UFUNCTION(BlueprintCallable, Category = "Night|Route")
	FNightRouteRuleRow GetRule(ENightRouteId RouteId) const;

	/** Returns false when the route is not explicitly authored in this asset. */
	UFUNCTION(BlueprintCallable, Category = "Night|Route")
	bool TryGetRule(ENightRouteId RouteId, FNightRouteRuleRow& OutRule) const;

	UFUNCTION(BlueprintCallable, Category = "Night|Route")
	bool ValidateRules(FString& OutError) const;

	/** Helper defaults for authoring/tests; Director still requires an asset. */
	UFUNCTION(BlueprintCallable, Category = "Night|Route")
	static FNightRouteRuleRow MakeDefaultRule(ENightRouteId RouteId);
};
#pragma endregion K2 moonyfli
