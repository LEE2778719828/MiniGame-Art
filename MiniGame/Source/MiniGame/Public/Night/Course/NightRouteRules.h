#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightRouteRules.generated.h"

#pragma region K2 moonyfli
/** Per-route modifiers for branch segment (A/B/C fully tunable). */
USTRUCT(BlueprintType)
struct FNightRouteRuleRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	ENightRouteId RouteId = ENightRouteId::A;

	/** How many stones ahead of the runner stay visible (fog = lower). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	int32 VisibleBlockCount = 8;

	/** Multiplies Wrong/Miss soul penalty while on branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	float SoulPenaltyScale = 1.f;

	/** Etch-fire / reverse-fire DoT: soul drained per second on branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	float DotSoulPerSecond = 0.f;

	/**
	 * Reverse-fire (C): DoT also ticks while advancing.
	 * Etch-fire (B): typically false (idle windows only).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	bool bReverseFire = false;

	/** Ingredients granted once when entering this branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	int32 EnterDropCount = 0;

	/** None = use course DefaultDropId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	EIngredientId EnterDropId = EIngredientId::None;

	/** Extra fraction applied to branch-segment drops at finish (e.g. 0.2 = +20%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	float CarryOutBonus = 0.f;

	/** Keep every Nth attack drop (1 = every attack). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	int32 DropRhythmEveryN = 1;

	/**
	 * Drop cycle for successful Attack beats on this route.
	 * Empty = use stone DropId. Non-empty cycles by branch attack index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route")
	TArray<EIngredientId> DropCycle;

	/** Multiplier on stone DropCount when granting branch attack drops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Route", meta = (ClampMin = "1"))
	int32 BranchDropCountMul = 1;
};

UCLASS(BlueprintType)
class MINIGAME_API UNightRouteRulesAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Route")
	TArray<FNightRouteRuleRow> Rows;

	UFUNCTION(BlueprintCallable, Category = "Night|Route")
	FNightRouteRuleRow GetRule(ENightRouteId RouteId) const;

	/** Built-in A/B/C rows used when no asset is assigned. */
	UFUNCTION(BlueprintCallable, Category = "Night|Route")
	static FNightRouteRuleRow MakeDefaultRule(ENightRouteId RouteId);
};
#pragma endregion K2 moonyfli
