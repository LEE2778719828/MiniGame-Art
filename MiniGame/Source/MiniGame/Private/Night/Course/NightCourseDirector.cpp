#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightCourseAtomRouteData.h"
#include "Night/Course/NightCourseRuleData.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightTrackGenerator.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Course/NightCoursePawn.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/BoxComponent.h"
#include "Misc/PackageName.h"

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

void UNightCourseDirector::SetLayoutBoundsComponent(
	UBoxComponent* InBoundsComponent,
	bool bInEnforceBounds)
{
	LayoutBoundsComponent = InBoundsComponent;
	bEnforceLayoutBounds = bInEnforceBounds;
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
	VisualBindings.Reset();
	BuildCourseForPreview(StoneSpecs, BeatSpecs, BridgeSpecs, VisualBindings);
}

namespace NightCourseAtom_Private
{
	static UClass* ResolveAtomClass(
		const TSoftClassPtr<ANightCourseAtomActor>& SoftClass)
	{
		if (UClass* LoadedClass = SoftClass.LoadSynchronous())
		{
			return LoadedClass;
		}

		const FString AssetPath = SoftClass.ToSoftObjectPath().ToString();
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}

		if (AssetPath.Contains(TEXT(".")))
		{
			return LoadObject<UClass>(nullptr, *AssetPath);
		}

		const FString AssetName = FPackageName::GetShortName(AssetPath);
		const FString GeneratedClassPath =
			AssetPath + TEXT(".") + AssetName + TEXT("_C");
		return LoadObject<UClass>(nullptr, *GeneratedClassPath);
	}

	static FTransform MakeAtomWorldTransform(
		const FTransform& TargetEntry,
		const FTransform& LocalEntry)
	{
		const FQuat WorldRotation =
			TargetEntry.GetRotation() * LocalEntry.GetRotation().Inverse();
		const FVector WorldLocation =
			TargetEntry.GetLocation() - WorldRotation.RotateVector(LocalEntry.GetLocation());
		return FTransform(WorldRotation, WorldLocation, FVector::OneVector);
	}

	static FVector GetLocalStoneLocation(const FNightStoneSpec& Stone)
	{
		return Stone.bUseWorldPose
			? Stone.WorldLocation
			: FVector(Stone.TrackDistance, 0.f, 0.f);
	}

	static float GetTrackDistance(
		const FVector& WorldLocation,
		const FVector& TrackOrigin,
		const FVector& TrackForward)
	{
		return FVector::DotProduct(WorldLocation - TrackOrigin, TrackForward);
	}
}

bool UNightCourseDirector::BuildCourseForPreview(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges) const
{
	TArray<FNightAtomVisualBinding> IgnoredVisualBindings;
	return BuildCourseForPreview(OutStones, OutBeats, OutBridges, IgnoredVisualBindings);
}

bool UNightCourseDirector::BuildCourseForPreview(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges,
	TArray<FNightAtomVisualBinding>& OutVisualBindings) const
{
	OutStones.Reset();
	OutBeats.Reset();
	OutBridges.Reset();
	OutVisualBindings.Reset();
	if (!Config)
	{
		return false;
	}

	if (Config->CourseRuleData && Config->CourseRuleData->bEnabled)
	{
		if (!Config->AtomRoute)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse] CourseRuleData is enabled but AtomRoute is missing; no fallback is allowed."));
			return false;
		}
		return BuildAtomRouteCourse(OutStones, OutBeats, OutBridges, OutVisualBindings);
	}

	if (Config->AtomRoute && Config->AtomRoute->bEnabled)
	{
		return BuildAtomRouteCourse(OutStones, OutBeats, OutBridges, OutVisualBindings);
	}

	if (Config->ProcParams.bEnableProcGenerator)
	{
		const FNightGeneratedCourse Generated = UNightTrackGenerator::GenerateBaseOnly(
			Config->ProcParams,
			Config->TrackOrigin,
			Config->TrackForward);
		OutStones = Generated.Stones;
		OutBeats = Generated.Beats;
		OutBridges = Generated.Bridges;
		return OutStones.Num() > 0;
	}

	Config->BuildCourse(OutStones, OutBeats);
	return OutStones.Num() > 0;
}

