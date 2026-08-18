#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightForkController.h"
#include "Night/Course/NightRouteRules.h"
#include "Night/Course/NightTrackGenerator.h"
#include "Night/Course/NightProcParamsAsset.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
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
	if (RunnerPawn)
	{
		RunnerPawn->CourseDirector = this;
	}
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

bool UNightCourseDirector::IsForkChoiceActive() const
{
	return Phase == ENightCoursePhase::ForkChoice && ForkController && ForkController->IsForkActive();
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
	if (!StoneSpecs.IsValidIndex(StoneIndex))
	{
		return GetTrackLocation(0.f);
	}
	const FNightStoneSpec& Spec = StoneSpecs[StoneIndex];
	if (Spec.bUseWorldPose)
	{
		return Spec.WorldLocation;
	}
	return GetTrackLocation(Spec.TrackDistance);
}

FRotator UNightCourseDirector::GetStoneWorldRotation(int32 StoneIndex) const
{
	if (StoneSpecs.IsValidIndex(StoneIndex) && StoneSpecs[StoneIndex].bUseWorldPose)
	{
		return FRotator(0.f, StoneSpecs[StoneIndex].YawDeg, 0.f);
	}
	return Config ? Config->TrackForward.Rotation() : FRotator::ZeroRotator;
}

void UNightCourseDirector::SyncPawnToProgress(bool bInstant)
{
	if (!RunnerPawn || !Config)
	{
		return;
	}
	FVector Loc = GetTrackLocation(ProgressDistance);
	FRotator Rot = Config->TrackForward.Rotation();
	if (StoneSpecs.IsValidIndex(CurrentStoneIndex) && StoneSpecs[CurrentStoneIndex].bUseWorldPose)
	{
		Loc = StoneSpecs[CurrentStoneIndex].WorldLocation;
		Rot = GetStoneWorldRotation(CurrentStoneIndex);
	}
	else
	{
		// Snap along arc: find nearest stone by TrackDistance.
		for (int32 i = 0; i < StoneSpecs.Num(); ++i)
		{
			if (FMath::IsNearlyEqual(StoneSpecs[i].TrackDistance, ProgressDistance, 1.f))
			{
				Loc = GetStoneWorldLocation(i);
				Rot = GetStoneWorldRotation(i);
				break;
			}
		}
	}
	if (bInstant)
	{
		RunnerPawn->SnapToTrack(Loc, Rot);
	}
	else
	{
		RunnerPawn->SetTrackTarget(Loc, Rot);
	}
}

FNightRouteRuleRow UNightCourseDirector::ResolveRouteRule(ENightRouteId RouteId) const
{
	if (Config && Config->RouteRules)
	{
		return Config->RouteRules->GetRule(RouteId);
	}
	return UNightRouteRulesAsset::MakeDefaultRule(RouteId);
}

void UNightCourseDirector::EnsureBaseCourse()
{
	ClearSpawnedActors();
	StoneSpecs.Reset();
	BeatSpecs.Reset();
	BridgeSpecs.Reset();
	bUsingProcCourse = false;
	ProcForkAfterStoneIndex = INDEX_NONE;
	ResolvedProcSeed = ActiveBootstrap.Seed;

	ActiveProcParams = FNightProcCourseParams();
	if (ProcParamsAsset)
	{
		ActiveProcParams = ProcParamsAsset->Params;
	}
	else if (Config)
	{
		ActiveProcParams = Config->ProcParams;
	}

	const bool bProc = ActiveProcParams.bEnableProcGenerator
		&& (ProcParamsAsset != nullptr || (Config && Config->bUseProcGenerator));

	if (bProc)
	{
		bUsingProcCourse = true;
		if (ActiveProcParams.Seed == 0 && ActiveBootstrap.Seed != 0)
		{
			ActiveProcParams.Seed = ActiveBootstrap.Seed;
		}

		if (ProcParamsAsset && ProcParamsAsset->bPreferBakedCourse && ProcParamsAsset->BakedStones.Num() > 0)
		{
			StoneSpecs = ProcParamsAsset->BakedStones;
			BeatSpecs = ProcParamsAsset->BakedBeats;
			BridgeSpecs = ProcParamsAsset->BakedBridges;
			ResolvedProcSeed = ActiveProcParams.Seed != 0 ? ActiveProcParams.Seed : ActiveBootstrap.Seed;
			BaseBeatCount = BeatSpecs.Num();
			ActiveBootstrap.ForkPair = UNightTrackGenerator::ForkEnvToPair(
				ActiveProcParams.ForkEnv, ActiveProcParams.ForkPair);
			ActiveBootstrap.Seed = ResolvedProcSeed;
		}
		else
		{
			const FVector Origin = Config ? Config->TrackOrigin : FVector::ZeroVector;
			const FVector Forward = Config ? Config->TrackForward : FVector::ForwardVector;
			FNightGeneratedCourse Gen = UNightTrackGenerator::GenerateBaseOnly(ActiveProcParams, Origin, Forward);
			StoneSpecs = Gen.Stones;
			BeatSpecs = Gen.Beats;
			BridgeSpecs = Gen.Bridges;
			BaseBeatCount = Gen.BaseBeatCount;
			ResolvedProcSeed = Gen.ResolvedSeed;
			ProcForkAfterStoneIndex = Gen.ForkAfterStoneIndex;
			ActiveBootstrap.ForkPair = Gen.ForkPair;
			ActiveBootstrap.Seed = Gen.ResolvedSeed;
			ActiveProcParams.Seed = Gen.ResolvedSeed;
		}

		if (Config)
		{
			Config->ForkTimeoutSeconds = ActiveProcParams.ForkTimeoutSeconds;
			Config->WrongPenalty = ActiveProcParams.WrongPenalty;
			Config->StartingSoul = ActiveProcParams.StartingSoul;
			Config->JumpGapCm = ActiveProcParams.JumpGapCm;
			Config->KillGapCm = ActiveProcParams.KillGapCm;
			Config->BranchEntryGapCm = ActiveProcParams.BranchEntryGapCm;
		}
		return;
	}

	if (Config)
	{
		Config->BuildBaseCourse(StoneSpecs, BeatSpecs);
	}
	BaseBeatCount = BeatSpecs.Num();
}

