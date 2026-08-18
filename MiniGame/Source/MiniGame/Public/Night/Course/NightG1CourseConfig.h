#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightG1CourseConfig.generated.h"

#pragma region K2 moonyfli
/**
 * Builds a 刃心 stone chain: stones + beats (Jump across gap / Attack into foe stone).
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightG1CourseConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Number of actions (beats). Stones = BeatCount + 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	int32 BeatCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	float FirstStoneDistance = 0.f;

	/** Gap size for Jump beats (center-to-center). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	float JumpGapCm = 420.f;

	/** Gap size for Attack beats (center-to-center, close pads). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	float KillGapCm = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	float AdvanceSpeed = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	float ExitBufferSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Space")
	FVector TrackOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Space")
	FVector TrackForward = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat")
	float WrongPenalty = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat")
	float MissPenalty = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat")
	float StartingSoul = 100.f;

	/** Override beat actions; empty = Jump, Attack, Jump, Attack... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Pattern")
	TArray<ENightNodeKind> PatternOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops")
	EIngredientId DefaultDropId = EIngredientId::F01_LingGu;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops")
	int32 DefaultDropCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Classes")
	TSubclassOf<AActor> StoneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Debug")
	FNightG1DebugSettings Debug;

	UFUNCTION(BlueprintCallable, Category = "Night|G1")
	void BuildCourse(TArray<FNightStoneSpec>& OutStones, TArray<FNightBeatSpec>& OutBeats) const;
};
#pragma endregion K2 moonyfli
