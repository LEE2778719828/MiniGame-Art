#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightTrackNodeActor.h"
#include "Night/Course/NightFoeActor.h"
#include "Night/Course/NightHazardActor.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Course/NightCoursePawn.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

#pragma region K2 moonyfli
UNightCourseDirector::UNightCourseDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UNightCourseDirector::BindFeelBridge(UObject* FeelObject)
{
	FeelBridgeObject = FeelObject;
}

void UNightCourseDirector::BindRunnerPawn(ANightCoursePawn* InPawn)
{
	RunnerPawn = InPawn;
}

INightFeelBridge* UNightCourseDirector::GetFeel() const
{
	return FeelBridgeObject ? Cast<INightFeelBridge>(FeelBridgeObject) : nullptr;
}

const UNightG1CourseConfig* UNightCourseDirector::GetConfig() const
{
	return Config;
}

FNightG1DebugSettings UNightCourseDirector::GetDebug() const
{
	if (bUseDebugOverride)
	{
		return DebugOverride;
	}
	if (Config)
	{
		return Config->Debug;
	}
	return FNightG1DebugSettings();
}

void UNightCourseDirector::SetPhase(ENightCoursePhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}
	const ENightCoursePhase Old = Phase;
	Phase = NewPhase;
	OnPhaseChanged.Broadcast(Old, NewPhase);
	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] Phase %d -> %d"), static_cast<int32>(Old), static_cast<int32>(NewPhase));
	}
}

FVector UNightCourseDirector::GetTrackLocation(float Distance) const
{
	const FVector Origin = Config ? Config->TrackOrigin : FVector::ZeroVector;
	const FVector Forward = Config ? Config->TrackForward.GetSafeNormal() : FVector::ForwardVector;
	return Origin + Forward * Distance;
}

void UNightCourseDirector::SyncPawnToProgress(bool bInstant)
{
	if (!RunnerPawn || !Config)
	{
		return;
	}
	const FVector Loc = GetTrackLocation(ProgressDistance);
	const FRotator Rot = Config->TrackForward.Rotation();
	if (bInstant)
	{
		RunnerPawn->SnapToTrack(Loc, Rot);
	}
	else
	{
		RunnerPawn->SetTrackTarget(Loc, Rot);
	}
}

void UNightCourseDirector::EnsureSpecs()
{
	NodeSpecs.Reset();
	if (Config)
	{
		Config->BuildNodeSpecs(NodeSpecs);
	}
}

void UNightCourseDirector::SpawnNodeActor(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World || !NodeSpecs.IsValidIndex(Index))
	{
		return;
	}

	const FNightTrackNodeSpec& Spec = NodeSpecs[Index];
	UClass* SpawnClass = nullptr;
	if (Spec.Kind == ENightNodeKind::Enemy)
	{
		SpawnClass = Config && Config->FoeClass ? Config->FoeClass.Get() : ANightFoeActor::StaticClass();
	}
	else
	{
		SpawnClass = Config && Config->HazardClass ? Config->HazardClass.Get() : ANightHazardActor::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator Facing = Config ? Config->TrackForward.Rotation() : FRotator::ZeroRotator;
	ANightTrackNodeActor* Node = World->SpawnActor<ANightTrackNodeActor>(
		SpawnClass,
		GetTrackLocation(Spec.TrackDistance),
		Facing,
		Params);
	if (!Node)
	{
		return;
	}

	Node->SetupNode(Index, Spec);
	// Fixed pose for the whole night — foes do not walk toward the player.
	Node->SetTrackPose(GetTrackLocation(Spec.TrackDistance), Facing);
	SpawnedNodes[Index] = Node;
}

void UNightCourseDirector::StartNight(const FNightBootstrap& Bootstrap)
{
	if (bRunning)
	{
		return;
	}

	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] StartNight failed: Config is null"));
		return;
	}

	ActiveBootstrap = Bootstrap;
	bRunning = true;
	ElapsedSeconds = 0.f;
	ProgressDistance = 0.f;
	ActiveNodeIndex = INDEX_NONE;
	bWindowOpen = false;
	bAdvancing = false;
	AdvanceTargetDistance = 0.f;
	CollectedIngredients.Reset();
	EnsureSpecs();
	NodeConsumed.Init(0, NodeSpecs.Num());
	SpawnedNodes.Init(nullptr, NodeSpecs.Num());

	for (int32 Index = 0; Index < NodeSpecs.Num(); ++Index)
	{
		SpawnNodeActor(Index);
	}

	SyncPawnToProgress(true);
	SetPhase(ENightCoursePhase::BaseSegment);
	SetComponentTickEnabled(true);

	// Arm first beat; world stays frozen until the player presses Jump/Attack.
	if (NodeSpecs.Num() > 0)
	{
		TryOpenWindow(0);
	}
	else
	{
		ExitBufferEndTime = ElapsedSeconds + Config->ExitBufferSeconds;
		SetPhase(ENightCoursePhase::ExitBuffer);
	}

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] StartNight Level=%d Nodes=%d Seed=%d (action-driven)"),
			static_cast<int32>(Bootstrap.LevelId), NodeSpecs.Num(), Bootstrap.Seed);
	}
}