bool UNightCourseDirector::BuildAtomRouteCourse(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges) const
{
	TArray<FNightAtomVisualBinding> IgnoredVisualBindings;
	return BuildAtomRouteCourse(OutStones, OutBeats, OutBridges, IgnoredVisualBindings);
}

bool UNightCourseDirector::BuildAtomRouteCourse(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges,
	TArray<FNightAtomVisualBinding>& OutVisualBindings) const
{
	OutStones.Reset();
	OutBeats.Reset();
	OutBridges.Reset();
	OutVisualBindings.Reset();
	if (!Config || !Config->AtomRoute)
	{
		return false;
	}

	const UNightCourseAtomRouteData* Route = Config->AtomRoute;
	const UNightCourseRuleData* Rule = Config->CourseRuleData;
	const bool bUsePlannerRule = Rule && Rule->bEnabled;

	FString RouteError;
	if (bUsePlannerRule)
	{
		if (!Rule->ValidateRuleAgainstLibrary(Route, RouteError))
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse] Planner rule is invalid: %s"), *RouteError);
			return false;
		}
	}
	else if (!Route->ValidateRoute(RouteError))
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] Atom route is invalid: %s"), *RouteError);
		return false;
	}

	const FVector TrackForward = Config->TrackForward.GetSafeNormal().IsNearlyZero()
		? FVector::ForwardVector
		: Config->TrackForward.GetSafeNormal();
	const FTransform InitialEntry(
		TrackForward.Rotation().Quaternion(),
		Config->TrackOrigin,
		FVector::OneVector);
	FTransform PreviousExit = InitialEntry;
	int32 PreviousLastStoneIndex = INDEX_NONE;
	bool bFirstAtom = true;
	const int32 AtomCount = bUsePlannerRule ? Rule->Route.Num() : Route->AtomSequence.Num();
	FRandomStream RuleRandomStream(bUsePlannerRule ? Rule->Seed : 0);

	for (int32 AtomSequenceIndex = 0; AtomSequenceIndex < AtomCount; ++AtomSequenceIndex)
	{
		const FString AtomKey = bUsePlannerRule
			? Rule->Route[AtomSequenceIndex].AtomKey
			: Route->AtomSequence[AtomSequenceIndex];
		const TSoftClassPtr<ANightCourseAtomActor>* AtomClassRef = Route->AtomMap.Find(AtomKey);
		UClass* AtomClass = AtomClassRef
			? NightCourseAtom_Private::ResolveAtomClass(*AtomClassRef)
			: nullptr;
		if (!AtomClass || !AtomClass->IsChildOf(ANightCourseAtomActor::StaticClass()))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse] Atom route key '%s' has no valid Atom BP class."),
				*AtomKey);
			return false;
		}

		const ANightCourseAtomActor* AtomDefaults =
			AtomClass->GetDefaultObject<ANightCourseAtomActor>();
		if (!AtomDefaults)
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse] Atom route key '%s' has no valid CDO."), *AtomKey);
			return false;
		}

		ANightCourseAtomActor* AtomInstance = nullptr;
		const ANightCourseAtomActor* AtomSource = AtomDefaults;
		if (UWorld* World = GetWorld())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags |= RF_Transient;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AtomInstance = World->SpawnActor<ANightCourseAtomActor>(
				AtomClass,
				FTransform::Identity,
				SpawnParams);
			if (AtomInstance)
			{
				AtomSource = AtomInstance;
			}
		}

		FString AtomError;
		if (!AtomSource->ValidateAtom(AtomError))
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse] Atom '%s' is invalid: %s"), *AtomKey, *AtomError);
			if (AtomInstance)
			{
				AtomInstance->Destroy();
			}
			return false;
		}

		TArray<FNightStoneSpec> LocalStones;
		TArray<FNightBeatSpec> LocalBeats;
		TArray<FNightBridgeSpec> LocalBridges;
		TArray<FNightAtomVisualBinding> LocalVisualBindings;
		AtomSource->GetLocalArtSpecs(
			LocalStones,
			LocalBeats,
			LocalBridges,
			LocalVisualBindings);
		if (LocalStones.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse] Atom '%s' has no landing points or local stones."), *AtomKey);
			if (AtomInstance)
			{
				AtomInstance->Destroy();
			}
			return false;
		}

		if (bUsePlannerRule)
		{
			const FNightRuleAtomEntry& RuleEntry = Rule->Route[AtomSequenceIndex];
			if (RuleEntry.Actions.Num() != LocalStones.Num() - 1)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse] Rule '%s' has %d actions for %d landing points; expected %d."),
					*AtomKey,
					RuleEntry.Actions.Num(),
					LocalStones.Num(),
					FMath::Max(0, LocalStones.Num() - 1));
				if (AtomInstance)
				{
					AtomInstance->Destroy();
				}
				return false;
			}

			LocalBeats.Reset();
			for (int32 ActionIndex = 0; ActionIndex < RuleEntry.Actions.Num(); ++ActionIndex)
			{
				FNightBeatSpec Beat;
				Beat.FromStoneIndex = ActionIndex;
				Beat.ToStoneIndex = ActionIndex + 1;
				Beat.Action = RuleEntry.Actions[ActionIndex];
				LocalBeats.Add(Beat);
				LocalStones[ActionIndex + 1].bHasFoe = Beat.Action == ENightNodeKind::Enemy;
				LocalStones[ActionIndex + 1].FoeId = LocalStones[ActionIndex + 1].bHasFoe
					? EFoeId::M01
					: EFoeId::None;
			}
		}

		const float EntryAnchorError = FVector::Dist(
			NightCourseAtom_Private::GetLocalStoneLocation(LocalStones[0]),
			AtomSource->GetEntryAnchorTransform().GetLocation());
		const float ExitAnchorError = FVector::Dist(
			NightCourseAtom_Private::GetLocalStoneLocation(LocalStones.Last()),
			AtomSource->GetExitAnchorTransform().GetLocation());
		if (EntryAnchorError > 5.f || ExitAnchorError > 5.f)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[NightCourse] Atom '%s' anchor mismatch entry=%.1fcm exit=%.1fcm"),
				*AtomKey,
				EntryAnchorError,
				ExitAnchorError);
		}

		FTransform TargetEntry = InitialEntry;
		if (!bFirstAtom)
		{
			const FVector JumpForward =
				PreviousExit.GetRotation().GetForwardVector().GetSafeNormal();
			TargetEntry.SetLocation(
				PreviousExit.GetLocation()
				+ JumpForward * FMath::Max(0.f, Route->TransitionJumpGapCm));
			TargetEntry.SetRotation(PreviousExit.GetRotation());
		}

		const FTransform LocalEntry = AtomSource->GetEntryAnchorTransform();
		const FTransform LocalExit = AtomSource->GetExitAnchorTransform();
		const FTransform BaseAtomWorld = NightCourseAtom_Private::MakeAtomWorldTransform(
			TargetEntry,
			LocalEntry);

		TArray<float> CandidateYaws;
		if (bUsePlannerRule && AtomSource->bAllowDeterministicRandomYaw)
		{
			CandidateYaws.Add(RuleRandomStream.FRandRange(AtomSource->MinYawDeg, AtomSource->MaxYawDeg));
			CandidateYaws.Add(AtomSource->MinYawDeg);
			CandidateYaws.Add(AtomSource->MaxYawDeg);
			CandidateYaws.Add((AtomSource->MinYawDeg + AtomSource->MaxYawDeg) * 0.5f);
		}
		else
		{
			CandidateYaws.Add(0.f);
		}

		FTransform AtomWorld = BaseAtomWorld;
		bool bFoundValidTransform = false;
		for (const float CandidateYaw : CandidateYaws)
		{
			const FQuat DeltaRotation = FRotator(0.f, CandidateYaw, 0.f).Quaternion();
			FTransform Candidate = BaseAtomWorld;
			Candidate.SetRotation(DeltaRotation * BaseAtomWorld.GetRotation());
			Candidate.SetLocation(
				TargetEntry.GetLocation()
				+ DeltaRotation.RotateVector(BaseAtomWorld.GetLocation() - TargetEntry.GetLocation()));
			if (IsAtomTransformInsideLayoutBounds(AtomSource, Candidate))
			{
				AtomWorld = Candidate;
				bFoundValidTransform = true;
				break;
			}
		}
		if (!bFoundValidTransform)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse] Atom '%s' has no valid rotation candidate in scene bounds."),
				*AtomKey);
			if (AtomInstance)
			{
				AtomInstance->Destroy();
			}
			return false;
		}

		const int32 StoneOffset = OutStones.Num();
		const int32 BridgeOffset = OutBridges.Num();
		for (const FNightStoneSpec& LocalStone : LocalStones)
		{
			FNightStoneSpec WorldStone = LocalStone;
			const FVector WorldLocation = AtomWorld.TransformPosition(
				NightCourseAtom_Private::GetLocalStoneLocation(LocalStone));
			WorldStone.bUseWorldPose = true;
			WorldStone.WorldLocation = WorldLocation;
			WorldStone.TrackDistance = NightCourseAtom_Private::GetTrackDistance(
				WorldLocation,
				Config->TrackOrigin,
				TrackForward);
			WorldStone.YawDeg = AtomWorld.TransformRotation(
				FRotator(0.f, LocalStone.YawDeg, 0.f).Quaternion()).Rotator().Yaw;
			OutStones.Add(WorldStone);
		}

		if (!bFirstAtom && PreviousLastStoneIndex != INDEX_NONE && StoneOffset < OutStones.Num())
		{
			FNightBeatSpec TransitionBeat;
			TransitionBeat.FromStoneIndex = PreviousLastStoneIndex;
			TransitionBeat.ToStoneIndex = StoneOffset;
			TransitionBeat.Action = bUsePlannerRule
				? Rule->TransitionAction
				: ENightNodeKind::Hazard;
			OutBeats.Add(TransitionBeat);
		}

		for (FNightBeatSpec LocalBeat : LocalBeats)
		{
			LocalBeat.FromStoneIndex += StoneOffset;
			LocalBeat.ToStoneIndex += StoneOffset;
			OutBeats.Add(LocalBeat);
		}

		for (const FNightBridgeSpec& LocalBridge : LocalBridges)
		{
			FNightBridgeSpec WorldBridge = LocalBridge;
			WorldBridge.FromStoneIndex += StoneOffset;
			WorldBridge.ToStoneIndex += StoneOffset;
			WorldBridge.WorldLocation = AtomWorld.TransformPosition(LocalBridge.WorldLocation);
			WorldBridge.YawDeg = AtomWorld.TransformRotation(
				FRotator(0.f, LocalBridge.YawDeg, 0.f).Quaternion()).Rotator().Yaw;
			OutBridges.Add(WorldBridge);
		}

		for (FNightAtomVisualBinding LocalBinding : LocalVisualBindings)
		{
			LocalBinding.AtomKey = AtomKey;
			LocalBinding.AtomSequenceIndex = AtomSequenceIndex;
			if (LocalBinding.bIsBridge)
			{
				LocalBinding.BridgeIndex += BridgeOffset;
			}
			else
			{
				LocalBinding.StoneIndex += StoneOffset;
			}
			LocalBinding.LocalTransform.SetLocation(
				AtomWorld.TransformPosition(LocalBinding.LocalTransform.GetLocation()));
			LocalBinding.LocalTransform.SetRotation(
				AtomWorld.TransformRotation(LocalBinding.LocalTransform.GetRotation()));
			LocalBinding.LocalTransform.SetScale3D(
				AtomWorld.GetScale3D() * LocalBinding.LocalTransform.GetScale3D());
			OutVisualBindings.Add(MoveTemp(LocalBinding));
		}

		PreviousExit = FTransform(
			AtomWorld.TransformRotation(LocalExit.GetRotation()),
			AtomWorld.TransformPosition(LocalExit.GetLocation()),
			FVector::OneVector);
		PreviousLastStoneIndex = OutStones.Num() - 1;
		bFirstAtom = false;

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[NightCourse] Atom '%s' stones=%d beats=%d bridges=%d worldEntry=%s worldExit=%s"),
			*AtomKey,
			LocalStones.Num(),
			LocalBeats.Num(),
			LocalBridges.Num(),
			*AtomWorld.GetLocation().ToCompactString(),
			*PreviousExit.GetLocation().ToCompactString());
		if (AtomInstance)
		{
			AtomInstance->Destroy();
		}
	}

	return OutStones.Num() > 0;
}