void UNightCourseDirector::ClearSpawnedActors()
{
	for (ANightCourseStoneActor* Stone : SpawnedStones)
	{
		if (Stone)
		{
			Stone->Destroy();
		}
	}
	SpawnedStones.Reset();
	for (ANightBridgeSegmentActor* Bridge : SpawnedBridges)
	{
		if (Bridge)
		{
			Bridge->Destroy();
		}
	}
	SpawnedBridges.Reset();
}

UStaticMesh* UNightCourseDirector::ResolveBridgeMesh(int32 MeshVariant) const
{
	TSoftObjectPtr<UStaticMesh> Soft;
	if (ProcParamsAsset)
	{
		Soft = (MeshVariant == 0) ? ProcParamsAsset->BridgeMeshA : ProcParamsAsset->BridgeMeshB;
	}
	else if (Config)
	{
		Soft = (MeshVariant == 0) ? Config->BridgeMeshA : Config->BridgeMeshB;
	}
	return Soft.LoadSynchronous();
}

UStaticMesh* UNightCourseDirector::ResolveFoeMesh(EFoeId FoeId) const
{
	TSoftObjectPtr<UStaticMesh> Soft;
	if (ProcParamsAsset)
	{
		switch (FoeId)
		{
		case EFoeId::M02: Soft = ProcParamsAsset->FoeMeshM02; break;
		case EFoeId::M03: Soft = ProcParamsAsset->FoeMeshM03; break;
		default: Soft = ProcParamsAsset->FoeMeshM01; break;
		}
	}
	else if (Config)
	{
		switch (FoeId)
		{
		case EFoeId::M02: Soft = Config->FoeMeshM02; break;
		case EFoeId::M03: Soft = Config->FoeMeshM03; break;
		default: Soft = Config->FoeMeshM01; break;
		}
	}
	return Soft.LoadSynchronous();
}

void UNightCourseDirector::ApplyHeroArtMesh()
{
	if (!RunnerPawn)
	{
		return;
	}
	TSoftObjectPtr<UStaticMesh> Soft;
	if (ProcParamsAsset)
	{
		Soft = ProcParamsAsset->HeroMesh;
	}
	else if (Config)
	{
		Soft = Config->HeroMesh;
	}
	if (UStaticMesh* Mesh = Soft.LoadSynchronous())
	{
		if (RunnerPawn->BodyMesh)
		{
			RunnerPawn->BodyMesh->SetStaticMesh(Mesh);
			RunnerPawn->BodyMesh->SetRelativeScale3D(FVector(1.f));
		}
		if (RunnerPawn->HeadMesh)
		{
			RunnerPawn->HeadMesh->SetHiddenInGame(true);
			RunnerPawn->HeadMesh->SetVisibility(false);
		}
	}
}

void UNightCourseDirector::SpawnBridgeActor(int32 BridgeIndex)
{
	UWorld* World = GetWorld();
	if (!World || !BridgeSpecs.IsValidIndex(BridgeIndex))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANightBridgeSegmentActor* Bridge = World->SpawnActor<ANightBridgeSegmentActor>(
		ANightBridgeSegmentActor::StaticClass(),
		BridgeSpecs[BridgeIndex].WorldLocation,
		FRotator(0.f, BridgeSpecs[BridgeIndex].YawDeg, 0.f),
		Params);
	if (!Bridge)
	{
		return;
	}
	Bridge->SetupBridge(BridgeSpecs[BridgeIndex], ResolveBridgeMesh(BridgeSpecs[BridgeIndex].MeshVariant));
	if (SpawnedBridges.Num() <= BridgeIndex)
	{
		SpawnedBridges.SetNum(BridgeIndex + 1);
	}
	SpawnedBridges[BridgeIndex] = Bridge;
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
	const FVector Loc = GetStoneWorldLocation(Index);
	const FRotator Facing = GetStoneWorldRotation(Index);
	ANightCourseStoneActor* Stone = World->SpawnActor<ANightCourseStoneActor>(
		SpawnClass,
		Loc,
		Facing,
		Params);
	if (!Stone)
	{
		return;
	}

	Stone->SetupStone(Index, StoneSpecs[Index]);
	Stone->SetTrackPose(Loc, Facing);
	if (UStaticMesh* FoeMesh = ResolveFoeMesh(StoneSpecs[Index].FoeId))
	{
		if (Stone->FoeCapsule)
		{
			Stone->FoeCapsule->SetStaticMesh(FoeMesh);
			Stone->FoeCapsule->SetRelativeScale3D(FVector(1.f));
		}
	}
	if (Config)
	{
		UMaterialInterface* FadeMat = Config->DistanceFadeMaterial;
		Stone->ConfigureDistanceFadeMaterial(FadeMat, Config->DistanceFade);
	}
	if (SpawnedStones.Num() <= Index)
	{
		SpawnedStones.SetNum(Index + 1);
	}
	SpawnedStones[Index] = Stone;
}

