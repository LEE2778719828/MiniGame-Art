#pragma once

#include "CoreMinimal.h"
#include "RunnerTypes.generated.h"

#pragma region K2 moonyfli
UENUM(BlueprintType)
enum class ERunnerEventType : uint8
{
	Gap UMETA(DisplayName = "Gap"),
	Enemy UMETA(DisplayName = "Enemy"),
	Goal UMETA(DisplayName = "Goal")
};

UENUM(BlueprintType)
enum class ERunnerJudgeResult : uint8
{
	None UMETA(DisplayName = "None"),
	Success UMETA(DisplayName = "Success"),
	WrongButton UMETA(DisplayName = "WrongButton"),
	Miss UMETA(DisplayName = "Miss")
};

UENUM(BlueprintType)
enum class ERunnerInputAction : uint8
{
	Jump UMETA(DisplayName = "Jump"),
	Attack UMETA(DisplayName = "Attack")
};

USTRUCT(BlueprintType)
struct FRunnerTrackEvent
{
	GENERATED_BODY()

	/** Distance along the track in cm where this event is centered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner")
	float Distance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner")
	ERunnerEventType Type = ERunnerEventType::Gap;

	/** Optional tag for whitebox / future enemy class lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner")
	FName EventId = NAME_None;
};
#pragma endregion K2 moonyfli