bool UNightCourseDirector::IsAtomTransformInsideLayoutBounds(
	const ANightCourseAtomActor* AtomDefaults,
	const FTransform& AtomWorld) const
{
	if (!bEnforceLayoutBounds || !LayoutBoundsComponent)
	{
		return true;
	}
	if (!AtomDefaults)
	{
		return false;
	}

	const FBox WorldBounds = LayoutBoundsComponent->Bounds.GetBox();
	if (!WorldBounds.IsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] Layout Bounds component has invalid world bounds."));
		return false;
	}

	FVector LocalMin;
	FVector LocalMax;
	AtomDefaults->GetLocalArtBounds(LocalMin, LocalMax);
	const FVector Corners[8] =
	{
		FVector(LocalMin.X, LocalMin.Y, LocalMin.Z),
		FVector(LocalMin.X, LocalMin.Y, LocalMax.Z),
		FVector(LocalMin.X, LocalMax.Y, LocalMin.Z),
		FVector(LocalMin.X, LocalMax.Y, LocalMax.Z),
		FVector(LocalMax.X, LocalMin.Y, LocalMin.Z),
		FVector(LocalMax.X, LocalMin.Y, LocalMax.Z),
		FVector(LocalMax.X, LocalMax.Y, LocalMin.Z),
		FVector(LocalMax.X, LocalMax.Y, LocalMax.Z)
	};
	for (const FVector& LocalCorner : Corners)
	{
		if (!WorldBounds.IsInside(AtomWorld.TransformPosition(LocalCorner)))
		{
			return false;
		}
	}
	return true;
}

