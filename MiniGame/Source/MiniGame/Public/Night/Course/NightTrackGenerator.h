#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightTrackGenerator.generated.h"

#pragma region K2 moonyfli
USTRUCT(BlueprintType)
struct FNightGeneratedCourse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Night|Proc")
	TArray<FNightStoneSpec> Stones;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Proc")
	TArray<FNightBeatSpec> Beats;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Proc")
	TArray<FNightBridgeSpec> Bridges;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Proc")
	int32 ForkAfterStoneIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Proc")
	int32 BaseBeatCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Proc")
	int32 ResolvedSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Proc")
	ENightForkPair ForkPair = ENightForkPair::AB;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Proc")
	TArray<FNightKeySwapCue> KeySwaps;
};

/**
 * Seed-driven stone-chain + bridge layout generator (G3.5).
 * RNG matches HTML designer (UE FRandomStream LCG).
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightTrackGenerator : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Night|Proc")
	static ENightForkPair ForkEnvToPair(ENightForkEnv Env, ENightForkPair CustomPair);

	UFUNCTION(BlueprintCallable, Category = "Night|Proc")
	static FNightGeneratedCourse Generate(
		const FNightProcCourseParams& Params,
		const FVector& Origin,
		const FVector& Forward);

	UFUNCTION(BlueprintCallable, Category = "Night|Proc")
	static FNightGeneratedCourse GenerateBaseOnly(
		const FNightProcCourseParams& Params,
		const FVector& Origin,
		const FVector& Forward);

	UFUNCTION(BlueprintCallable, Category = "Night|Proc")
	static void AppendBranch(
		FNightGeneratedCourse& InOutCourse,
		ENightRouteId RouteId,
		const FNightProcCourseParams& Params,
		const FVector& Origin);

	UFUNCTION(BlueprintCallable, Category = "Night|Proc")
	static float QuantizeYaw01(float Deg);

protected:
	static int32 ResolveSeed(int32 InSeed);
	static ENightNodeKind PickAction(FRandomStream& Rng, float AttackBias, ENightNodeKind Last, int32& Streak, int32 MaxStreak);
	static void AppendStep(
		FNightGeneratedCourse& Out,
		FRandomStream& Rng,
		const FNightProcCourseParams& Params,
		FVector& InOutPos,
		float& InOutYawDeg,
		float& InOutArc,
		ENightNodeKind& InOutLastAction,
		int32& InOutStreak,
		bool bAsAttackPreferred);
	static void BuildKeySwapCues(FNightGeneratedCourse& Out, const FNightProcCourseParams& Params, int32 BranchBeats);
};
#pragma endregion K2 moonyfli