bool UNightCourseDirector::ImportProcParamsFromJsonFile(const FString& Path)
{
	if (!ProcParamsAsset)
	{
		ProcParamsAsset = NewObject<UNightProcParamsAsset>(this, TEXT("RuntimeProcParams"));
	}
	FString Err;
	if (!ProcParamsAsset->ImportFromJsonFile(Path, Err))
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] ImportProcParams failed: %s"), *Err);
		return false;
	}
	UE_LOG(LogTemp, Warning, TEXT("[NightCourse] Imported proc params from %s (seed=%d nodes=%d)"),
		*Path, ProcParamsAsset->Params.Seed, ProcParamsAsset->Params.TotalNodes);
	return true;
}

void UNightCourseDirector::RefreshStoneVisibility()
{
	if (!Config)
	{
		return;
	}

	const FNightDistanceFadeSettings& Fade = Config->DistanceFade;
	const int32 VisibleAhead = (RouteTaken != ENightRouteId::None)
		? FMath::Max(1, ActiveRouteRule.VisibleBlockCount)
		: 99;

	FVector AnchorWS = GetTrackLocation(ProgressDistance);
	float AnchorTrackDist = ProgressDistance;
	if (RunnerPawn)
	{
		AnchorWS = RunnerPawn->GetActorLocation();
		if (Fade.DistanceSpace == ENightDistanceFadeSpace::TrackDistance && Config)
		{
			const FVector Origin = Config->TrackOrigin;
			const FVector Forward = Config->TrackForward.GetSafeNormal();
			AnchorTrackDist = FVector::DotProduct(AnchorWS - Origin, Forward);
		}
	}

	for (int32 Index = 0; Index < SpawnedStones.Num(); ++Index)
	{
		ANightCourseStoneActor* Stone = SpawnedStones[Index];
		if (!Stone)
		{
			continue;
		}

		float Opacity = 1.f;
		if (Fade.bEnabled)
		{
			Opacity = ComputeStoneFadeOpacity(Index, AnchorWS, AnchorTrackDist);
		}

		if (Fade.bEnabled && Fade.bCombineWithVisibleBlockCull)
		{
			const int32 SoftMax = CurrentStoneIndex + VisibleAhead + Fade.SoftCullExtraBlocks;
			if (Index < CurrentStoneIndex)
			{
				if (!Fade.bKeepPastStonesOpaque)
				{
					Opacity = 0.f;
				}
			}
			else if (Index > SoftMax)
			{
				Opacity = 0.f;
			}
			else if (Index > CurrentStoneIndex + VisibleAhead)
			{
				const float SoftT = float(Index - (CurrentStoneIndex + VisibleAhead))
					/ float(FMath::Max(1, Fade.SoftCullExtraBlocks));
				Opacity *= FMath::Clamp(1.f - SoftT, 0.f, 1.f);
			}
		}
		else if (!Fade.bEnabled)
		{
			const bool bVisible = (Index >= CurrentStoneIndex) && (Index <= CurrentStoneIndex + VisibleAhead);
			Opacity = bVisible ? 1.f : 0.f;
		}

		if (Fade.bEnabled)
		{
			Stone->ApplyDistanceFade(Opacity, Fade);
		}
		else
		{
			const bool bVisible = Opacity > 0.5f;
			Stone->SetActorHiddenInGame(!bVisible);
			Stone->SetActorEnableCollision(bVisible);
		}
	}
}