bool UNightCourseDirector::HasVisualBindingForStone(int32 StoneIndex) const
{
	for (const FNightAtomVisualBinding& Binding : VisualBindings)
	{
		if (!Binding.bIsBridge && Binding.StoneIndex == StoneIndex)
		{
			return true;
		}
	}
	return false;
}

void UNightCourseDirector::SpawnVisualBinding(int32 BindingIndex)
{
	UWorld* World = GetWorld();
	if (!World || !VisualBindings.IsValidIndex(BindingIndex))
	{
		return;
	}

	const FNightAtomVisualBinding& Binding = VisualBindings[BindingIndex];
	TSubclassOf<AActor> VisualClass = Binding.VisualPrefabClass;
	if (!Binding.bIsBridge
		&& StoneSpecs.IsValidIndex(Binding.StoneIndex)
		&& StoneSpecs[Binding.StoneIndex].bHasFoe
		&& Binding.AlternateVisualPrefabClass)
	{
		VisualClass = Binding.AlternateVisualPrefabClass;
	}
	if (!VisualClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse] Visual binding %d has no prefab class (stone=%d bridge=%d)."),
			BindingIndex,
			Binding.StoneIndex,
			Binding.BridgeIndex);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* VisualActor = World->SpawnActor<AActor>(
		VisualClass.Get(),
		Binding.LocalTransform.GetLocation(),
		Binding.LocalTransform.GetRotation().Rotator(),
		Params);
	if (!VisualActor)
	{
		return;
	}
	VisualActor->SetActorTransform(Binding.LocalTransform);

	if (!Binding.bIsBridge && StoneSpecs.IsValidIndex(Binding.StoneIndex))
	{
		if (ANightCourseStoneActor* VisualStone = Cast<ANightCourseStoneActor>(VisualActor))
		{
			VisualStone->SetupStone(Binding.StoneIndex, StoneSpecs[Binding.StoneIndex]);
		}
		else if (ANightBridgeSegmentActor* VisualBridge = Cast<ANightBridgeSegmentActor>(VisualActor))
		{
			FNightBridgeSpec PadSpec;
			PadSpec.FromStoneIndex = Binding.StoneIndex;
			PadSpec.ToStoneIndex = Binding.StoneIndex;
			PadSpec.WorldLocation = Binding.LocalTransform.GetLocation();
			PadSpec.YawDeg = Binding.LocalTransform.GetRotation().Rotator().Yaw;
			PadSpec.LengthScale = 1.f;
			VisualBridge->SetupBridge(
				PadSpec,
				nullptr,
				nullptr,
				FVector::ZeroVector,
				1.f);
		}
	}
	else if (Binding.bIsBridge && BridgeSpecs.IsValidIndex(Binding.BridgeIndex))
	{
		if (ANightBridgeSegmentActor* VisualBridge = Cast<ANightBridgeSegmentActor>(VisualActor))
		{
			VisualBridge->SetupBridge(
				BridgeSpecs[Binding.BridgeIndex],
				nullptr,
				nullptr,
				FVector::ZeroVector,
				1.f);
		}
	}

	if (SpawnedVisualActors.IsValidIndex(BindingIndex))
	{
		SpawnedVisualActors[BindingIndex] = VisualActor;
	}
}

