#include "Runner/RunnerFlowComponent.h"
#include "Runner/RunnerTrackData.h"

#pragma region K2 moonyfli
URunnerFlowComponent::URunnerFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URunnerFlowComponent::ResetRun()
{
	CurrentDistance = 0.f;
	Combo = 0;
	bIsBusy = false;
	CurrentHP = TrackData ? TrackData->MaxHP : 3;
	BroadcastStats();
	OnDistanceChanged.Broadcast(CurrentDistance);
}

void URunnerFlowComponent::SetTrackData(URunnerTrackData* InTrackData)
{
	TrackData = InTrackData;
	ResetRun();
}

bool URunnerFlowComponent::TryGetNextEvent(FRunnerTrackEvent& OutEvent) const
{
	if (!TrackData)
	{
		return false;
	}

	const int32 Index = TrackData->FindNextEventIndex(CurrentDistance);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutEvent = TrackData->Events[Index];
	return true;
}

bool URunnerFlowComponent::TryResolveInput(ERunnerInputAction Action, float& OutMoveForward, float& OutJumpHeight, ERunnerEventType& OutEventType)
{
	OutMoveForward = 0.f;
	OutJumpHeight = 0.f;
	OutEventType = ERunnerEventType::Gap;

	if (!IsAlive() || bIsBusy || !TrackData)
	{
		return false;
	}

	FRunnerTrackEvent NextEvent;
	if (!TryGetNextEvent(NextEvent))
	{
		return false;
	}

	OutEventType = NextEvent.Type;
	const float HalfWidth = TrackData->JudgeHalfWidth;
	const bool bInWindow = FMath::Abs(NextEvent.Distance - CurrentDistance) <= HalfWidth
		|| (NextEvent.Distance >= CurrentDistance && (NextEvent.Distance - CurrentDistance) <= HalfWidth * 2.f);

	if (NextEvent.Type == ERunnerEventType::Goal)
	{
		if (CurrentDistance + KINDA_SMALL_NUMBER >= NextEvent.Distance)
		{
			OnWon.Broadcast();
		}
		return false;
	}

	const bool bExpectJump = (NextEvent.Type == ERunnerEventType::Gap);
	const bool bExpectAttack = (NextEvent.Type == ERunnerEventType::Enemy);
	const bool bCorrect =
		(bExpectJump && Action == ERunnerInputAction::Jump) ||
		(bExpectAttack && Action == ERunnerInputAction::Attack);

	if (!bInWindow)
	{
		// Too early: treat as wrong rhythm for demo simplicity.
		ApplyFail(ERunnerJudgeResult::WrongButton, NextEvent.Type);
		return false;
	}

	if (!bCorrect)
	{
		ApplyFail(ERunnerJudgeResult::WrongButton, NextEvent.Type);
		return false;
	}

	if (Action == ERunnerInputAction::Jump)
	{
		OutMoveForward = TrackData->JumpForward;
		OutJumpHeight = TrackData->JumpHeight;
	}
	else
	{
		OutMoveForward = TrackData->AttackForward;
		OutJumpHeight = 0.f;
	}

	bIsBusy = true;
	ApplySuccess(NextEvent, OutMoveForward);
	return true;
}

void URunnerFlowComponent::NotifyMoveFinished()
{
	bIsBusy = false;

	FRunnerTrackEvent NextEvent;
	if (TryGetNextEvent(NextEvent) && NextEvent.Type == ERunnerEventType::Goal)
	{
		if (CurrentDistance + KINDA_SMALL_NUMBER >= NextEvent.Distance)
		{
			OnWon.Broadcast();
		}
	}
}

void URunnerFlowComponent::ApplySuccess(const FRunnerTrackEvent& Event, float MoveForward)
{
	(void)MoveForward;
	++Combo;
	// Snap past the resolved event so FindNextEventIndex advances; visual move uses MoveForward separately.
	CurrentDistance = Event.Distance + 0.01f;
	OnJudge.Broadcast(ERunnerJudgeResult::Success, Event.Type);
	OnDistanceChanged.Broadcast(CurrentDistance);
	BroadcastStats();
}

void URunnerFlowComponent::ApplyFail(ERunnerJudgeResult Result, ERunnerEventType EventType)
{
	Combo = 0;
	CurrentHP = FMath::Max(0, CurrentHP - 1);
	OnJudge.Broadcast(Result, EventType);
	BroadcastStats();

	if (CurrentHP <= 0)
	{
		OnDied.Broadcast();
	}
}

void URunnerFlowComponent::BroadcastStats() const
{
	OnStatsChanged.Broadcast(CurrentHP, Combo);
}
#pragma endregion K2 moonyfli