void UNightCourseDirector::TryOpenWindow(int32 Index)
{
	if (!NodeSpecs.IsValidIndex(Index) || NodeConsumed[Index])
	{
		return;
	}

	ActiveNodeIndex = Index;
	bWindowOpen = true;

	FNightJudgeRequest Request;
	Request.NodeIndex = Index;
	Request.Kind = NodeSpecs[Index].Kind;
	Request.FoeId = NodeSpecs[Index].FoeId;
	Request.NodeActor = SpawnedNodes[Index];
	// No time-based close: window stays open until the player acts.
	Request.WindowOpenTime = ElapsedSeconds;
	Request.WindowCloseTime = ElapsedSeconds + 3600.f;

	if (ANightTrackNodeActor* Node = SpawnedNodes[Index])
	{
		Node->OnJudgeWindowOpened();
	}

	if (INightFeelBridge* Feel = GetFeel())
	{
		INightFeelBridge::Execute_NotifyJudgeRequest(FeelBridgeObject, Request);
	}

	if (GetDebug().bAutoSucceedWindows)
	{
		NotifyFeelResolved(Index, ENightJudgeOutcome::Success);
	}

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] OpenWindow idx=%d kind=%d (await input)"), Index, static_cast<int32>(Request.Kind));
	}
}

void UNightCourseDirector::BeginAdvanceTo(float TargetDistance)
{
	bAdvancing = true;
	bWindowOpen = false;
	AdvanceTargetDistance = TargetDistance;
	if (RunnerPawn && Config)
	{
		RunnerPawn->BeginTrackAdvance(GetTrackLocation(TargetDistance), Config->TrackForward.Rotation(), Config->AdvanceSpeed);
	}
}

void UNightCourseDirector::OnAdvanceArrived()
{
	bAdvancing = false;
	ProgressDistance = AdvanceTargetDistance;
	SyncPawnToProgress(true);
	OpenNextPendingWindowOrExit();
}

void UNightCourseDirector::OpenNextPendingWindowOrExit()
{
	for (int32 Index = 0; Index < NodeSpecs.Num(); ++Index)
	{
		if (!NodeConsumed[Index])
		{
			TryOpenWindow(Index);
			return;
		}
	}

	ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
	SetPhase(ENightCoursePhase::ExitBuffer);
}

void UNightCourseDirector::NotifyFeelResolved(int32 NodeIndex, ENightJudgeOutcome Outcome)
{
	if (!bRunning || !bWindowOpen || bAdvancing || ActiveNodeIndex != NodeIndex)
	{
		return;
	}
	if (Outcome == ENightJudgeOutcome::None)
	{
		return;
	}
	ResolveNode(NodeIndex, Outcome);
}

void UNightCourseDirector::ResolveNode(int32 Index, ENightJudgeOutcome Outcome)
{
	if (!NodeSpecs.IsValidIndex(Index) || NodeConsumed[Index] || !Config)
	{
		return;
	}

	NodeConsumed[Index] = 1;
	ActiveNodeIndex = INDEX_NONE;

	if (INightFeelBridge* Feel = GetFeel())
	{
		INightFeelBridge::Execute_ClearJudgeRequest(FeelBridgeObject, Index);
		if (Outcome == ENightJudgeOutcome::Success)
		{
			INightFeelBridge::Execute_PlaySuccessFeedback(FeelBridgeObject, NodeSpecs[Index].Kind);
		}
		else
		{
			const float Penalty = (Outcome == ENightJudgeOutcome::Miss) ? Config->MissPenalty : Config->WrongPenalty;
			INightFeelBridge::Execute_ApplySoulPenalty(FeelBridgeObject, Penalty, Outcome);
			INightFeelBridge::Execute_PlayFailFeedback(FeelBridgeObject, Outcome, NodeSpecs[Index].Kind);
		}
	}

	if (Outcome == ENightJudgeOutcome::Success && NodeSpecs[Index].Kind == ENightNodeKind::Enemy)
	{
		AddDrop(NodeSpecs[Index].DropId, NodeSpecs[Index].DropCount);
	}

	if (ANightTrackNodeActor* Node = SpawnedNodes[Index])
	{
		Node->OnResolved(Outcome);
		Node->OnDespawnRequested();
		SpawnedNodes[Index] = nullptr;
	}

	OnNodeResolved.Broadcast(Index, NodeSpecs[Index].Kind, Outcome);

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] Resolve idx=%d outcome=%d -> advance"), Index, static_cast<int32>(Outcome));
	}

	// Every resolve (success or wrong) moves the runner forward past that beat.
	const float Target = NodeSpecs[Index].TrackDistance + Config->AdvancePastNode;
	BeginAdvanceTo(FMath::Max(ProgressDistance, Target));
}

void UNightCourseDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bRunning || !Config)
	{
		return;
	}

	ElapsedSeconds += DeltaTime;
	OnDebugTick.Broadcast(ElapsedSeconds);

	if (GetDebug().bDrawDebug)
	{
		DrawDebugLine(GetWorld(), GetTrackLocation(0.f), GetTrackLocation(Config->FirstNodeDistance + Config->NodeSpacing * Config->NodeCount), FColor::Yellow, false, -1.f, 0, 2.f);
		if (ActiveNodeIndex != INDEX_NONE && NodeSpecs.IsValidIndex(ActiveNodeIndex))
		{
			DrawDebugSphere(GetWorld(), GetTrackLocation(NodeSpecs[ActiveNodeIndex].TrackDistance), 35.f, 12, FColor::Green, false, -1.f, 0, 1.5f);
		}
	}

	if (bAdvancing)
	{
		if (RunnerPawn && !RunnerPawn->IsTrackAdvancing())
		{
			OnAdvanceArrived();
		}
		return;
	}

	// Idle: no auto motion of nodes, no auto Miss, no camera chase beyond pawn snap.
	if (Phase == ENightCoursePhase::ExitBuffer)
	{
		if (ElapsedSeconds >= ExitBufferEndTime)
		{
			FNightResult Result;
			Result.bSuccess = true;
			Result.bFailedMidway = false;
			Result.RouteTaken = ENightRouteId::None;
			Result.Ingredients = CollectedIngredients;
			if (INightFeelBridge* Feel = GetFeel())
			{
				Result.SoulLeft = INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
			}
			else
			{
				Result.SoulLeft = Config->StartingSoul;
			}
			FinishNight(Result);
		}
	}
}

void UNightCourseDirector::AddDrop(EIngredientId Id, int32 Count)
{
	if (Id == EIngredientId::None || Count <= 0)
	{
		return;
	}

	for (FIngredientStack& Stack : CollectedIngredients)
	{
		if (Stack.Id == Id)
		{
			Stack.Count += Count;
			return;
		}
	}

	FIngredientStack NewStack;
	NewStack.Id = Id;
	NewStack.Count = Count;
	CollectedIngredients.Add(NewStack);
}

void UNightCourseDirector::FinishNight(const FNightResult& Result)
{
	SetComponentTickEnabled(false);
	bRunning = false;
	bAdvancing = false;
	bWindowOpen = false;
	SetPhase(ENightCoursePhase::Finished);
	OnFinished.Broadcast(Result);

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] Finished success=%d ingredients=%d soul=%.1f"),
			Result.bSuccess ? 1 : 0, Result.Ingredients.Num(), Result.SoulLeft);
	}
}

void UNightCourseDirector::DebugForceFinish(bool bSuccess)
{
	FNightResult Result;
	Result.bSuccess = bSuccess;
	Result.bFailedMidway = !bSuccess;
	Result.Ingredients = CollectedIngredients;
	if (INightFeelBridge* Feel = GetFeel())
	{
		Result.SoulLeft = INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
	}
	FinishNight(Result);
}

void UNightCourseDirector::DebugSkipToExit()
{
	for (int32 Index = 0; Index < NodeConsumed.Num(); ++Index)
	{
		NodeConsumed[Index] = 1;
		if (SpawnedNodes.IsValidIndex(Index) && SpawnedNodes[Index])
		{
			SpawnedNodes[Index]->Destroy();
			SpawnedNodes[Index] = nullptr;
		}
	}
	bWindowOpen = false;
	bAdvancing = false;
	ActiveNodeIndex = INDEX_NONE;
	ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
	SetPhase(ENightCoursePhase::ExitBuffer);
}
#pragma endregion K2 moonyfli
