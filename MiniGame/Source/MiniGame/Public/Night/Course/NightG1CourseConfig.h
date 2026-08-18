#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightG1CourseConfig.generated.h"

class UStaticMesh;
class UMaterialInterface;
class ANightBridgeSegmentActor;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5")
	FNightProcCourseParams ProcParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5")
	bool bUseProcGenerator = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5")
	bool bEnableFork = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> BridgeMeshA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> BridgeMeshB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Bridge Classes")
	TSubclassOf<ANightBridgeSegmentActor> BridgeClassA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Bridge Classes")
	TSubclassOf<ANightBridgeSegmentActor> BridgeClassB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> HeroMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM01;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM02;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM03;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM04;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM05;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Materials")
	TSoftObjectPtr<UMaterialInterface> DefaultArtMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Materials")
	TSoftObjectPtr<UMaterialInterface> BridgeMaterialA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Materials")
	TSoftObjectPtr<UMaterialInterface> BridgeMaterialB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Materials")
	TSoftObjectPtr<UMaterialInterface> HeroMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Materials")
	TSoftObjectPtr<UMaterialInterface> FoeMaterialM01;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Materials")
	TSoftObjectPtr<UMaterialInterface> FoeMaterialM02;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Materials")
	TSoftObjectPtr<UMaterialInterface> FoeMaterialM03;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Materials")
	TSoftObjectPtr<UMaterialInterface> FoeMaterialM04;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Materials")
	TSoftObjectPtr<UMaterialInterface> FoeMaterialM05;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Foe Transform")
	float FoeYawOffsetDeg = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Foe Transform")
	float FoeScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Foe Transform")
	float FoeHeightOffsetCm = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Foe Transform")
	FVector FoePivotOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Hero Transform")
	FVector HeroPivotOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Bridge Transform")
	FVector BridgePivotOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Bridge Transform")
	float BridgeGlobalScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Debug")
	FNightG1DebugSettings Debug;

	UFUNCTION(BlueprintCallable, Category = "Night|G1")
	void BuildCourse(TArray<FNightStoneSpec>& OutStones, TArray<FNightBeatSpec>& OutBeats) const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|G1|Editor")
	void MarkPackageDirtyForEditor();
};
#pragma endregion K2 moonyfli
