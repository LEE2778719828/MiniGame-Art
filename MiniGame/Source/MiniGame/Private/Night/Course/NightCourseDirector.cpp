#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightTrackGenerator.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Course/NightCoursePawn.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

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

FVector UNightCourseDirector::GetStoneWorldLocation(int32 StoneIndex) const
{
	if (StoneSpecs.IsValidIndex(StoneIndex) && StoneSpecs[StoneIndex].bUseWorldPose)
	{
		return StoneSpecs[StoneIndex].WorldLocation;
	}
	return StoneSpecs.IsValidIndex(StoneIndex)
		? GetTrackLocation(StoneSpecs[StoneIndex].TrackDistance)
		: FVector::ZeroVector;
}

void UNightCourseDirector::SyncPawnToProgress(bool bInstant)
{
	if (!RunnerPawn || !Config)
	{
		return;
	}
	const int32 TargetStone = FMath::Clamp(CurrentStoneIndex, 0, StoneSpecs.Num() - 1);
	const FVector Loc = StoneSpecs.IsValidIndex(TargetStone)
		? GetStoneWorldLocation(TargetStone)
		: GetTrackLocation(ProgressDistance);
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

void UNightCourseDirector::EnsureCourse()
{
	StoneSpecs.Reset();
	BeatSpecs.Reset();
	BridgeSpecs.Reset();
	if (Config && Config->ProcParams.bEnableProcGenerator)
	{
		const FNightGeneratedCourse Generated = UNightTrackGenerator::GenerateBaseOnly(
			Config->ProcParams,
			Config->TrackOrigin,
			Config->TrackForward);
		StoneSpecs = Generated.Stones;
		BeatSpecs = Generated.Beats;
		BridgeSpecs = Generated.Bridges;
	}
	else if (Config)
	{
		Config->BuildCourse(StoneSpecs, BeatSpecs);
	}
}

void UNightCourseDirector::SpawnStoneActor(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World || !StoneSpecs.IsValidIndex(Index))
	{
		return;
	}

	UClass* SpawnClass = (Config && Config->StoneClass)
		? Config->StoneClass.Get()
		: ANightCourseStoneActor::StaticClass();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator Facing = Config ? Config->TrackForward.Rotation() : FRotator::ZeroRotator;
	ANightCourseStoneActor* Stone = World->SpawnActor<ANightCourseStoneActor>(
		SpawnClass,
		GetStoneWorldLocation(Index),
		Facing,
		Params);
	if (!Stone)
	{
		return;
	}

	Stone->SetupStone(Index, StoneSpecs[Index]);
	if (StoneSpecs[Index].bHasFoe)
	{
		UStaticMesh* FoeMesh = nullptr;
		UMaterialInterface* FoeMaterial = nullptr;
		if (Config)
		{
			switch (StoneSpecs[Index].FoeId)
			{
			case EFoeId::M01:
				FoeMesh = Config->FoeMeshM01.LoadSynchronous();
				FoeMaterial = Config->FoeMaterialM01.LoadSynchronous();
				break;
			case EFoeId::M02:
				FoeMesh = Config->FoeMeshM02.LoadSynchronous();
				FoeMaterial = Config->FoeMaterialM02.LoadSynchronous();
				break;
			case EFoeId::M03:
				FoeMesh = Config->FoeMeshM03.LoadSynchronous();
				FoeMaterial = Config->FoeMaterialM03.LoadSynchronous();
				break;
			case EFoeId::M04:
				FoeMesh = Config->FoeMeshM04.LoadSynchronous();
				FoeMaterial = Config->FoeMaterialM04.LoadSynchronous();
				break;
			case EFoeId::M05:
				FoeMesh = Config->FoeMeshM05.LoadSynchronous();
				FoeMaterial = Config->FoeMaterialM05.LoadSynchronous();
				break;
			default: break;
			}
			if (!FoeMaterial)
			{
				FoeMaterial = Config->DefaultArtMaterial.LoadSynchronous();
			}
		}
		if (!FoeMesh)
		{
			const TCHAR* FoePath = TEXT("/Game/Night/Course/Art/Foe/fish.fish");
			switch (StoneSpecs[Index].FoeId)
			{
			case EFoeId::M02: FoePath = TEXT("/Game/Night/Course/Art/Foe/box1.box1"); break;
			case EFoeId::M03: FoePath = TEXT("/Game/Night/Course/Art/Foe/box2.box2"); break;
			case EFoeId::M04: FoePath = TEXT("/Game/Night/Course/Art/Foe/box3.box3"); break;
			case EFoeId::M05: FoePath = TEXT("/Game/Night/Course/Art/Foe/cantingguai.cantingguai"); break;
			default: break;
			}
			FoeMesh = LoadObject<UStaticMesh>(nullptr, FoePath);
		}
		Stone->ApplyFoeMesh(FoeMesh, FoeMaterial);
		if (Config)
		{
			Stone->SetFoeArtTransform(
				Config->FoeYawOffsetDeg,
				Config->FoeScale,
				Config->FoeHeightOffsetCm,
				Config->FoePivotOffsetCm);
		}
	}
	Stone->SetTrackPose(GetStoneWorldLocation(Index), Facing);
	SpawnedStones[Index] = Stone;
}

