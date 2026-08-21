#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightCourseInterface.h"
#include "Night/Course/NightCourseRuleData.h"
#include "Night/Course/NightRouteRules.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightCourseDirector.generated.h"

class UNightG1CourseConfig;
class ANightCourseStoneActor;
class ANightBridgeSegmentActor;
class ANightCoursePawn;
class AActor;
class UBoxComponent;
class INightFeelBridge;
class UNightForkController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNightCoursePhaseChanged, ENightCoursePhase, OldPhase, ENightCoursePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNightCourseNodeEvent, int32, NodeIndex, ENightNodeKind, Kind, ENightJudgeOutcome, Outcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNightCourseDebugTick, float, ElapsedSeconds);

#pragma region K2 moonyfli
/**
 * 刃心 stone-chain director: stand on stone, Jump/Attack to next stone.
 * Idle = frozen; action advances runner to ToStone.
 */
UCLASS(ClassGroup = (Night), meta = (BlueprintSpawnableComponent))
class MINIGAME_API UNightCourseDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UNightCourseDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	TObjectPtr<UNightG1CourseConfig> Config;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	FNightG1DebugSettings DebugOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bUseDebugOverride = false;

	UPROPERTY(BlueprintAssignable, Category = "Night|Course")
	FOnNightCourseFinished OnFinished;

	UPROPERTY(BlueprintAssignable, Category = "Night|Course|Debug")
	FOnNightCoursePhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Night|Course|Debug")
	FOnNightCourseNodeEvent OnNodeResolved;

	UPROPERTY(BlueprintAssignable, Category = "Night|Course|Debug")
	FOnNightCourseDebugTick OnDebugTick;

	UPROPERTY(BlueprintAssignable, Category = "Night|Course|Debug")
	FOnNightCourseDebugMessage OnDebugMessage;

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void StartNight(const FNightBootstrap& Bootstrap);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	bool TryStartNight(const FNightBootstrap& Bootstrap, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void BindFeelBridge(UObject* FeelObject);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void BindRunnerPawn(ANightCoursePawn* InPawn);

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Layout")
	void SetLayoutBoundsComponent(UBoxComponent* InBoundsComponent, bool bInEnforceBounds);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void NotifyFeelResolved(int32 NodeIndex, ENightJudgeOutcome Outcome);

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Debug")
	void DebugForceFinish(bool bSuccess);

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Debug")
	void DebugSkipToExit();

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Fork")
	void ChooseForkLeft();

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Fork")
	void ChooseForkRight();

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Fork")
	void SkipFork();

	UFUNCTION(BlueprintCallable, Category = "Night|Course|KeySwap")
	void ForceKeySwap();

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Debug")
	void ResetCourse();

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Debug")
	bool ValidateConfiguration(FString& OutError) const;

	bool BuildCourseForPreview(
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats,
		TArray<FNightBridgeSpec>& OutBridges) const;

	bool BuildCourseForPreview(
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats,
		TArray<FNightBridgeSpec>& OutBridges,
		TArray<FNightAtomVisualBinding>& OutVisualBindings) const;

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	bool IsRunning() const { return bRunning; }

	UFUNCTION(BlueprintPure, Category = "Night|Course|Result")
	bool HasNightResult() const { return bHasResult; }

	UFUNCTION(BlueprintPure, Category = "Night|Course|Result")
	FNightResult GetNightResult() const { return LastResult; }

	UFUNCTION(BlueprintPure, Category = "Night|Course|Debug")
	FString GetLastFailureReason() const { return LastFailureReason; }

	UFUNCTION(BlueprintPure, Category = "Night|Course|Debug")
	bool DidEnterRuntimeCourse() const { return bDidEnterRuntimeCourse; }

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	bool IsAwaitingInput() const { return bRunning && bWindowOpen && !bAdvancing; }

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	ENightCoursePhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	float GetElapsedSeconds() const { return ElapsedSeconds; }

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	float GetProgressDistance() const { return ProgressDistance; }

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	int32 GetActiveNodeIndex() const { return ActiveBeatIndex; }

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	int32 GetCurrentStoneIndex() const { return CurrentStoneIndex; }

	UFUNCTION(BlueprintPure, Category = "Night|Course|Fork")
	bool IsForkChoiceActive() const
	{
		return bRunning && Phase == ENightCoursePhase::ForkChoice;
	}

	UFUNCTION(BlueprintPure, Category = "Night|Course|Fork")
	float GetForkSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Night|Course|Fork")
	ENightRouteId GetForkLeftRoute() const;

	UFUNCTION(BlueprintPure, Category = "Night|Course|Fork")
	ENightRouteId GetForkRightRoute() const;

	UFUNCTION(BlueprintPure, Category = "Night|Course|Fork")
	ENightRouteId GetCurrentRoute() const { return CurrentRoute; }

	UFUNCTION(BlueprintPure, Category = "Night|Course|Fork")
	int32 GetBranchBeatCount() const { return BranchBeatCount; }

	UFUNCTION(BlueprintPure, Category = "Night|Course|Route")
	int32 GetVisibleBlockCount() const
	{
		return bHasActiveRouteRule
			? FMath::Max(1, ActiveRouteRule.VisibleBlockCount)
			: 0;
	}

	UFUNCTION(BlueprintPure, Category = "Night|Course|Gift")
	FString GetForkHintText() const;

	UFUNCTION(BlueprintPure, Category = "Night|Course|KeySwap")
	bool IsKeySwapWarningActive() const
	{
		return Phase == ENightCoursePhase::KeySwapWarning
			|| Phase == ENightCoursePhase::KeySwapSafetyHold;
	}

	UFUNCTION(BlueprintPure, Category = "Night|Course|KeySwap")
	float GetKeySwapSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Night|Course|KeySwap")
	bool IsCourseFailed() const { return Phase == ENightCoursePhase::Failed; }

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	const TArray<FIngredientStack>& GetCollectedIngredients() const { return CollectedIngredients; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	FNightBootstrap ActiveBootstrap;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	bool bRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	ENightCoursePhase Phase = ENightCoursePhase::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Debug")
	bool bDidEnterRuntimeCourse = false;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Result")
	bool bHasResult = false;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Result")
	FNightResult LastResult;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Debug")
	FString LastFailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	float ElapsedSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	float ProgressDistance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	int32 CurrentStoneIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	int32 ActiveBeatIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TArray<FNightStoneSpec> StoneSpecs;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TArray<FNightBeatSpec> BeatSpecs;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TArray<FNightBridgeSpec> BridgeSpecs;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Visual")
	TArray<FNightAtomVisualBinding> VisualBindings;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TArray<TObjectPtr<ANightCourseStoneActor>> SpawnedStones;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TArray<TObjectPtr<ANightBridgeSegmentActor>> SpawnedBridges;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Visual")
	TArray<TObjectPtr<AActor>> SpawnedVisualActors;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TArray<FIngredientStack> CollectedIngredients;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Drops")
	TArray<FIngredientStack> BranchCollectedIngredients;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Fork")
	ENightRouteId CurrentRoute = ENightRouteId::None;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Fork")
	ENightForkPair ActiveForkPair = ENightForkPair::AB;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Fork")
	int32 BranchBeatCount = 0;

	UPROPERTY()
	TObjectPtr<UObject> FeelBridgeObject;

	UPROPERTY()
	TObjectPtr<ANightCoursePawn> RunnerPawn;

	TArray<uint8> BeatConsumed;
	float ExitBufferEndTime = 0.f;
	float AdvanceTargetDistance = 0.f;
	bool bWindowOpen = false;
	bool bAdvancing = false;

	UPROPERTY()
	TObjectPtr<UBoxComponent> LayoutBoundsComponent;

	UPROPERTY()
	bool bEnforceLayoutBounds = false;

	UPROPERTY()
	TObjectPtr<UNightForkController> ForkController;

	FNightRouteRuleRow ActiveRouteRule;
	bool bHasActiveRouteRule = false;
	bool bForkPending = false;
	bool bBranchSelected = false;
	bool bSpareLampConsumed = false;
	bool bBranchTransitionConsumed = false;
	bool bBranchHasExplicitTransitionBeat = true;
	int32 BaseBeatCount = 0;
	int32 BranchTransitionBeatIndex = INDEX_NONE;
	int32 RuntimeSeed = 0;
	bool bHasRuntimeSeed = false;
	bool bBuildingRuntimeCourse = false;
	int32 NextKeySwapCueIndex = 0;
	float BranchEnterBufferEndTime = 0.f;
	float KeySwapEndTime = 0.f;
	TArray<FNightKeySwapCue> AuthoredKeySwapCues;
	TArray<FNightKeySwapCue> ActiveKeySwapCues;

	const UNightG1CourseConfig* GetConfig() const;
	FNightG1DebugSettings GetDebug() const;
	void SetPhase(ENightCoursePhase NewPhase);
	void EmitDebugMessage(const FString& Message, bool bIsError);
	void FinishNight(const FNightResult& Result);
	bool EnsureCourse(FString& OutError);
	void ClearSpawnedCourseActors();
	void SpawnCourseActors();
	bool RebuildCourseForSelectedRoute(FString& OutError);
	void BeginForkChoice();
	UFUNCTION()
	void HandleForkResolved(ENightRouteId RouteTaken, bool bTimedOut);
	void BeginKeySwapWarning();
	void ApplyKeySwapCue(const FNightKeySwapCue& Cue);
	bool HasPendingKeySwap() const;
	void UpdateRouteEffects(float DeltaTime);
	void UpdateRouteVisibility();
	void HandleFailedInput(int32 BeatIndex, ENightJudgeOutcome Outcome);
	void BeginFailure(const FString& Reason);
	bool HasBranchQueueForRoute(ENightRouteId RouteId) const;
	bool BuildAtomRouteCourse(
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats,
		TArray<FNightBridgeSpec>& OutBridges) const;
	bool BuildAtomRouteCourse(
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats,
		TArray<FNightBridgeSpec>& OutBridges,
		TArray<FNightAtomVisualBinding>& OutVisualBindings) const;
	void SpawnStoneActor(int32 Index);
	void SpawnBridgeActor(int32 Index);
	void SpawnVisualBinding(int32 BindingIndex);
	void SetStoneVisualVisibility(int32 StoneIndex, bool bVisible);
	bool IsAtomTransformInsideLayoutBounds(
		const ANightCourseAtomActor* AtomDefaults,
		const FTransform& AtomWorld) const;
	void TryOpenBeat(int32 BeatIndex);
	void ResolveBeat(int32 BeatIndex, ENightJudgeOutcome Outcome);
	void BeginAdvanceToStone(int32 StoneIndex);
	void OnAdvanceArrived();
	void OpenNextBeatOrExit();
	void SyncPawnToProgress(bool bInstant);
	void AddDrop(EIngredientId Id, int32 Count);
	void AddDropToArray(TArray<FIngredientStack>& Target, EIngredientId Id, int32 Count) const;
	FVector GetTrackLocation(float Distance) const;
	FVector GetStoneWorldLocation(int32 StoneIndex) const;
	INightFeelBridge* GetFeel() const;
};
#pragma endregion K2 moonyfli
