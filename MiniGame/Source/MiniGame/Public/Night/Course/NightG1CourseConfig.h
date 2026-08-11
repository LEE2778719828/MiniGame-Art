#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightG1CourseConfig.generated.h"

#pragma region K2 moonyfli
/**
 * Tunable G1 straight-course config.
 * Action-driven: nodes stay fixed; player advances only on Jump/Attack.
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightG1CourseConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	int32 NodeCount = 8;

	/** Distance of first node along track (+X). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	float FirstNodeDistance = 500.f;

	/** Spacing between nodes along track. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	float NodeSpacing = 450.f;

	/** After resolving a node, stop this far past it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	float AdvancePastNode = 120.f;

	/** Lerp speed while dashing forward after an input (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	float AdvanceSpeed = 1400.f;

	/** Brief pause after last node before Result (still no auto motion of foes). */
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

	/** Alternate Enemy/Hazard starting with Enemy when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Pattern")
	TArray<ENightNodeKind> PatternOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops")
	EIngredientId DefaultDropId = EIngredientId::F01_LingGu;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops")
	int32 DefaultDropCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Classes")
	TSubclassOf<AActor> FoeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Classes")
	TSubclassOf<AActor> HazardClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Debug")
	FNightG1DebugSettings Debug;

	UFUNCTION(BlueprintCallable, Category = "Night|G1")
	void BuildNodeSpecs(TArray<FNightTrackNodeSpec>& OutSpecs) const;
};
#pragma endregion K2 moonyfli