float UNightCourseDirector::ComputeStoneFadeOpacity(int32 StoneIndex, const FVector& AnchorWS, float AnchorTrackDist) const
{
	if (!Config || !StoneSpecs.IsValidIndex(StoneIndex))
	{
		return 1.f;
	}

	const FNightDistanceFadeSettings& Fade = Config->DistanceFade;
	const FNightStoneSpec& Spec = StoneSpecs[StoneIndex];

	if (Fade.bKeepPastStonesOpaque && Spec.TrackDistance <= AnchorTrackDist + 1.f
		&& Fade.DistanceSpace == ENightDistanceFadeSpace::TrackDistance)
	{
		return Fade.MaxOpacity;
	}

	float DistCm = 0.f;
	switch (Fade.DistanceSpace)
	{
	case ENightDistanceFadeSpace::HorizontalXY:
	{
		const FVector StoneWS = GetStoneWorldLocation(StoneIndex);
		DistCm = FVector::Dist2D(StoneWS, AnchorWS);
		break;
	}
	case ENightDistanceFadeSpace::World3D:
	{
		const FVector StoneWS = GetStoneWorldLocation(StoneIndex);
		DistCm = FVector::Dist(StoneWS, AnchorWS);
		break;
	}
	case ENightDistanceFadeSpace::TrackDistance:
	default:
		DistCm = FMath::Abs(Spec.TrackDistance - AnchorTrackDist);
		break;
	}

	float FadeEnd = FMath::Max(Fade.FadeStartCm + 1.f, Fade.FadeEndCm);
	if (Fade.bScaleEndByVisibleBlocks && RouteTaken != ENightRouteId::None)
	{
		const float Ref = float(FMath::Max(1, Fade.ReferenceVisibleBlocks));
		const float Scale = FMath::Clamp(
			float(ActiveRouteRule.VisibleBlockCount) / Ref,
			Fade.VisibleBlockScaleMin,
			Fade.VisibleBlockScaleMax);
		FadeEnd *= Scale;
	}
	FadeEnd += Fade.SoftFalloffExtraCm;

	const float Start = Fade.FadeStartCm;
	float Alpha01 = 1.f;
	if (DistCm <= Start)
	{
		Alpha01 = 1.f;
	}
	else if (DistCm >= FadeEnd)
	{
		Alpha01 = 0.f;
	}
	else
	{
		const float T = (DistCm - Start) / FMath::Max(1.f, FadeEnd - Start);
		Alpha01 = 1.f - FMath::Clamp(T, 0.f, 1.f);
		Alpha01 = FMath::Pow(Alpha01, FMath::Max(0.1f, Fade.FadePower));
	}

	float Opacity = FMath::Lerp(Fade.MinOpacity, Fade.MaxOpacity, Alpha01);
	Opacity *= Fade.OpacityMul;
	return FMath::Clamp(Opacity, 0.f, 1.f);
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

	if (!ForkController)
	{
		ForkController = NewObject<UNightForkController>(this, TEXT("ForkController"));
		ForkController->OnForkResolved.AddDynamic(this, &UNightCourseDirector::OnForkControllerResolved);
	}

	ActiveBootstrap = Bootstrap;
	if (Config->bApplyLevelTableToBootstrap)
	{
		Config->ApplyLevelDefaultsToBootstrap(ActiveBootstrap);
	}
	ActiveLevelSettings = Config->GetLevelSettings(ActiveBootstrap.LevelId);

	bRunning = true;
	ElapsedSeconds = 0.f;
	CurrentStoneIndex = 0;
	ActiveBeatIndex = INDEX_NONE;
	bWindowOpen = false;
	bAdvancing = false;
	bPendingBranchHop = false;
	bKeySwapWarningActive = false;
	bKeySwapSafetyActive = false;
	bPendingOpenAfterKeySwap = false;
	ActiveControlScheme = ENightControlScheme::Normal;
	ApplyControlScheme(ENightControlScheme::Normal);
	RouteTaken = ENightRouteId::None;
	ActiveRouteRule = FNightRouteRuleRow();
	BranchFirstBeatIndex = INDEX_NONE;
	BranchFirstStoneIndex = INDEX_NONE;
	BranchAttackResolveCount = 0;
	BranchBeatsResolved = 0;
	NextKeySwapCueIndex = 0;
	CollectedIngredients.Reset();
	BranchCollectedIngredients.Reset();
	EnsureBaseCourse();
	BeatConsumed.Init(0, BeatSpecs.Num());
	SpawnedStones.Init(nullptr, StoneSpecs.Num());

	for (int32 Index = 0; Index < StoneSpecs.Num(); ++Index)
	{
		SpawnStoneActor(Index);
	}
	SpawnedBridges.Init(nullptr, BridgeSpecs.Num());
	for (int32 Index = 0; Index < BridgeSpecs.Num(); ++Index)
	{
		SpawnBridgeActor(Index);
	}
	ApplyHeroArtMesh();

	ProgressDistance = StoneSpecs.IsValidIndex(0) ? StoneSpecs[0].TrackDistance : 0.f;
	CurrentStoneIndex = 0;
	SyncPawnToProgress(true);
	RefreshStoneVisibility();
	SetPhase(ENightCoursePhase::BaseSegment);
	SetComponentTickEnabled(true);

	if (BeatSpecs.Num() > 0)
	{
		TryOpenBeat(0);
	}
	else if (Config->bEnableFork)
	{
		BeginForkChoice();
	}
	else
	{
		ExitBufferEndTime = ElapsedSeconds + Config->ExitBufferSeconds;
		SetPhase(ENightCoursePhase::ExitBuffer);
	}

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] StartNight stones=%d beats=%d bridges=%d fork=%d level=%d pair=%d seed=%d proc=%d"),
			StoneSpecs.Num(), BeatSpecs.Num(), BridgeSpecs.Num(), Config->bEnableFork ? 1 : 0,
			static_cast<int32>(ActiveBootstrap.LevelId), static_cast<int32>(ActiveBootstrap.ForkPair),
			ResolvedProcSeed, bUsingProcCourse ? 1 : 0);
	}
}

