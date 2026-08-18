#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightCourseDirector.generated.h"

class UNightG1CourseConfig;
class ANightCourseStoneActor;
class ANightBridgeSegmentActor;
class ANightCoursePawn;
class INightFeelBridge;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNightCourseFinished, const FNightResult&, Result);
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

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void StartNight(const FNightBootstrap& Bootstrap);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void BindFeelBridge(UObject* FeelObject);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void BindRunnerPawn(ANightCoursePawn* InPawn);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void NotifyFeelResolved(int32 NodeIndex, ENightJudgeOutcome Outcome);

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Debug")
	void DebugForceFinish(bool bSuccess);

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Debug")
	void DebugSkipToExit();

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	bool IsRunning() const { return bRunning; }

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

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TArray<TObjectPtr<ANightCourseStoneActor>> SpawnedStones;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TArray<TObjectPtr<ANightBridgeSegmentActor>> SpawnedBridges;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TArray<FIngredientStack> CollectedIngredients;

	UPROPERTY()
	TObjectPtr<UObject> FeelBridgeObject;

	UPROPERTY()
	TObjectPtr<ANightCoursePawn> RunnerPawn;

	TArray<uint8> BeatConsumed;
	float ExitBufferEndTime = 0.f;
	float AdvanceTargetDistance = 0.f;
	bool bWindowOpen = false;
	bool bAdvancing = false;

	const UNightG1CourseConfig* GetConfig() const;
	FNightG1DebugSettings GetDebug() const;
	void SetPhase(ENightCoursePhase NewPhase);
	void FinishNight(const FNightResult& Result);
	void EnsureCourse();
	void SpawnStoneActor(int32 Index);
	void SpawnBridgeActor(int32 Index);
	void TryOpenBeat(int32 BeatIndex);
	void ResolveBeat(int32 BeatIndex, ENightJudgeOutcome Outcome);
	void BeginAdvanceToStone(int32 StoneIndex);
	void OnAdvanceArrived();
	void OpenNextBeatOrExit();
	void SyncPawnToProgress(bool bInstant);
	void AddDrop(EIngredientId Id, int32 Count);
	FVector GetTrackLocation(float Distance) const;
	FVector GetStoneWorldLocation(int32 StoneIndex) const;
	INightFeelBridge* GetFeel() const;
};
#pragma endregion K2 moonyfli