void UNightCourseDirector::SpawnBridgeActor(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World || !BridgeSpecs.IsValidIndex(Index))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANightBridgeSegmentActor* Bridge = World->SpawnActor<ANightBridgeSegmentActor>(
		ANightBridgeSegmentActor::StaticClass(),
		BridgeSpecs[Index].WorldLocation,
		FRotator(0.f, BridgeSpecs[Index].YawDeg, 0.f),
		Params);
	if (!Bridge)
	{
		return;
	}

	UStaticMesh* Mesh = nullptr;
	if (Config)
	{
		const TSoftObjectPtr<UStaticMesh>& SoftMesh =
			BridgeSpecs[Index].MeshVariant == 0 ? Config->BridgeMeshA : Config->BridgeMeshB;
		Mesh = SoftMesh.LoadSynchronous();
	}
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(
			nullptr,
			BridgeSpecs[Index].MeshVariant == 0
				? TEXT("/Game/Night/Course/Art/Bridge/muban1.muban1")
				: TEXT("/Game/Night/Course/Art/Bridge/muban2.muban2"));
	}
	UMaterialInterface* BridgeMaterial = nullptr;
	if (Config)
	{
		BridgeMaterial = BridgeSpecs[Index].MeshVariant == 0
			? Config->BridgeMaterialA.LoadSynchronous()
			: Config->BridgeMaterialB.LoadSynchronous();
		if (!BridgeMaterial)
		{
			BridgeMaterial = Config->DefaultArtMaterial.LoadSynchronous();
		}
	}
	Bridge->SetupBridge(
		BridgeSpecs[Index],
		Mesh,
		BridgeMaterial,
		Config ? Config->BridgePivotOffsetCm : FVector::ZeroVector);
	SpawnedBridges[Index] = Bridge;
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
	CurrentStoneIndex = 0;
	ActiveBeatIndex = INDEX_NONE;
	bWindowOpen = false;
	bAdvancing = false;
	CollectedIngredients.Reset();
	EnsureCourse();
	BeatConsumed.Init(0, BeatSpecs.Num());
	SpawnedStones.Init(nullptr, StoneSpecs.Num());
	SpawnedBridges.Init(nullptr, BridgeSpecs.Num());

	for (int32 Index = 0; Index < BridgeSpecs.Num(); ++Index)
	{
		SpawnBridgeActor(Index);
	}
	for (int32 Index = 0; Index < StoneSpecs.Num(); ++Index)
	{
		SpawnStoneActor(Index);
	}

	ProgressDistance = StoneSpecs.IsValidIndex(0) ? StoneSpecs[0].TrackDistance : 0.f;
	SyncPawnToProgress(true);
	SetPhase(ENightCoursePhase::BaseSegment);
	SetComponentTickEnabled(true);

	if (BeatSpecs.Num() > 0)
	{
		TryOpenBeat(0);
	}
	else
	{
		ExitBufferEndTime = ElapsedSeconds + Config->ExitBufferSeconds;
		SetPhase(ENightCoursePhase::ExitBuffer);
	}

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] StartNight stones=%d beats=%d (stone-chain)"),
			StoneSpecs.Num(), BeatSpecs.Num());
	}
}