void UNightCourseDirector::TryOpenBeat(int32 BeatIndex)
{
	if (!BeatSpecs.IsValidIndex(BeatIndex) || BeatConsumed[BeatIndex])
	{
		return;
	}
	if (bKeySwapWarningActive || bKeySwapSafetyActive)
	{
		return;
	}

	ActiveBeatIndex = BeatIndex;
	bWindowOpen = true;
	CurrentStoneIndex = BeatSpecs[BeatIndex].FromStoneIndex;
	RefreshStoneVisibility();

	if (SpawnedStones.IsValidIndex(CurrentStoneIndex) && SpawnedStones[CurrentStoneIndex])
	{
		SpawnedStones[CurrentStoneIndex]->SetHighlight(true);
	}

	const FNightBeatSpec& Beat = BeatSpecs[BeatIndex];
	FNightJudgeRequest Request;
	Request.NodeIndex = BeatIndex;
	Request.Kind = Beat.Action;
	Request.WindowOpenTime = ElapsedSeconds;
	Request.WindowCloseTime = ElapsedSeconds + (Config ? Config->JudgeWindowSeconds : 3600.f);
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
	if (Phase == ENightCoursePhase::ForkChoice || Phase == ENightCoursePhase::BranchEnterBuffer
		|| bKeySwapWarningActive || bKeySwapSafetyActive)
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

	const FNightBeatSpec& Beat = BeatSpecs[BeatIndex];
	const bool bOnBranch = (Phase == ENightCoursePhase::BranchSegment);

#pragma region K2 moonyfli
	// Wrong button: penalty only, keep window open so Jump stays Jump and Attack stays Attack.
	if (Outcome == ENightJudgeOutcome::WrongButton)
	{
		if (INightFeelBridge* Feel = GetFeel())
		{
			float Penalty = Config->WrongPenalty;
			if (bOnBranch)
			{
				Penalty *= ActiveRouteRule.SoulPenaltyScale;
			}
			INightFeelBridge::Execute_ApplySoulPenalty(FeelBridgeObject, Penalty, Outcome);
			INightFeelBridge::Execute_PlayFailFeedback(FeelBridgeObject, Outcome, Beat.Action);
		}
		OnNodeResolved.Broadcast(BeatIndex, Beat.Action, Outcome);
		if (GetDebug().bLogEvents)
		{
			UE_LOG(LogTemp, Log, TEXT("[NightCourse] ResolveBeat idx=%d WrongButton (no advance)"), BeatIndex);
		}
		return;
	}
#pragma endregion K2 moonyfli

	BeatConsumed[BeatIndex] = 1;
	bWindowOpen = false;
	ActiveBeatIndex = INDEX_NONE;

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
			float Penalty = Config->MissPenalty;
			if (bOnBranch)
			{
				Penalty *= ActiveRouteRule.SoulPenaltyScale;
			}
			INightFeelBridge::Execute_ApplySoulPenalty(FeelBridgeObject, Penalty, Outcome);
			INightFeelBridge::Execute_PlayFailFeedback(FeelBridgeObject, Outcome, Beat.Action);
		}
	}

	const bool bAttackBeat = (Beat.Action == ENightNodeKind::Enemy);
	if (Outcome == ENightJudgeOutcome::Success && bAttackBeat && StoneSpecs.IsValidIndex(Beat.ToStoneIndex))
	{
		bool bGrantDrop = true;
		if (bOnBranch)
		{
			++BranchAttackResolveCount;
			const int32 EveryN = FMath::Max(1, ActiveRouteRule.DropRhythmEveryN);
			bGrantDrop = ((BranchAttackResolveCount % EveryN) == 0);
		}

		if (bGrantDrop)
		{
			EIngredientId DropId = StoneSpecs[Beat.ToStoneIndex].DropId;
			int32 DropCount = StoneSpecs[Beat.ToStoneIndex].DropCount;
			if (bOnBranch)
			{
				if (ActiveRouteRule.DropCycle.Num() > 0)
				{
					const int32 CycleIndex = (BranchAttackResolveCount - 1) % ActiveRouteRule.DropCycle.Num();
					DropId = ActiveRouteRule.DropCycle[CycleIndex];
				}
				DropCount = FMath::Max(1, DropCount * FMath::Max(1, ActiveRouteRule.BranchDropCountMul));
			}
			AddDrop(DropId, DropCount, bOnBranch);
			if (SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
			{
				SpawnedStones[Beat.ToStoneIndex]->ClearFoe(true);
				SpawnedStones[Beat.ToStoneIndex]->PlayDropBurst(DropId, DropCount);
			}
		}
		else if (SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
		{
			SpawnedStones[Beat.ToStoneIndex]->ClearFoe(true);
		}
	}
	else if (Outcome != ENightJudgeOutcome::Success && bAttackBeat
		&& SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
	{
		SpawnedStones[Beat.ToStoneIndex]->ClearFoe(false);
	}

	if (bOnBranch)
	{
		++BranchBeatsResolved;
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
	RefreshStoneVisibility();
	if (RunnerPawn)
	{
		RunnerPawn->BeginTrackAdvance(
			GetStoneWorldLocation(StoneIndex),
			GetStoneWorldRotation(StoneIndex),
			Config->AdvanceSpeed);
	}
}

void UNightCourseDirector::OnAdvanceArrived()
{
	bAdvancing = false;
	ProgressDistance = AdvanceTargetDistance;
	SyncPawnToProgress(true);
	RefreshStoneVisibility();

	if (bPendingBranchHop)
	{
		bPendingBranchHop = false;
		// Keep BranchEnterBuffer until the 1.2s pure-run window elapses.
		if (ElapsedSeconds >= BranchEnterBufferEndTime)
		{
			EnterBranchSegment();
		}
		return;
	}

	if (Phase == ENightCoursePhase::BranchSegment && TryBeginPendingKeySwap())
	{
		return;
	}

	OpenNextBeatOrExit();
}

void UNightCourseDirector::OpenNextBeatOrExit()
{
	if (bKeySwapWarningActive || bKeySwapSafetyActive)
	{
		return;
	}

	if (Phase == ENightCoursePhase::BaseSegment)
	{
		for (int32 Index = 0; Index < BaseBeatCount; ++Index)
		{
			if (BeatConsumed.IsValidIndex(Index) && !BeatConsumed[Index])
			{
				TryOpenBeat(Index);
				return;
			}
		}

		if (Config && Config->bEnableFork)
		{
			BeginForkChoice();
			return;
		}

		ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
		SetPhase(ENightCoursePhase::ExitBuffer);
		return;
	}

	if (Phase == ENightCoursePhase::BranchSegment)
	{
		if (TryBeginPendingKeySwap())
		{
			return;
		}

		for (int32 Index = BranchFirstBeatIndex; Index < BeatSpecs.Num(); ++Index)
		{
			if (BeatConsumed.IsValidIndex(Index) && !BeatConsumed[Index])
			{
				TryOpenBeat(Index);
				return;
			}
		}

		ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
		SetPhase(ENightCoursePhase::ExitBuffer);
		return;
	}
}

void UNightCourseDirector::BeginForkChoice()
{
	if (INightFeelBridge* Feel = GetFeel())
	{
		if (ActiveBeatIndex != INDEX_NONE)
		{
			INightFeelBridge::Execute_ClearJudgeRequest(FeelBridgeObject, ActiveBeatIndex);
		}
	}
	bWindowOpen = false;
	ActiveBeatIndex = INDEX_NONE;
	SetPhase(ENightCoursePhase::ForkChoice);

	if (!ForkController)
	{
		ForkController = NewObject<UNightForkController>(this, TEXT("ForkController"));
		ForkController->OnForkResolved.AddDynamic(this, &UNightCourseDirector::OnForkControllerResolved);
	}

	const float Timeout = Config ? Config->ForkTimeoutSeconds : 2.4f;
	const bool bPickLeft = Config ? Config->bForkTimeoutPickLeft : true;
	ForkController->BeginFork(ActiveBootstrap.ForkPair, Timeout, bPickLeft);

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] ForkChoice timeout=%.2f pair=%d"),
			Timeout, static_cast<int32>(ActiveBootstrap.ForkPair));
	}
}