void UNightCourseDirector::SetStoneVisualVisibility(int32 StoneIndex, bool bVisible)
{
	for (int32 BindingIndex = 0; BindingIndex < VisualBindings.Num(); ++BindingIndex)
	{
		const FNightAtomVisualBinding& Binding = VisualBindings[BindingIndex];
		if (Binding.bIsBridge || Binding.StoneIndex != StoneIndex)
		{
			continue;
		}

		if (StoneSpecs.IsValidIndex(StoneIndex)
			&& Binding.AlternateVisualPrefabClass
			&& SpawnedVisualActors.IsValidIndex(BindingIndex)
			&& ((bVisible && StoneSpecs[StoneIndex].bHasFoe)
				|| (!bVisible && !StoneSpecs[StoneIndex].bHasFoe)))
		{
			if (SpawnedVisualActors[BindingIndex])
			{
				SpawnedVisualActors[BindingIndex]->Destroy();
				SpawnedVisualActors[BindingIndex] = nullptr;
			}
			SpawnVisualBinding(BindingIndex);
		}

		if (SpawnedVisualActors.IsValidIndex(BindingIndex)
			&& SpawnedVisualActors[BindingIndex])
		{
			SpawnedVisualActors[BindingIndex]->SetActorHiddenInGame(false);
			SpawnedVisualActors[BindingIndex]->SetActorEnableCollision(true);
			if (!bVisible && !Binding.AlternateVisualPrefabClass)
			{
				SpawnedVisualActors[BindingIndex]->SetActorHiddenInGame(true);
				SpawnedVisualActors[BindingIndex]->SetActorEnableCollision(false);
			}
		}
	}
}