void UNightCourseDirector::TryOpenBeat(int32 BeatIndex)
{
	if (!BeatSpecs.IsValidIndex(BeatIndex) || BeatConsumed[BeatIndex])
	{
		return;
	}

	ActiveBeatIndex = BeatIndex;
	bWindowOpen = true;
	CurrentStoneIndex = BeatSpecs[BeatIndex].FromStoneIndex;

	if (SpawnedStones.IsValidIndex(CurrentStoneIndex) && SpawnedStones[CurrentStoneIndex])
	{
		SpawnedStones[CurrentStoneIndex]->SetHighlight(true);
	}

	const FNightBeatSpec& Beat = BeatSpecs[BeatIndex];
	if (Beat.Action == ENightNodeKind::Enemy && StoneSpecs.IsValidIndex(Beat.ToStoneIndex))
	{
		FNightStoneSpec& TargetStone = StoneSpecs[Beat.ToStoneIndex];
		if (!TargetStone.bHasFoe)
		{
			TargetStone.bHasFoe = true;
			TargetStone.FoeId = EFoeId::M01;
		}
		if (SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
		{
			SpawnedStones[Beat.ToStoneIndex]->ShowFoe();
		}
	}
	FNightJudgeRequest Request;
	Request.NodeIndex = BeatIndex;
	Request.Kind = Beat.Action;
	Request.WindowOpenTime = ElapsedSeconds;
	Request.WindowCloseTime = ElapsedSeconds + 3600.f;
	if (StoneSpecs.IsValidIndex(Beat.ToStoneIndex) && StoneSpecs[Beat.ToStoneIndex].bHasFoe)
	{
		Request.FoeId = StoneSpecs[Beat.ToStoneIndex].FoeId;
	}
	Request.NodeActor = SpawnedStones.IsValidIndex(Beat.ToStoneIndex) ? SpawnedStones[Beat.ToStoneIndex] : nullptr;

	if (INightFeelBridge* Feel = GetFeel())
	{
		INightFeelBridge::Execute_NotifyJudgeRequest(FeelBridgeObject, Request);
	}

	if (GetDebug().bAutoSucceedWindows)
	{
		NotifyFeelResolved(BeatIndex, ENightJudgeOutcome::Success);
	}

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] OpenBeat idx=%d action=%d from=%d to=%d"),
			BeatIndex, static_cast<int32>(Beat.Action), Beat.FromStoneIndex, Beat.ToStoneIndex);
	}
}

void UNightCourseDirector::NotifyFeelResolved(int32 NodeIndex, ENightJudgeOutcome Outcome)
{
	if (!bRunning || !bWindowOpen || bAdvancing || ActiveBeatIndex != NodeIndex)
	{
		return;
	}
	if (Outcome == ENightJudgeOutcome::None)
	{
		return;
	}
	ResolveBeat(NodeIndex, Outcome);
}