void UNightCourseDirector::OnForkControllerResolved(ENightRouteId ChosenRoute, bool bTimedOut)
{
	HandleForkResolved(ChosenRoute, bTimedOut);
}

void UNightCourseDirector::HandleForkResolved(ENightRouteId ChosenRoute, bool bTimedOut)
{
	if (Phase != ENightCoursePhase::ForkChoice)
	{
		return;
	}

	RouteTaken = ChosenRoute;
	ActiveRouteRule = ResolveRouteRule(RouteTaken);

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] Fork resolved route=%d timedOut=%d"),
			static_cast<int32>(RouteTaken), bTimedOut ? 1 : 0);
	}

	AppendBranchCourse(RouteTaken);

	if (ActiveRouteRule.EnterDropCount > 0 && Config)
	{
		const EIngredientId EnterId = (ActiveRouteRule.EnterDropId != EIngredientId::None)
			? ActiveRouteRule.EnterDropId
			: Config->DefaultDropId;
		AddDrop(EnterId, ActiveRouteRule.EnterDropCount, true);
	}

	BeginBranchEnterBuffer();
}

void UNightCourseDirector::AppendBranchCourse(ENightRouteId ChosenRoute)
{
	if (!Config || StoneSpecs.Num() == 0)
	{
		return;
	}

	BranchFirstStoneIndex = StoneSpecs.Num();
	BranchFirstBeatIndex = BeatSpecs.Num();
	const int32 BridgeStart = BridgeSpecs.Num();

	if (bUsingProcCourse)
	{
		FNightGeneratedCourse Gen;
		Gen.Stones = StoneSpecs;
		Gen.Beats = BeatSpecs;
		Gen.Bridges = BridgeSpecs;
		Gen.ResolvedSeed = ResolvedProcSeed;
		Gen.BaseBeatCount = BaseBeatCount;
		UNightTrackGenerator::AppendBranch(Gen, ChosenRoute, ActiveProcParams, Config->TrackOrigin);
		StoneSpecs = Gen.Stones;
		BeatSpecs = Gen.Beats;
		BridgeSpecs = Gen.Bridges;

		if (Gen.KeySwaps.Num() > 0)
		{
			ActiveLevelSettings.KeySwaps = Gen.KeySwaps;
			ActiveLevelSettings.bKeySwapOnlyOnRouteC = (ChosenRoute == ENightRouteId::C);
			NextKeySwapCueIndex = 0;
		}
	}
	else
	{
		const float LastDist = StoneSpecs.Last().TrackDistance;
		const float StartDist = LastDist + Config->BranchEntryGapCm;
		TArray<FNightStoneSpec> BranchStones;
		TArray<FNightBeatSpec> BranchBeats;
		Config->BuildBranchCourse(ChosenRoute, StartDist, BranchFirstStoneIndex, BranchStones, BranchBeats);
		StoneSpecs.Append(BranchStones);
		BeatSpecs.Append(BranchBeats);
	}

	const int32 OldConsumed = BeatConsumed.Num();
	BeatConsumed.SetNum(BeatSpecs.Num());
	for (int32 Index = OldConsumed; Index < BeatConsumed.Num(); ++Index)
	{
		BeatConsumed[Index] = 0;
	}

	SpawnedStones.SetNum(StoneSpecs.Num());
	for (int32 Index = BranchFirstStoneIndex; Index < StoneSpecs.Num(); ++Index)
	{
		SpawnStoneActor(Index);
	}
	SpawnedBridges.SetNum(BridgeSpecs.Num());
	for (int32 Index = BridgeStart; Index < BridgeSpecs.Num(); ++Index)
	{
		SpawnBridgeActor(Index);
	}
	RefreshStoneVisibility();
}

void UNightCourseDirector::BeginBranchEnterBuffer()
{
	SetPhase(ENightCoursePhase::BranchEnterBuffer);
	BranchEnterBufferEndTime = ElapsedSeconds + (Config ? Config->BranchEnterBufferSeconds : 1.2f);

	if (BranchFirstStoneIndex != INDEX_NONE && StoneSpecs.IsValidIndex(BranchFirstStoneIndex))
	{
		bPendingBranchHop = true;
		BeginAdvanceToStone(BranchFirstStoneIndex);
	}
	else
	{
		EnterBranchSegment();
	}
}

void UNightCourseDirector::EnterBranchSegment()
{
	SetPhase(ENightCoursePhase::BranchSegment);
	RefreshStoneVisibility();

	if (TryBeginPendingKeySwap())
	{
		return;
	}

	if (BranchFirstBeatIndex != INDEX_NONE && BeatSpecs.IsValidIndex(BranchFirstBeatIndex))
	{
		TryOpenBeat(BranchFirstBeatIndex);
	}
	else
	{
		ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
		SetPhase(ENightCoursePhase::ExitBuffer);
	}
}

void UNightCourseDirector::ChooseForkLeft()
{
	if (IsForkChoiceActive() && ForkController)
	{
		ForkController->ChooseLeft();
	}
}

void UNightCourseDirector::ChooseForkRight()
{
	if (IsForkChoiceActive() && ForkController)
	{
		ForkController->ChooseRight();
	}
}

