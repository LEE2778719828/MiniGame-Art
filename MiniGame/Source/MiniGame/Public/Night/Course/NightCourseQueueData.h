#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightCourseQueueData.generated.h"

/** One authored NightCourse selection in the Day -> Night queue. */
USTRUCT(BlueprintType)
struct FNightCourseQueueEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Course Queue")
	ENightRouteId MainRoute = ENightRouteId::A;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Course Queue", meta = (ClampMin = "1"))
	int32 MainRouteAtomCount = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Course Queue")
	bool bEnableFork = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Course Queue", meta = (EditCondition = "bEnableFork"))
	ENightForkPair ForkPair = ENightForkPair::AB;

	/** Applied symmetrically to either player-selected route in the configured fork pair. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Course Queue", meta = (EditCondition = "bEnableFork", ClampMin = "1"))
	int32 ForkRouteAtomCount = 30;

	/** Deterministic random seed for this exact Day -> Night queue entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Course Queue", meta = (DisplayName = "关卡随机种子", ToolTip = "同一条目和种子始终生成同一套 Atom、敌人与道路装饰组合。失败重试不会切换种子。", ClampMin = "1"))
	int32 Seed = 1001;
};

/** Ordered Day -> Night course plans. Entries loop by default so a completed list remains playable. */
UCLASS(BlueprintType, meta = (DisplayName = "Night Course Queue"))
class MINIGAME_API UNightCourseQueueData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Course Queue")
	TArray<FNightCourseQueueEntry> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Course Queue")
	bool bLoop = true;
};
