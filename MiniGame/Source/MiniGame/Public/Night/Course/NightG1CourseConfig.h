#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightG1CourseConfig.generated.h"

class UNightRouteRulesAsset;
class UMaterialInterface;
class UStaticMesh;

#pragma region K2 moonyfli
/**
 * Fully tunable night course: base stone chain + fork + A/B/C branch layouts + key-swap table.
 * BeatCount = base beats. Stones_base = BeatCount + 1.
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightG1CourseConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Number of base-segment actions (beats). Stones = BeatCount + 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout")
	int32 BeatCount = 4;

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

	/** Feel window length written into FNightJudgeRequest (stub keeps long open). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout", meta = (ClampMin = "0.1"))
	float JudgeWindowSeconds = 3600.f;

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

	/** Override base beat actions; empty = Jump, Attack, Jump, Attack... */
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

	// --- G2/G3 fork ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|Fork")
	bool bEnableFork = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|Fork")
	float ForkTimeoutSeconds = 2.4f;

	/** Timeout picks left card when true; right when false. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|Fork")
	bool bForkTimeoutPickLeft = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|Fork")
	float BranchEnterBufferSeconds = 1.2f;

	/** Gap used for the auto-hop from last base stone onto first branch stone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|Fork")
	float BranchEntryGapCm = 280.f;

	/** When true, Host/StartNight can pull ForkPair from LevelRows for Bootstrap.LevelId. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|Fork")
	bool bApplyLevelTableToBootstrap = false;

	// --- Per-route branch layouts (preferred) ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|BranchA")
	FNightBranchLayoutSettings BranchA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|BranchB")
	FNightBranchLayoutSettings BranchB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3|BranchC")
	FNightBranchLayoutSettings BranchC;

	/** Legacy flat fields kept for existing DA; used when Branch*.BeatCount <= 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|BranchLegacy")
	int32 BranchABeatCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|BranchLegacy")
	TArray<ENightNodeKind> BranchAPatternOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|BranchLegacy")
	int32 BranchBBeatCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|BranchLegacy")
	TArray<ENightNodeKind> BranchBPatternOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G2|Rules")
	TObjectPtr<UNightRouteRulesAsset> RouteRules;

	// --- G3 key swap ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3|KeySwap")
	bool bEnableKeySwap = true;

	/** Global fallback warning / safety if a cue leaves them at 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3|KeySwap", meta = (ClampMin = "0.0"))
	float DefaultKeySwapWarningSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3|KeySwap", meta = (ClampMin = "0.0"))
	float DefaultKeySwapSafetySeconds = 0.6f;

	/**
	 * Honor GiftBuffs.bKeyCoin: skip the first key-swap cue when route is C.
	 * Full gift suite still lands in G5; this hook is required for G3 R1 contract.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3|KeySwap")
	bool bHonorKeyCoinSkipFirstSwap = true;

	/** Per-level fork + swap tables. Empty entries fall back to built-in defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3|Level")
	TArray<FNightLevelCourseSettings> LevelRows;

	/** Distance fade (fog): opacity falls off from the runner pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|DistanceFade")
	FNightDistanceFadeSettings DistanceFade;

	/** Translucent unlit material with Color / Opacity / FadeAlpha. Falls back to opaque Color-only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|DistanceFade")
	TObjectPtr<UMaterialInterface> DistanceFadeMaterial;

	// --- G3.5 procedural + art ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5|Proc")
	bool bUseProcGenerator = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5|Proc")
	FNightProcCourseParams ProcParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5|Art")
	TSoftObjectPtr<UStaticMesh> BridgeMeshA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5|Art")
	TSoftObjectPtr<UStaticMesh> BridgeMeshB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5|Art")
	TSoftObjectPtr<UStaticMesh> HeroMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM01;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM02;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM03;

	UNightG1CourseConfig();

	/** Builds base segment only (alias kept for G1 callers). */
	UFUNCTION(BlueprintCallable, Category = "Night|G1")
	void BuildCourse(TArray<FNightStoneSpec>& OutStones, TArray<FNightBeatSpec>& OutBeats) const;

	UFUNCTION(BlueprintCallable, Category = "Night|G2")
	void BuildBaseCourse(TArray<FNightStoneSpec>& OutStones, TArray<FNightBeatSpec>& OutBeats) const;

	/**
	 * Append-ready branch chain. Stone indices start at StoneIndexOffset.
	 * First stone distance = StartDistance (auto-hop target after fork).
	 */
	UFUNCTION(BlueprintCallable, Category = "Night|G2")
	void BuildBranchCourse(
		ENightRouteId RouteId,
		float StartDistance,
		int32 StoneIndexOffset,
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats) const;

	UFUNCTION(BlueprintCallable, Category = "Night|G3")
	FNightLevelCourseSettings GetLevelSettings(ENightLevelId LevelId) const;

	UFUNCTION(BlueprintCallable, Category = "Night|G3")
	static FNightLevelCourseSettings MakeDefaultLevelSettings(ENightLevelId LevelId);

	UFUNCTION(BlueprintCallable, Category = "Night|G3")
	void ApplyLevelDefaultsToBootstrap(FNightBootstrap& InOutBootstrap) const;

	UFUNCTION(BlueprintPure, Category = "Night|G2")
	FNightBranchLayoutSettings ResolveBranchLayout(ENightRouteId RouteId) const;

protected:
	void BuildSegment(
		int32 InBeatCount,
		const TArray<ENightNodeKind>& Pattern,
		float StartDistance,
		int32 StoneIndexOffset,
		bool bIncludeStartStone,
		bool bDefaultPreferAttack,
		float InJumpGapCm,
		float InKillGapCm,
		EIngredientId InDropId,
		int32 InDropCount,
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats) const;
};
#pragma endregion K2 moonyfli