void UNightCourseDirector::ResolveBeat(int32 BeatIndex, ENightJudgeOutcome Outcome)
{
	if (!BeatSpecs.IsValidIndex(BeatIndex) || BeatConsumed[BeatIndex] || !Config)
	{
		return;
	}

	BeatConsumed[BeatIndex] = 1;
	bWindowOpen = false;
	ActiveBeatIndex = INDEX_NONE;

	const FNightBeatSpec& Beat = BeatSpecs[BeatIndex];
	if (SpawnedStones.IsValidIndex(Beat.FromStoneIndex) && SpawnedStones[Beat.FromStoneIndex])
	{
		SpawnedStones[Beat.FromStoneIndex]->SetHighlight(false);
	}

	if (INightFeelBridge* Feel = GetFeel())
	{
		INightFeelBridge::Execute_ClearJudgeRequest(FeelBridgeObject, BeatIndex);
		if (Outcome == ENightJudgeOutcome::Success)
		{
			INightFeelBridge::Execute_PlaySuccessFeedback(FeelBridgeObject, Beat.Action);
		}
		else
		{
			const float Penalty = (Outcome == ENightJudgeOutcome::Miss) ? Config->MissPenalty : Config->WrongPenalty;
			INightFeelBridge::Execute_ApplySoulPenalty(FeelBridgeObject, Penalty, Outcome);
			INightFeelBridge::Execute_PlayFailFeedback(FeelBridgeObject, Outcome, Beat.Action);
		}
	}

	const bool bAttackBeat = (Beat.Action == ENightNodeKind::Enemy);
	if (Outcome == ENightJudgeOutcome::Success && bAttackBeat && StoneSpecs.IsValidIndex(Beat.ToStoneIndex))
	{
		AddDrop(StoneSpecs[Beat.ToStoneIndex].DropId, StoneSpecs[Beat.ToStoneIndex].DropCount);
		if (SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
		{
			SpawnedStones[Beat.ToStoneIndex]->ClearFoe(true);
			SpawnedStones[Beat.ToStoneIndex]->PlayDropBurst(
				StoneSpecs[Beat.ToStoneIndex].DropId,
				StoneSpecs[Beat.ToStoneIndex].DropCount);
		}
	}
	else if (Outcome != ENightJudgeOutcome::Success && bAttackBeat
		&& SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
	{
		// Wrong/Miss still advances onto the stone but foe can stay or clear — clear to keep chain readable.
		SpawnedStones[Beat.ToStoneIndex]->ClearFoe(false);
	}

	OnNodeResolved.Broadcast(BeatIndex, Beat.Action, Outcome);

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] ResolveBeat idx=%d outcome=%d -> stone %d"),
			BeatIndex, static_cast<int32>(Outcome), Beat.ToStoneIndex);
	}

	BeginAdvanceToStone(Beat.ToStoneIndex);
}

void UNightCourseDirector::BeginAdvanceToStone(int32 StoneIndex)
{
	if (!StoneSpecs.IsValidIndex(StoneIndex) || !Config)
	{
		OpenNextBeatOrExit();
		return;
	}

	bAdvancing = true;
	CurrentStoneIndex = StoneIndex;
	AdvanceTargetDistance = StoneSpecs[StoneIndex].TrackDistance;
	if (RunnerPawn)
	{
		RunnerPawn->BeginTrackAdvance(
			GetStoneWorldLocation(StoneIndex),
			Config->TrackForward.Rotation(),
			Config->AdvanceSpeed);
	}
}

void UNightCourseDirector::OnAdvanceArrived()
{
	bAdvancing = false;
	ProgressDistance = AdvanceTargetDistance;
	SyncPawnToProgress(true);
	OpenNextBeatOrExit();
}

void UNightCourseDirector::OpenNextBeatOrExit()
{
	for (int32 Index = 0; Index < BeatSpecs.Num(); ++Index)
	{
		if (!BeatConsumed[Index])
		{
			TryOpenBeat(Index);
			return;
		}
	}

	ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
	SetPhase(ENightCoursePhase::ExitBuffer);
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
		for (const FNightStoneSpec& Stone : StoneSpecs)
		{
			DrawDebugSphere(GetWorld(), Stone.bUseWorldPose ? Stone.WorldLocation : GetTrackLocation(Stone.TrackDistance), 25.f, 8,
				Stone.bHasFoe ? FColor::Red : FColor::Cyan, false, -1.f, 0, 1.f);
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

	if (Phase == ENightCoursePhase::ExitBuffer && ElapsedSeconds >= ExitBufferEndTime)
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
	for (int32 Index = 0; Index < BeatConsumed.Num(); ++Index)
	{
		BeatConsumed[Index] = 1;
	}
	bWindowOpen = false;
	bAdvancing = false;
	ActiveBeatIndex = INDEX_NONE;
	ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
	SetPhase(ENightCoursePhase::ExitBuffer);
}
#pragma endregion K2 moonyfli