void UNightCourseDirector::DebugSkipFork()
{
	if (Phase == ENightCoursePhase::ForkChoice && ForkController)
	{
		ForkController->ChooseLeft();
	}
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
			DrawDebugSphere(GetWorld(), GetTrackLocation(Stone.TrackDistance), 25.f, 8,
				Stone.bHasFoe ? FColor::Red : FColor::Cyan, false, -1.f, 0, 1.f);
		}
	}

	if (Phase == ENightCoursePhase::ForkChoice && ForkController)
	{
		ForkController->TickFork(DeltaTime);
	}

	if (bKeySwapWarningActive && ElapsedSeconds >= KeySwapPhaseEndTime)
	{
		ApplyPendingKeySwapScheme();
	}
	else if (bKeySwapSafetyActive && ElapsedSeconds >= KeySwapPhaseEndTime)
	{
		EndKeySwapSafetyAndResume();
	}

	if (bAdvancing)
	{
		if (RunnerPawn && !RunnerPawn->IsTrackAdvancing())
		{
			OnAdvanceArrived();
		}
		if (Config && Config->DistanceFade.bEnabled && Config->DistanceFade.bUpdateEveryTick)
		{
			RefreshStoneVisibility();
		}

		// Reverse-fire DoT continues while advancing.
		if (Phase == ENightCoursePhase::BranchSegment
			&& ActiveRouteRule.bReverseFire
			&& ActiveRouteRule.DotSoulPerSecond > 0.f
			&& FeelBridgeObject)
		{
			const float Dot = ActiveRouteRule.DotSoulPerSecond * DeltaTime;
			if (Dot > 0.f)
			{
				INightFeelBridge::Execute_ApplySoulPenalty(FeelBridgeObject, Dot, ENightJudgeOutcome::Miss);
			}
		}
		return;
	}

	if (Config && Config->DistanceFade.bEnabled && Config->DistanceFade.bUpdateEveryTick)
	{
		RefreshStoneVisibility();
	}

	if (Phase == ENightCoursePhase::BranchSegment
		&& ActiveRouteRule.DotSoulPerSecond > 0.f
		&& FeelBridgeObject
		&& !bKeySwapWarningActive
		&& !bKeySwapSafetyActive)
	{
		const float Dot = ActiveRouteRule.DotSoulPerSecond * DeltaTime;
		if (Dot > 0.f)
		{
			INightFeelBridge::Execute_ApplySoulPenalty(FeelBridgeObject, Dot, ENightJudgeOutcome::Miss);
		}
	}

	if (Phase == ENightCoursePhase::BranchEnterBuffer
		&& !bPendingBranchHop
		&& ElapsedSeconds >= BranchEnterBufferEndTime)
	{
		EnterBranchSegment();
		return;
	}

	if (Phase == ENightCoursePhase::ExitBuffer && ElapsedSeconds >= ExitBufferEndTime)
	{
		ApplyCarryOutBonus();

		FNightResult Result;
		Result.bSuccess = true;
		Result.bFailedMidway = false;
		Result.RouteTaken = RouteTaken;
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

void UNightCourseDirector::AddDrop(EIngredientId Id, int32 Count, bool bCountAsBranch)
{
	if (Id == EIngredientId::None || Count <= 0)
	{
		return;
	}

	auto Accumulate = [](TArray<FIngredientStack>& Bags, EIngredientId InId, int32 InCount)
	{
		for (FIngredientStack& Stack : Bags)
		{
			if (Stack.Id == InId)
			{
				Stack.Count += InCount;
				return;
			}
		}
		FIngredientStack NewStack;
		NewStack.Id = InId;
		NewStack.Count = InCount;
		Bags.Add(NewStack);
	};

	Accumulate(CollectedIngredients, Id, Count);
	if (bCountAsBranch)
	{
		Accumulate(BranchCollectedIngredients, Id, Count);
	}
}

void UNightCourseDirector::ApplyCarryOutBonus()
{
	if (ActiveRouteRule.CarryOutBonus <= 0.f || BranchCollectedIngredients.Num() == 0)
	{
		return;
	}

	for (const FIngredientStack& Stack : BranchCollectedIngredients)
	{
		const int32 Bonus = FMath::CeilToInt(static_cast<float>(Stack.Count) * ActiveRouteRule.CarryOutBonus);
		if (Bonus > 0)
		{
			AddDrop(Stack.Id, Bonus, false);
			if (GetDebug().bLogEvents)
			{
				UE_LOG(LogTemp, Log, TEXT("[NightCourse] CarryOut +%d of %d (branch base %d)"),
					Bonus, static_cast<int32>(Stack.Id), Stack.Count);
			}
		}
	}
}

bool UNightCourseDirector::ShouldRunKeySwaps() const
{
	if (!Config || !Config->bEnableKeySwap)
	{
		return false;
	}
	if (ActiveLevelSettings.KeySwaps.Num() == 0)
	{
		return false;
	}
	if (ActiveLevelSettings.bKeySwapOnlyOnRouteC && RouteTaken != ENightRouteId::C)
	{
		return false;
	}
	return true;
}

bool UNightCourseDirector::TryBeginPendingKeySwap()
{
	if (bKeySwapWarningActive || bKeySwapSafetyActive || !ShouldRunKeySwaps())
	{
		return false;
	}

	while (NextKeySwapCueIndex < ActiveLevelSettings.KeySwaps.Num())
	{
		const FNightKeySwapCue& Cue = ActiveLevelSettings.KeySwaps[NextKeySwapCueIndex];
		if (BranchBeatsResolved < Cue.TriggerAfterBranchBeats)
		{
			return false;
		}

		++NextKeySwapCueIndex;

		const bool bSkipFirst =
			Config->bHonorKeyCoinSkipFirstSwap
			&& ActiveBootstrap.GiftBuffs.bKeyCoin
			&& (NextKeySwapCueIndex == 1);
		if (bSkipFirst)
		{
			if (GetDebug().bLogEvents)
			{
				UE_LOG(LogTemp, Log, TEXT("[NightCourse] KeySwap cue skipped by KeyCoin"));
			}
			continue;
		}

		BeginKeySwapWarning(Cue);
		return true;
	}
	return false;
}

void UNightCourseDirector::BeginKeySwapWarning(const FNightKeySwapCue& Cue)
{
	PendingKeySwapCue = Cue;
	bPendingOpenAfterKeySwap = true;
	bKeySwapWarningActive = true;
	bKeySwapSafetyActive = false;

	if (INightFeelBridge* Feel = GetFeel())
	{
		if (ActiveBeatIndex != INDEX_NONE)
		{
			INightFeelBridge::Execute_ClearJudgeRequest(FeelBridgeObject, ActiveBeatIndex);
		}
	}
	bWindowOpen = false;
	ActiveBeatIndex = INDEX_NONE;

	const float Warn = (Cue.WarningSeconds > 0.f)
		? Cue.WarningSeconds
		: (Config ? Config->DefaultKeySwapWarningSeconds : 0.8f);
	KeySwapPhaseEndTime = ElapsedSeconds + Warn;

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] KeySwap WARNING %.2fs (branchBeats=%d)"),
			Warn, BranchBeatsResolved);
	}
}

