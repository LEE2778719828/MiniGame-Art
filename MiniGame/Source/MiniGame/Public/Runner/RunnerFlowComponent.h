#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Runner/RunnerTypes.h"
#include "RunnerFlowComponent.generated.h"

class URunnerTrackData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRunnerJudge, ERunnerJudgeResult, Result, ERunnerEventType, EventType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerDistanceChanged, float, NewDistance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRunnerStatsChanged, int32, HP, int32, Combo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRunnerWon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRunnerDied);

#pragma region K2 moonyfli
/**
 * Track progress + dual-button judgment. Attach to RunnerCharacter.
 * No free locomotion: distance advances only via successful Jump/Attack.
 */
UCLASS(ClassGroup = (Runner), meta = (BlueprintSpawnableComponent))
class MINIGAME_API URunnerFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URunnerFlowComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<URunnerTrackData> TrackData;

	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	float CurrentDistance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	int32 CurrentHP = 3;

	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	int32 Combo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	bool bIsBusy = false;

	UPROPERTY(BlueprintAssignable, Category = "Runner")
	FOnRunnerJudge OnJudge;

	UPROPERTY(BlueprintAssignable, Category = "Runner")
	FOnRunnerDistanceChanged OnDistanceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Runner")
	FOnRunnerStatsChanged OnStatsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Runner")
	FOnRunnerWon OnWon;

	UPROPERTY(BlueprintAssignable, Category = "Runner")
	FOnRunnerDied OnDied;

	UFUNCTION(BlueprintCallable, Category = "Runner")
	void ResetRun();

	UFUNCTION(BlueprintCallable, Category = "Runner")
	void SetTrackData(URunnerTrackData* InTrackData);

	/** Called by Character after Jump/Attack input. Returns false if busy or dead. */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	bool TryResolveInput(ERunnerInputAction Action, float& OutMoveForward, float& OutJumpHeight, ERunnerEventType& OutEventType);

	UFUNCTION(BlueprintCallable, Category = "Runner")
	void NotifyMoveFinished();

	UFUNCTION(BlueprintPure, Category = "Runner")
	bool IsAlive() const { return CurrentHP > 0; }

	UFUNCTION(BlueprintPure, Category = "Runner")
	bool TryGetNextEvent(FRunnerTrackEvent& OutEvent) const;

protected:
	void ApplySuccess(const FRunnerTrackEvent& Event, float MoveForward);
	void ApplyFail(ERunnerJudgeResult Result, ERunnerEventType EventType);
	void BroadcastStats() const;
};
#pragma endregion K2 moonyfli
