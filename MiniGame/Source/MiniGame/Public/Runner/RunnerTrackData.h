#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Runner/RunnerTypes.h"
#include "RunnerTrackData.generated.h"

#pragma region K2 moonyfli
/**
 * One demo level as a list of track events sorted by Distance.
 * Create assets under /Game/Runner/Data/
 */
UCLASS(BlueprintType)
class MINIGAME_API URunnerTrackData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	TArray<FRunnerTrackEvent> Events;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0"))
	float JumpForward = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0"))
	float JumpHeight = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0"))
	float AttackForward = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0"))
	float JudgeHalfWidth = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0"))
	float InputBufferTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "1"))
	int32 MaxHP = 3;

	/** Returns index of nearest event at or ahead of CurrentDistance, or INDEX_NONE. */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	int32 FindNextEventIndex(float CurrentDistance) const;
};
#pragma endregion K2 moonyfli