void UNightCourseDirector::SpawnStoneActor(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World || !StoneSpecs.IsValidIndex(Index))
	{
		return;
	}

	const bool bHasArtVisual = HasVisualBindingForStone(Index);
	UClass* SpawnClass = bHasArtVisual
		? ANightCourseStoneActor::StaticClass()
		: ((Config && Config->StoneClass)
			? Config->StoneClass.Get()
			: ANightCourseStoneActor::StaticClass());
	bool bFoeClassSelected = false;
	if (!bHasArtVisual && StoneSpecs[Index].bHasFoe && Config)
	{
		switch (StoneSpecs[Index].FoeId)
		{
		case EFoeId::M01:
			if (Config->FoeClassM01) { SpawnClass = Config->FoeClassM01.Get(); bFoeClassSelected = true; }
			break;
		case EFoeId::M02:
			if (Config->FoeClassM02) { SpawnClass = Config->FoeClassM02.Get(); bFoeClassSelected = true; }
			break;
		case EFoeId::M03:
			if (Config->FoeClassM03) { SpawnClass = Config->FoeClassM03.Get(); bFoeClassSelected = true; }
			break;
		case EFoeId::M04:
			if (Config->FoeClassM04) { SpawnClass = Config->FoeClassM04.Get(); bFoeClassSelected = true; }
			break;
		case EFoeId::M05:
			if (Config->FoeClassM05) { SpawnClass = Config->FoeClassM05.Get(); bFoeClassSelected = true; }
			break;
		default: break;
		}
	}
	if (!bHasArtVisual && StoneSpecs[Index].bHasFoe && !bFoeClassSelected)
	{
		// Never substitute another visual class. A missing BP intentionally
		// spawns an empty native actor so the configuration error is visible.
		SpawnClass = ANightCourseStoneActor::StaticClass();
	}
	if (StoneSpecs[Index].bHasFoe)
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[NightCourse] Foe M%02d stone=%d class=%s"),
			static_cast<int32>(StoneSpecs[Index].FoeId),
			Index,
			*GetNameSafe(SpawnClass));
	}

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
	UClass* BridgeClass = ANightBridgeSegmentActor::StaticClass();
	bool bHasArtVisual = false;
	for (const FNightAtomVisualBinding& Binding : VisualBindings)
	{
		if (Binding.bIsBridge && Binding.BridgeIndex == Index)
		{
			bHasArtVisual = true;
			break;
		}
	}
	if (Config && !bHasArtVisual)
	{
		UClass* ConfiguredClass =
			BridgeSpecs[Index].MeshVariant == 0
				? Config->BridgeClassA.Get()
				: Config->BridgeClassB.Get();
		if (ConfiguredClass)
		{
			BridgeClass = ConfiguredClass;
		}
	}
	ANightBridgeSegmentActor* Bridge = World->SpawnActor<ANightBridgeSegmentActor>(
		BridgeClass,
		BridgeSpecs[Index].WorldLocation,
		FRotator(0.f, BridgeSpecs[Index].YawDeg, 0.f),
		Params);
	if (!Bridge)
	{
		return;
	}

	Bridge->SetupBridge(
		BridgeSpecs[Index],
		nullptr,
		nullptr,
		FVector::ZeroVector,
		1.f);
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
	SpawnedVisualActors.Init(nullptr, VisualBindings.Num());

	for (int32 Index = 0; Index < BridgeSpecs.Num(); ++Index)
	{
		SpawnBridgeActor(Index);
	}
	for (int32 Index = 0; Index < StoneSpecs.Num(); ++Index)
	{
		SpawnStoneActor(Index);
	}
	for (int32 Index = 0; Index < VisualBindings.Num(); ++Index)
	{
		SpawnVisualBinding(Index);
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
		SetStoneVisualVisibility(Beat.ToStoneIndex, true);
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
		StoneSpecs[Beat.ToStoneIndex].bHasFoe = false;
		SetStoneVisualVisibility(Beat.ToStoneIndex, false);
	}
	else if (Outcome != ENightJudgeOutcome::Success && bAttackBeat
		&& SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
	{
		// Wrong/Miss still advances onto the stone but foe can stay or clear — clear to keep chain readable.
		SpawnedStones[Beat.ToStoneIndex]->ClearFoe(false);
		StoneSpecs[Beat.ToStoneIndex].bHasFoe = false;
		SetStoneVisualVisibility(Beat.ToStoneIndex, false);
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