void UNightCourseDirector::ApplyPendingKeySwapScheme()
{
	bKeySwapWarningActive = false;
	bKeySwapSafetyActive = true;

	ENightControlScheme NextScheme = PendingKeySwapCue.TargetScheme;
	if (PendingKeySwapCue.bToggle)
	{
		NextScheme = (ActiveControlScheme == ENightControlScheme::Normal)
			? ENightControlScheme::Swapped
			: ENightControlScheme::Normal;
	}
	ApplyControlScheme(NextScheme);

	const float Safety = (PendingKeySwapCue.SafetyHoldSeconds > 0.f)
		? PendingKeySwapCue.SafetyHoldSeconds
		: (Config ? Config->DefaultKeySwapSafetySeconds : 0.6f);
	KeySwapPhaseEndTime = ElapsedSeconds + Safety;

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] KeySwap APPLIED scheme=%d safety=%.2fs"),
			static_cast<int32>(ActiveControlScheme), Safety);
	}
}

void UNightCourseDirector::EndKeySwapSafetyAndResume()
{
	bKeySwapSafetyActive = false;
	bKeySwapWarningActive = false;
	const bool bResume = bPendingOpenAfterKeySwap;
	bPendingOpenAfterKeySwap = false;
	if (bResume && Phase == ENightCoursePhase::BranchSegment)
	{
		OpenNextBeatOrExit();
	}
}

void UNightCourseDirector::ApplyControlScheme(ENightControlScheme Scheme)
{
	ActiveControlScheme = Scheme;
	if (FeelBridgeObject)
	{
		INightFeelBridge::Execute_SetControlScheme(FeelBridgeObject, Scheme);
	}
}

float UNightCourseDirector::GetKeySwapSecondsRemaining() const
{
	if (!bKeySwapWarningActive && !bKeySwapSafetyActive)
	{
		return 0.f;
	}
	return FMath::Max(0.f, KeySwapPhaseEndTime - ElapsedSeconds);
}

void UNightCourseDirector::DebugForceKeySwap()
{
	if (!bRunning || Phase != ENightCoursePhase::BranchSegment)
	{
		return;
	}
	FNightKeySwapCue Cue;
	Cue.TriggerAfterBranchBeats = 0;
	Cue.WarningSeconds = Config ? Config->DefaultKeySwapWarningSeconds : 0.8f;
	Cue.SafetyHoldSeconds = Config ? Config->DefaultKeySwapSafetySeconds : 0.6f;
	Cue.bToggle = true;
	BeginKeySwapWarning(Cue);
}

void UNightCourseDirector::FinishNight(const FNightResult& Result)
{
	SetComponentTickEnabled(false);
	bRunning = false;
	bAdvancing = false;
	bWindowOpen = false;
	bPendingBranchHop = false;
	bKeySwapWarningActive = false;
	bKeySwapSafetyActive = false;
	bPendingOpenAfterKeySwap = false;
	if (ForkController)
	{
		ForkController->CancelFork();
	}
	SetPhase(ENightCoursePhase::Finished);
	OnFinished.Broadcast(Result);
	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] Finished success=%d route=%d ingredients=%d soul=%.1f scheme=%d"),
			Result.bSuccess ? 1 : 0, static_cast<int32>(Result.RouteTaken), Result.Ingredients.Num(), Result.SoulLeft,
			static_cast<int32>(ActiveControlScheme));
	}
}

void UNightCourseDirector::DebugForceFinish(bool bSuccess)
{
	ApplyCarryOutBonus();
	FNightResult Result;
	Result.bSuccess = bSuccess;
	Result.bFailedMidway = !bSuccess;
	Result.RouteTaken = RouteTaken;
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
	bPendingBranchHop = false;
	bKeySwapWarningActive = false;
	bKeySwapSafetyActive = false;
	bPendingOpenAfterKeySwap = false;
	ActiveBeatIndex = INDEX_NONE;
	if (ForkController)
	{
		ForkController->CancelFork();
	}
	ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
	SetPhase(ENightCoursePhase::ExitBuffer);
}
#pragma endregion K2 moonyfli
