#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightG1CourseConfig.generated.h"

class ANightCoursePawn;
class UNightCourseAtomRouteData;
class UNightCourseRuleData;
class UNightRouteRulesAsset;

USTRUCT(BlueprintType)
struct MINIGAME_API FNightLevelCourseRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level")
	ENightLevelId LevelId = ENightLevelId::T0;

	/** Let this level row choose the pair; otherwise Bootstrap.ForkPair wins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level")
	bool bUseForkPair = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level")
	ENightForkPair ForkPair = ENightForkPair::AB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level")
	bool bUseKeySwapCues = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level")
	TArray<FNightKeySwapCue> KeySwapCues;
};

#pragma region K2 moonyfli
/**
 * Builds a 刃心 stone chain: stones + beats (Jump across gap / Attack into foe stone).
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightG1CourseConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops")
	EIngredientId DefaultDropId = EIngredientId::F01_LingGu;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops")
	int32 DefaultDropCount = 1;

	/** Repeated IDs act as weights for procedural enemy selection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat")
	TArray<EFoeId> FoeWeightPool;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat")
	EFoeId DefaultFoeId = EFoeId::M01;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5")
	bool bEnableFork = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork")
	float ForkTimeoutSeconds = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork")
	bool bForkTimeoutPickLeft = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork")
	float BranchEnterBufferSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork")
	float BranchEntryGapCm = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork")
	int32 ForkAfterBaseAtomIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork")
	ENightRouteId PreviewRoute = ENightRouteId::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap")
	bool bEnableKeySwap = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap")
	bool bKeySwapOnlyOnRouteC = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap")
	float DefaultKeySwapWarningSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap")
	float DefaultKeySwapSafetySeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap")
	TArray<FNightKeySwapCue> KeySwapCues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Gift", meta = (ClampMin = "0"))
	int32 TaotieFoeOverrideCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level")
	TArray<FNightLevelCourseRule> LevelRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Classes")
	TSubclassOf<ANightCoursePawn> HeroClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Route")
	TObjectPtr<UNightCourseAtomRouteData> AtomRoute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Route")
	TObjectPtr<UNightCourseRuleData> CourseRuleData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Route Rules")
	TObjectPtr<UNightRouteRulesAsset> RouteRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Debug")
	FNightG1DebugSettings Debug;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|G1|Editor")
	void MarkPackageDirtyForEditor();
};
#pragma endregion K2 moonyfli
