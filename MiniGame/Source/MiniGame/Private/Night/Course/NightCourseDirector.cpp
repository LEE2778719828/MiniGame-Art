#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightCourseAtomRouteData.h"
#include "Night/Course/NightCourseRuleData.h"
#include "Night/Course/NightForkController.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightBridgeSegmentActor.h"
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

void UNightCourseDirector::EmitDebugMessage(const FString& Message, bool bIsError)
{
	OnDebugMessage.Broadcast(Message, bIsError);
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

bool UNightCourseDirector::EnsureCourse(FString& OutError)
{
	OutError.Reset();
	StoneSpecs.Reset();
	BeatSpecs.Reset();
	BridgeSpecs.Reset();
	VisualBindings.Reset();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Compose] EnsureCourse begin Config='%s' runtimeContext=%d."),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		bBuildingRuntimeCourse || bRunning ? 1 : 0);
	if (!BuildCourseForPreview(StoneSpecs, BeatSpecs, BridgeSpecs, VisualBindings))
	{
		OutError = TEXT("Course composition failed; see the first NightCourse error for the authoritative cause.");
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] EnsureCourse failed: %s."),
			*OutError);
		StoneSpecs.Reset();
		BeatSpecs.Reset();
		BridgeSpecs.Reset();
		VisualBindings.Reset();
		return false;
	}
	if (StoneSpecs.Num() <= 0)
	{
		OutError = TEXT("Course composition produced zero stones.");
		UE_LOG(LogTemp, Error, TEXT("[NightCourse][Stage=Compose] EnsureCourse failed: %s."), *OutError);
		return false;
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Compose] EnsureCourse complete stones=%d beats=%d bridges=%d visualBindings=%d."),
		StoneSpecs.Num(),
		BeatSpecs.Num(),
		BridgeSpecs.Num(),
		VisualBindings.Num());
	return true;
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

	static EFoeId PickFoeId(
		FRandomStream& Rng,
		const FNightBootstrap& Bootstrap,
		const UNightG1CourseConfig* Config,
		EFoeId Fallback,
		int32 FoeOrdinal)
	{
		const TArray<EFoeId>* Pool = nullptr;
		const bool bTaotieOverrideActive =
			Bootstrap.GiftBuffs.bTaotieBox
			&& Config
			&& FoeOrdinal < FMath::Max(0, Config->TaotieFoeOverrideCount);
		if (Bootstrap.FoeWeightOverride.Num() > 0
			&& (!Bootstrap.GiftBuffs.bTaotieBox || bTaotieOverrideActive))
		{
			Pool = &Bootstrap.FoeWeightOverride;
		}
		else if (Config && Config->FoeWeightPool.Num() > 0)
		{
			Pool = &Config->FoeWeightPool;
		}
		if (!Pool)
		{
			return Fallback;
		}

		TArray<EFoeId> ValidPool;
		for (const EFoeId Candidate : *Pool)
		{
			if (Candidate != EFoeId::None)
			{
				ValidPool.Add(Candidate);
			}
		}
		return ValidPool.Num() > 0
			? ValidPool[Rng.RandRange(0, ValidPool.Num() - 1)]
			: Fallback;
	}

	static EIngredientId PickIngredientDropId(
		FRandomStream& Rng,
		const UNightG1CourseConfig* Config,
		const EIngredientId AuthoredFallback)
	{
		if (!Config || !Config->bRandomizeEnemyDrops)
		{
			return AuthoredFallback != EIngredientId::None
				? AuthoredFallback
				: (Config ? Config->DefaultDropId : EIngredientId::F01_LingGu);
		}

		TArray<EIngredientId> ValidPool;
		for (const EIngredientId Candidate : Config->IngredientDropPool)
		{
			if (Candidate != EIngredientId::None)
			{
				ValidPool.Add(Candidate);
			}
		}
		if (ValidPool.Num() == 0)
		{
			ValidPool = {
				EIngredientId::F01_LingGu,
				EIngredientId::F02_YinShanJun,
				EIngredientId::F03_ChiYanJiao,
				EIngredientId::F04_YueLinYu,
				EIngredientId::F05_XuanYuQin
			};
		}
		return ValidPool[Rng.RandRange(0, ValidPool.Num() - 1)];
	}

	static bool SelectWeightedTemplate(
		const TArray<FNightRuleAtomEntry>& Templates,
		FRandomStream& Rng,
		FNightRuleAtomEntry& OutTemplate,
		int32& OutTemplateIndex)
	{
		int32 TotalWeight = 0;
		for (const FNightRuleAtomEntry& Template : Templates)
		{
			if (Template.Weight > 0
				&& TotalWeight <= MAX_int32 - Template.Weight)
			{
				TotalWeight += Template.Weight;
			}
		}
		if (TotalWeight <= 0)
		{
			return false;
		}

		const int32 Pick = Rng.RandRange(1, TotalWeight);
		int32 AccumulatedWeight = 0;
		for (int32 TemplateIndex = 0; TemplateIndex < Templates.Num(); ++TemplateIndex)
		{
			const int32 Weight = Templates[TemplateIndex].Weight;
			if (Weight <= 0)
			{
				continue;
			}
			AccumulatedWeight += Weight;
			if (Pick <= AccumulatedWeight)
			{
				OutTemplate = Templates[TemplateIndex];
				OutTemplateIndex = TemplateIndex;
				return true;
			}
		}
		return false;
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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] BuildCourseForPreview aborted: Config is null."));
		return false;
	}

	if (!Config->CourseRuleData || !Config->CourseRuleData->bEnabled)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] Canonical CourseRuleData is missing or disabled on Config='%s'."),
			*Config->GetPathName());
		return false;
	}
	if (!Config->AtomRoute || !Config->AtomRoute->bEnabled)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] Canonical AtomRoute is missing or disabled on Config='%s'."),
			*Config->GetPathName());
		return false;
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Compose] Inputs Config='%s' Rule='%s' AtomLibrary='%s'."),
		*Config->GetPathName(),
		*Config->CourseRuleData->GetPathName(),
		*Config->AtomRoute->GetPathName());
	return BuildAtomRouteCourse(OutStones, OutBeats, OutBridges, OutVisualBindings);
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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] BuildAtomRouteCourse aborted: Config='%s' AtomLibrary='%s'."),
			Config ? *Config->GetPathName() : TEXT("<null>"),
			Config && Config->AtomRoute ? *Config->AtomRoute->GetPathName() : TEXT("<null>"));
		return false;
	}

	const UNightCourseAtomRouteData* Route = Config->AtomRoute;
	const UNightCourseRuleData* Rule = Config->CourseRuleData;
	if (!Rule || !Rule->bEnabled)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] Canonical CourseRuleData is missing or disabled."));
		return false;
	}
	const bool bRuntimeBuildContext = bBuildingRuntimeCourse || bRunning;
	const ENightRouteId BuildRoute = CurrentRoute != ENightRouteId::None
		? CurrentRoute
		: (bRuntimeBuildContext ? ENightRouteId::None : Config->PreviewRoute);
	TArray<FNightRuleAtomEntry> PlannerEntries;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Compose] Begin Atom composition context=%s seed=%d route=%d baseTemplates=%d baseTarget=%d branchRoutes=%d TransitionJumpGapCm=%.1f."),
		bRuntimeBuildContext ? TEXT("Runtime") : TEXT("Preview"),
		bHasRuntimeSeed ? RuntimeSeed : Rule->Seed,
		static_cast<int32>(BuildRoute),
		Rule->BaseRoute.Num(),
		Rule->BaseAtomCount > 0 ? Rule->BaseAtomCount : Rule->BaseRoute.Num(),
		Rule->BranchRoutes.Num(),
		Route->TransitionJumpGapCm);

	FString RouteError;
	if (!Rule->ValidateRuleAgainstLibrary(Route, RouteError))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=ValidateRule] Rule='%s' AtomLibrary='%s' invalid: %s"),
			*Rule->GetPathName(),
			*Route->GetPathName(),
			*RouteError);
		return false;
	}

	const int32 ConfigForkIndex = Config->ForkAfterBaseAtomIndex;
	const int32 RuleForkIndex = Rule->ForkAfterBaseAtomIndex;
	const int32 ForkIndex = ConfigForkIndex != INDEX_NONE
		? ConfigForkIndex
		: RuleForkIndex;
	const int32 AuthoredBaseAtomCount = Rule->BaseAtomCount > 0
		? Rule->BaseAtomCount
		: Rule->BaseRoute.Num();
	const bool bUsesForkBase =
		BuildRoute != ENightRouteId::None
		|| (Config->bEnableFork && ForkIndex != INDEX_NONE);
	if (bUsesForkBase
		&& (ForkIndex <= 0 || ForkIndex > AuthoredBaseAtomCount))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] ForkAfterBaseAtomIndex=%d is outside generated base route length %d."),
			ForkIndex,
			AuthoredBaseAtomCount);
		return false;
	}
	const int32 GeneratedBaseAtomCount = bUsesForkBase
		? ForkIndex
		: AuthoredBaseAtomCount;
	if (GeneratedBaseAtomCount <= 0)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] Generated base Atom count is %d; configure BaseAtomCount or BaseRoute templates."),
			GeneratedBaseAtomCount);
		return false;
	}

	FRandomStream TemplateSelectionStream(
		(bHasRuntimeSeed ? RuntimeSeed : Rule->Seed) ^ 0x54454D50);
	auto AppendWeightedTemplates =
		[&PlannerEntries, &TemplateSelectionStream](
			const TArray<FNightRuleAtomEntry>& Templates,
			const int32 TargetCount,
			const FString& QueueLabel) -> bool
	{
		if (Templates.Num() == 0 || TargetCount <= 0)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=AtomTemplateSelect] %s has no templates or target count (%d templates, target=%d)."),
				*QueueLabel,
				Templates.Num(),
				TargetCount);
			return false;
		}

		PlannerEntries.Reserve(PlannerEntries.Num() + TargetCount);
		for (int32 SlotIndex = 0; SlotIndex < TargetCount; ++SlotIndex)
		{
			FNightRuleAtomEntry SelectedTemplate;
			int32 TemplateIndex = INDEX_NONE;
			if (!NightCourseAtom_Private::SelectWeightedTemplate(
				Templates,
				TemplateSelectionStream,
				SelectedTemplate,
				TemplateIndex))
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse][Stage=AtomTemplateSelect] %s slot=%d could not select a positive-weight template."),
					*QueueLabel,
					SlotIndex);
				return false;
			}
			PlannerEntries.Add(MoveTemp(SelectedTemplate));
			UE_LOG(
				LogTemp,
				Log,
				TEXT("[NightCourse][Stage=AtomTemplateSelect] %s slot=%d template=%d weight=%d."),
				*QueueLabel,
				SlotIndex,
				TemplateIndex,
				PlannerEntries.Last().Weight);
		}
		return true;
	};

	if (!AppendWeightedTemplates(
		Rule->BaseRoute,
		GeneratedBaseAtomCount,
		TEXT("BaseRoute")))
	{
		return false;
	}

	if (BuildRoute != ENightRouteId::None)
	{
		const FNightRuleAtomQueue* BranchQueue = Rule->BranchRoutes.Find(BuildRoute);
		if (!BranchQueue || BranchQueue->Atoms.Num() == 0)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse] Selected route %d has no authored branch Atom templates."),
				static_cast<int32>(BuildRoute));
			return false;
		}
		const int32 BranchAtomCount = BranchQueue->TargetAtomCount > 0
			? BranchQueue->TargetAtomCount
			: BranchQueue->Atoms.Num();
		if (!AppendWeightedTemplates(
			BranchQueue->Atoms,
			BranchAtomCount,
			FString::Printf(
				TEXT("BranchRoutes[%d]"),
				static_cast<int32>(BuildRoute))))
		{
			return false;
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Compose] Planner queue resolved route=%d generatedBase=%d generatedBranch=%d total=%d forkIndex=%d."),
		static_cast<int32>(BuildRoute),
		GeneratedBaseAtomCount,
		PlannerEntries.Num() - GeneratedBaseAtomCount,
		PlannerEntries.Num(),
		ForkIndex);

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

	const int32 AtomCount = PlannerEntries.Num();
	int32 FoeOrdinal = 0;
	FRandomStream AtomSelectionStream(
		(bHasRuntimeSeed ? RuntimeSeed : Rule->Seed) ^ 0x41544F4D);
	FRandomStream RuleRandomStream(
		bHasRuntimeSeed ? RuntimeSeed : Rule->Seed);

	for (int32 AtomSlotIndex = 0; AtomSlotIndex < AtomCount; ++AtomSlotIndex)
	{
		const FNightRuleAtomEntry& PlannerEntry = PlannerEntries[AtomSlotIndex];
		FString AtomKey = PlannerEntry.AtomKey.TrimStartAndEnd();
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=AtomSelect] slot=%d requestedKey='%s' actions=%d."),
			AtomSlotIndex,
			AtomKey.IsEmpty() ? TEXT("<auto>") : *AtomKey,
			PlannerEntry.Actions.Num());
		if (AtomKey.IsEmpty())
		{
			TArray<FString> Candidates;
			TArray<FString> CandidateFailureReasons;
			Route->GetCompatibleAtomKeys(
				PlannerEntry.Actions.Num(),
				Candidates,
				&CandidateFailureReasons);
			if (Candidates.Num() == 0)
			{
				for (const FString& Reason : CandidateFailureReasons)
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[NightCourse] Seed=%d route=%d slot=%d Atom candidate rejected: %s"),
						bHasRuntimeSeed ? RuntimeSeed : Rule->Seed,
						static_cast<int32>(BuildRoute),
						AtomSlotIndex,
						*Reason);
				}
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse] Seed=%d route=%d slot=%d has no compatible Atom candidate for %d actions; library candidates=%d."),
					bHasRuntimeSeed ? RuntimeSeed : Rule->Seed,
					static_cast<int32>(BuildRoute),
					AtomSlotIndex,
					PlannerEntry.Actions.Num(),
					Route->AtomMap.Num());
				return false;
			}

			const int32 CandidateIndex =
				AtomSelectionStream.RandRange(0, Candidates.Num() - 1);
			AtomKey = Candidates[CandidateIndex];
			UE_LOG(
				LogTemp,
				Log,
				TEXT("[NightCourse] Seed=%d route=%d slot=%d selected AtomKey='%s' from %d compatible candidates."),
				bHasRuntimeSeed ? RuntimeSeed : Rule->Seed,
				static_cast<int32>(BuildRoute),
				AtomSlotIndex,
				*AtomKey,
				Candidates.Num());
		}
		else
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("[NightCourse] Seed=%d route=%d slot=%d uses explicit AtomKey='%s'."),
				bHasRuntimeSeed ? RuntimeSeed : Rule->Seed,
				static_cast<int32>(BuildRoute),
				AtomSlotIndex,
				*AtomKey);
		}

		const TSoftClassPtr<ANightCourseAtomActor>* AtomClassRef = Route->AtomMap.Find(AtomKey);
		UClass* AtomClass = AtomClassRef
			? NightCourseAtom_Private::ResolveAtomClass(*AtomClassRef)
			: nullptr;
		if (!AtomClass || !AtomClass->IsChildOf(ANightCourseAtomActor::StaticClass()))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=AtomLoad] slot=%d key='%s' path='%s' has no valid Atom BP class."),
				AtomSlotIndex,
				*AtomKey,
				AtomClassRef ? *AtomClassRef->ToSoftObjectPath().ToString() : TEXT("<missing map key>"));
			return false;
		}

		const ANightCourseAtomActor* AtomDefaults =
			AtomClass->GetDefaultObject<ANightCourseAtomActor>();
		if (!AtomDefaults)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=AtomLoad] slot=%d key='%s' class='%s' has no valid CDO."),
				AtomSlotIndex,
				*AtomKey,
				*GetNameSafe(AtomClass));
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
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=AtomValidate] slot=%d key='%s' class='%s' invalid: %s"),
				AtomSlotIndex,
				*AtomKey,
				*GetNameSafe(AtomClass),
				*AtomError);
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
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=AtomExtract] slot=%d key='%s' returned no local stones (beats=%d bridges=%d visuals=%d)."),
				AtomSlotIndex,
				*AtomKey,
				LocalBeats.Num(),
				LocalBridges.Num(),
				LocalVisualBindings.Num());
			if (AtomInstance)
			{
				AtomInstance->Destroy();
			}
			return false;
		}

		const FNightRuleAtomEntry& RuleEntry = PlannerEntry;
		if (RuleEntry.Actions.Num() != LocalStones.Num() - 1)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=AtomBind] slot=%d key='%s' has %d actions for %d landing points; expected %d."),
				AtomSlotIndex,
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
				? NightCourseAtom_Private::PickFoeId(
					RuleRandomStream,
					ActiveBootstrap,
					Config,
					Config->DefaultFoeId,
					FoeOrdinal++)
				: EFoeId::None;
			if (LocalStones[ActionIndex + 1].bHasFoe)
			{
				if (Config->bRandomizeEnemyDrops)
				{
					LocalStones[ActionIndex + 1].DropId =
						NightCourseAtom_Private::PickIngredientDropId(
							RuleRandomStream,
							Config,
							LocalStones[ActionIndex + 1].DropId);
				}
				else if (LocalStones[ActionIndex + 1].DropId == EIngredientId::None)
				{
					LocalStones[ActionIndex + 1].DropId = Config->DefaultDropId;
				}
				if (LocalStones[ActionIndex + 1].DropCount <= 0)
				{
					LocalStones[ActionIndex + 1].DropCount = Config->DefaultDropCount;
				}
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
		if (AtomSource->bAllowDeterministicRandomYaw)
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
				TEXT("[NightCourse][Stage=AtomTransform] slot=%d key='%s' has no valid rotation candidate in scene bounds."),
				AtomSlotIndex,
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
			TransitionBeat.Action = ENightNodeKind::Hazard;
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
			LocalBinding.AtomSlotIndex = AtomSlotIndex;
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
			TEXT("[NightCourse][Stage=AtomCompose] slot=%d key='%s' stones=%d beats=%d bridges=%d worldEntry=%s worldExit=%s"),
			AtomSlotIndex,
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

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Compose] Atom composition complete atoms=%d stones=%d beats=%d bridges=%d visualBindings=%d."),
		AtomCount,
		OutStones.Num(),
		OutBeats.Num(),
		OutBridges.Num(),
		OutVisualBindings.Num());
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

	const FTransform BoundsTransform = LayoutBoundsComponent->GetComponentTransform();
	const FVector BoundsExtent = LayoutBoundsComponent->GetScaledBoxExtent();
	if (BoundsExtent.IsNearlyZero())
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] Layout Bounds component has zero extent."));
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
		const FVector BoundsLocal = BoundsTransform.InverseTransformPosition(
			AtomWorld.TransformPosition(LocalCorner));
		if (FMath::Abs(BoundsLocal.X) > BoundsExtent.X
			|| FMath::Abs(BoundsLocal.Y) > BoundsExtent.Y
			|| FMath::Abs(BoundsLocal.Z) > BoundsExtent.Z)
		{
			return false;
		}
	}
	return true;
}

void UNightCourseDirector::SpawnVisualBinding(int32 BindingIndex)
{
	UWorld* World = GetWorld();
	if (!World || !VisualBindings.IsValidIndex(BindingIndex))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnVisual] binding=%d aborted: World='%s' bindingCount=%d."),
			BindingIndex,
			*GetNameSafe(World),
			VisualBindings.Num());
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
		const bool bEnemyVisualRequired =
			!Binding.bIsBridge
			&& StoneSpecs.IsValidIndex(Binding.StoneIndex)
			&& StoneSpecs[Binding.StoneIndex].bHasFoe;
		if (!Binding.bIsBridge && !bEnemyVisualRequired)
		{
			// Normal LandingPoint character previews are editor-only. The
			// runtime only spawns the alternate enemy prefab for Kill beats.
			return;
		}
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse] Visual binding %d has no prefab class (stone=%d bridge=%d)."),
			BindingIndex,
			Binding.StoneIndex,
			Binding.BridgeIndex);
		return;
	}

	if (!Binding.bIsBridge
		&& VisualClass->IsChildOf(ANightBridgeSegmentActor::StaticClass()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse] LandingPoint visual binding %d resolves to a Bridge BP; bridge visuals must use BridgeVisualComponent."),
			BindingIndex);
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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnVisual] binding=%d failed to spawn class='%s' atom='%s' stone=%d bridge=%d."),
			BindingIndex,
			*GetNameSafe(VisualClass.Get()),
			*Binding.AtomKey,
			Binding.StoneIndex,
			Binding.BridgeIndex);
		return;
	}
	VisualActor->SetActorTransform(Binding.LocalTransform);
	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("[NightCourse][Stage=SpawnVisual] binding=%d spawned class='%s' atom='%s' stone=%d bridge=%d."),
		BindingIndex,
		*GetNameSafe(VisualClass.Get()),
		*Binding.AtomKey,
		Binding.StoneIndex,
		Binding.BridgeIndex);

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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnStone] index=%d aborted: World='%s' stoneCount=%d."),
			Index,
			*GetNameSafe(World),
			StoneSpecs.Num());
		return;
	}

	// Atom visuals are authored by the Atom BP. The native stone is retained
	// only as the gameplay/collision carrier; no legacy Config mesh fallback is
	// allowed to replace an artist-authored visual.
	UClass* SpawnClass = ANightCourseStoneActor::StaticClass();
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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnStone] index=%d failed to spawn class='%s' location=%s."),
			Index,
			*GetNameSafe(SpawnClass),
			*GetStoneWorldLocation(Index).ToCompactString());
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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnBridge] index=%d aborted: World='%s' bridgeCount=%d."),
			Index,
			*GetNameSafe(World),
			BridgeSpecs.Num());
		return;
	}

	for (const FNightAtomVisualBinding& Binding : VisualBindings)
	{
		if (Binding.bIsBridge && Binding.BridgeIndex == Index)
		{
			// BridgeVisualComponent owns this bridge as its own actor. Do not
			// also spawn the native/configured compatibility actor.
			UE_LOG(
				LogTemp,
				Verbose,
				TEXT("[NightCourse][Stage=SpawnBridge] index=%d owned by Atom BridgeVisual binding; native carrier skipped."),
				Index);
			return;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// A BridgeVisual component owns its own actor. If an authored visual is
	// absent, keep a native compatibility actor instead of consulting removed
	// Config bridge-class fallbacks.
	UClass* BridgeClass = ANightBridgeSegmentActor::StaticClass();
	ANightBridgeSegmentActor* Bridge = World->SpawnActor<ANightBridgeSegmentActor>(
		BridgeClass,
		BridgeSpecs[Index].WorldLocation,
		FRotator(0.f, BridgeSpecs[Index].YawDeg, 0.f),
		Params);
	if (!Bridge)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnBridge] index=%d failed to spawn class='%s' location=%s."),
			Index,
			*GetNameSafe(BridgeClass),
			*BridgeSpecs[Index].WorldLocation.ToCompactString());
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

void UNightCourseDirector::ClearSpawnedCourseActors()
{
	for (AActor* Actor : SpawnedVisualActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	for (ANightBridgeSegmentActor* Bridge : SpawnedBridges)
	{
		if (Bridge)
		{
			Bridge->Destroy();
		}
	}
	for (ANightCourseStoneActor* Stone : SpawnedStones)
	{
		if (Stone)
		{
			Stone->Destroy();
		}
	}
	SpawnedVisualActors.Reset();
	SpawnedBridges.Reset();
	SpawnedStones.Reset();
}

void UNightCourseDirector::SpawnCourseActors()
{
	SpawnedStones.Init(nullptr, StoneSpecs.Num());
	SpawnedBridges.Init(nullptr, BridgeSpecs.Num());
	SpawnedVisualActors.Init(nullptr, VisualBindings.Num());
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Spawn] Begin carriers stones=%d bridges=%d visualBindings=%d."),
		StoneSpecs.Num(),
		BridgeSpecs.Num(),
		VisualBindings.Num());

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

	int32 MissingStoneActors = 0;
	for (const ANightCourseStoneActor* Stone : SpawnedStones)
	{
		MissingStoneActors += Stone ? 0 : 1;
	}
	int32 MissingBridgeActors = 0;
	for (const ANightBridgeSegmentActor* Bridge : SpawnedBridges)
	{
		MissingBridgeActors += Bridge ? 0 : 1;
	}
	int32 SpawnedVisualCount = 0;
	for (const AActor* Visual : SpawnedVisualActors)
	{
		SpawnedVisualCount += Visual ? 1 : 0;
	}
	if (MissingStoneActors == 0 && MissingBridgeActors == 0)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=Spawn] Complete stones=%d/%d bridges=%d/%d visuals=%d/%d missingStone=%d missingBridge=%d."),
			StoneSpecs.Num() - MissingStoneActors,
			StoneSpecs.Num(),
			BridgeSpecs.Num() - MissingBridgeActors,
			BridgeSpecs.Num(),
			SpawnedVisualCount,
			VisualBindings.Num(),
			MissingStoneActors,
			MissingBridgeActors);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Spawn] Complete stones=%d/%d bridges=%d/%d visuals=%d/%d missingStone=%d missingBridge=%d."),
			StoneSpecs.Num() - MissingStoneActors,
			StoneSpecs.Num(),
			BridgeSpecs.Num() - MissingBridgeActors,
			BridgeSpecs.Num(),
			SpawnedVisualCount,
			VisualBindings.Num(),
			MissingStoneActors,
			MissingBridgeActors);
	}
}

bool UNightCourseDirector::ValidateConfiguration(FString& OutError) const
{
	OutError.Reset();
	auto FailValidation = [&OutError]() -> bool
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Validate] FAILED: %s"),
			OutError.IsEmpty() ? TEXT("<empty>") : *OutError);
		return false;
	};
	if (!Config)
	{
		OutError = TEXT("Course Config is null.");
		return FailValidation();
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Validate] Begin Config='%s' Rule='%s' AtomLibrary='%s' RouteRules='%s'."),
		*Config->GetPathName(),
		Config->CourseRuleData ? *Config->CourseRuleData->GetPathName() : TEXT("<null>"),
		Config->AtomRoute ? *Config->AtomRoute->GetPathName() : TEXT("<null>"),
		Config->RouteRules ? *Config->RouteRules->GetPathName() : TEXT("<null>"));

	if (Config->ForkTimeoutSeconds < 0.f
		|| Config->BranchEnterBufferSeconds < 0.f
		|| Config->BranchEntryGapCm <= 0.f
		|| Config->ExitBufferSeconds < 0.f
		|| Config->WrongPenalty < 0.f
		|| Config->MissPenalty < 0.f
		|| Config->StartingSoul < 0.f
		|| Config->DefaultDropCount < 0
		|| Config->TaotieFoeOverrideCount < 0
		|| Config->DefaultKeySwapWarningSeconds < 0.f
		|| Config->DefaultKeySwapSafetySeconds < 0.f)
	{
		OutError = TEXT("Course Config contains a negative value or non-positive branch gap.");
		return FailValidation();
	}

	TSet<ENightLevelId> LevelRuleIds;
	for (const FNightLevelCourseRule& LevelRule : Config->LevelRules)
	{
		if (LevelRuleIds.Contains(LevelRule.LevelId))
		{
			OutError = TEXT("LevelRules contains duplicate LevelId rows.");
			return FailValidation();
		}
		LevelRuleIds.Add(LevelRule.LevelId);
	}
	auto ValidateKeySwapCues = [&OutError, &FailValidation](const TArray<FNightKeySwapCue>& Cues)
	{
		for (const FNightKeySwapCue& Cue : Cues)
		{
			if (Cue.TriggerAfterBranchBeats < 0
				|| Cue.WarningSeconds < 0.f
				|| Cue.SafetyHoldSeconds < 0.f)
			{
				OutError = TEXT("KeySwapCues contains a negative trigger or duration.");
				return FailValidation();
			}
		}
		return true;
	};
	if (!ValidateKeySwapCues(Config->KeySwapCues))
	{
		return false;
	}
	for (const FNightLevelCourseRule& LevelRule : Config->LevelRules)
	{
		if (!ValidateKeySwapCues(LevelRule.KeySwapCues))
		{
			return false;
		}
	}

	if (!Config->CourseRuleData || !Config->CourseRuleData->bEnabled)
	{
		OutError = TEXT("Canonical CourseRuleData is missing or disabled.");
		return FailValidation();
	}
	if (!Config->AtomRoute || !Config->AtomRoute->bEnabled)
	{
		OutError = TEXT("Canonical AtomRoute is missing or disabled.");
		return FailValidation();
	}
	if (!Config->AtomRoute->ValidateRoute(OutError))
	{
		return FailValidation();
	}
	if (!Config->CourseRuleData->ValidateRuleAgainstLibrary(Config->AtomRoute, OutError))
	{
		return FailValidation();
	}

	const int32 ForkIndex = Config->ForkAfterBaseAtomIndex != INDEX_NONE
		? Config->ForkAfterBaseAtomIndex
		: Config->CourseRuleData->ForkAfterBaseAtomIndex;
	const int32 BaseRouteLength =
		Config->CourseRuleData->GetBaseRouteLength();
	if (ForkIndex != INDEX_NONE
		&& (ForkIndex <= 0 || ForkIndex > BaseRouteLength))
	{
		OutError = FString::Printf(
			TEXT("ForkAfterBaseAtomIndex=%d is invalid for base route length %d."),
			ForkIndex,
			BaseRouteLength);
		return FailValidation();
	}
	if (Config->bEnableFork
		&& ForkIndex != INDEX_NONE
		&& Config->CourseRuleData->BranchRoutes.Num() == 0)
	{
		OutError = TEXT("ForkAfterBaseAtomIndex is set but no branch Atom queues are authored.");
		return FailValidation();
	}
	if (Config->bEnableFork && Config->CourseRuleData->BranchRoutes.Num() > 0)
	{
		if (ForkIndex == INDEX_NONE)
		{
			OutError = TEXT("Branch Atom queues require ForkAfterBaseAtomIndex.");
			return FailValidation();
		}

		ENightRouteId LeftRoute = ENightRouteId::None;
		ENightRouteId RightRoute = ENightRouteId::None;
		bool bForcedAB = false;
		UNightForkController::ResolvePairRoutes(
			ActiveForkPair,
			LeftRoute,
			RightRoute,
			bForcedAB);
		if (!Config->CourseRuleData->HasBranchRoute(LeftRoute)
			|| !Config->CourseRuleData->HasBranchRoute(RightRoute))
		{
			OutError = FString::Printf(
				TEXT("Fork pair requires authored branch queues for routes %d and %d."),
				static_cast<int32>(LeftRoute),
				static_cast<int32>(RightRoute));
			return FailValidation();
		}
	}

	const bool bRuleBranch =
		Config->CourseRuleData->BranchRoutes.Num() > 0;
	if (Config->bEnableFork && bRuleBranch)
	{
		if (!Config->RouteRules)
		{
			OutError = TEXT("A RouteRules asset with A/B/C rows is required for an enabled fork.");
			return FailValidation();
		}
		ENightRouteId LeftRoute = ENightRouteId::None;
		ENightRouteId RightRoute = ENightRouteId::None;
		bool bForcedAB = false;
		UNightForkController::ResolvePairRoutes(
			ActiveForkPair,
			LeftRoute,
			RightRoute,
			bForcedAB);
		FNightRouteRuleRow UnusedRule;
		if (!Config->RouteRules->TryGetRule(LeftRoute, UnusedRule)
			|| !Config->RouteRules->TryGetRule(RightRoute, UnusedRule))
		{
			OutError = TEXT("RouteRules is missing one of the selected fork route rows.");
			return FailValidation();
		}
	}

	if (Config->RouteRules && !Config->RouteRules->ValidateRules(OutError))
	{
		return FailValidation();
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Validate] OK baseRoute=%d branchRoutes=%d forkIndex=%d."),
		BaseRouteLength,
		Config->CourseRuleData->BranchRoutes.Num(),
		ForkIndex);
	return true;
}

bool UNightCourseDirector::TryStartNight(
	const FNightBootstrap& Bootstrap,
	FString& OutError)
{
	OutError.Reset();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=TryStart] request Config='%s' running=%d Level=%d Seed=%d ForkPair=%d."),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		bRunning ? 1 : 0,
		static_cast<int32>(Bootstrap.LevelId),
		Bootstrap.Seed,
		static_cast<int32>(Bootstrap.ForkPair));
	if (bRunning)
	{
		OutError = TEXT("A NightCourse is already running.");
		UE_LOG(LogTemp, Warning, TEXT("[NightCourse][Stage=TryStart] rejected: %s"), *OutError);
		EmitDebugMessage(
			FString::Printf(TEXT("Start rejected: %s"), *OutError),
			false);
		return false;
	}

	StartNight(Bootstrap);
	if (bRunning)
	{
		UE_LOG(LogTemp, Display, TEXT("[NightCourse][Stage=TryStart] accepted; course entered running state."));
		return true;
	}

	OutError = LastFailureReason;
	if (OutError.IsEmpty())
	{
		OutError = TEXT("NightCourse did not enter a running state.");
		EmitDebugMessage(
			FString::Printf(TEXT("Start rejected: %s"), *OutError),
			true);
	}
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[NightCourse][Stage=TryStart] rejected Error='%s'."),
		OutError.IsEmpty() ? TEXT("<empty>") : *OutError);
	return false;
}

void UNightCourseDirector::StartNight(const FNightBootstrap& Bootstrap)
{
	if (bRunning)
	{
		const FString Message = TEXT("StartNight ignored: a course is already running.");
		UE_LOG(LogTemp, Warning, TEXT("[NightCourse] %s"), *Message);
		EmitDebugMessage(Message, false);
		return;
	}

	LastFailureReason.Reset();
	bHasResult = false;
	LastResult = FNightResult();
	bDidEnterRuntimeCourse = false;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Start] begin Config='%s' Level=%d Seed=%d ForkPair=%d."),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		static_cast<int32>(Bootstrap.LevelId),
		Bootstrap.Seed,
		static_cast<int32>(Bootstrap.ForkPair));
	if (!Config)
	{
		BeginFailure(TEXT("StartNight failed: Config is null."));
		return;
	}

	ClearSpawnedCourseActors();
	ActiveBootstrap = Bootstrap;
	ActiveForkPair = Bootstrap.ForkPair;
	AuthoredKeySwapCues = Config->KeySwapCues;
	for (const FNightLevelCourseRule& LevelRule : Config->LevelRules)
	{
		if (LevelRule.LevelId != Bootstrap.LevelId)
		{
			continue;
		}
		if (LevelRule.bUseForkPair)
		{
			ActiveForkPair = LevelRule.ForkPair;
		}
		if (LevelRule.bUseKeySwapCues)
		{
			AuthoredKeySwapCues = LevelRule.KeySwapCues;
		}
		break;
	}
	CurrentRoute = ENightRouteId::None;
	bBranchSelected = false;
	bSpareLampConsumed = false;
	bBranchTransitionConsumed = false;
	BranchBeatCount = 0;
	BaseBeatCount = 0;
	BranchTransitionBeatIndex = INDEX_NONE;
	NextKeySwapCueIndex = 0;
	BranchEnterBufferEndTime = 0.f;
	KeySwapEndTime = 0.f;
	ActiveKeySwapCues = AuthoredKeySwapCues;
	bHasActiveRouteRule = false;
	RuntimeSeed = Bootstrap.Seed;
	if (RuntimeSeed == 0)
	{
		if (Config->CourseRuleData && Config->CourseRuleData->bEnabled)
		{
			RuntimeSeed = Config->CourseRuleData->Seed;
		}
	}
	bHasRuntimeSeed = true;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Start] resolved runtimeSeed=%d activeForkPair=%d levelRules=%d."),
		RuntimeSeed,
		static_cast<int32>(ActiveForkPair),
		Config->LevelRules.Num());
	if (INightFeelBridge* Feel = GetFeel())
	{
		INightFeelBridge::Execute_SetControlScheme(
			FeelBridgeObject,
			ENightControlScheme::Normal);
	}

	FString ValidationError;
	if (!ValidateConfiguration(ValidationError))
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] StartNight configuration invalid: %s"), *ValidationError);
		BeginFailure(ValidationError);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("[NightCourse][Stage=Start] configuration validation passed."));

	ENightRouteId LeftRoute = ENightRouteId::None;
	ENightRouteId RightRoute = ENightRouteId::None;
	bool bForcedAB = false;
	UNightForkController::ResolvePairRoutes(
		ActiveForkPair,
		LeftRoute,
		RightRoute,
		bForcedAB);
	bForkPending = false;
	if (Config->bEnableFork
		&& Config->CourseRuleData->BranchRoutes.Num() > 0)
	{
		bForkPending = true;
	}

	bBuildingRuntimeCourse = true;
	FString BuildError;
	UE_LOG(LogTemp, Display, TEXT("[NightCourse][Stage=Start] composing runtime course."));
	if (!EnsureCourse(BuildError))
	{
		bBuildingRuntimeCourse = false;
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] StartNight build failed: %s"), *BuildError);
		BeginFailure(BuildError);
		return;
	}
	bBuildingRuntimeCourse = false;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Start] composition passed stones=%d beats=%d bridges=%d visuals=%d."),
		StoneSpecs.Num(),
		BeatSpecs.Num(),
		BridgeSpecs.Num(),
		VisualBindings.Num());

	bRunning = true;
	bDidEnterRuntimeCourse = true;
	ElapsedSeconds = 0.f;
	CurrentStoneIndex = 0;
	ActiveBeatIndex = INDEX_NONE;
	bWindowOpen = false;
	bAdvancing = false;
	CollectedIngredients.Reset();
	BranchCollectedIngredients.Reset();
	BeatConsumed.Init(0, BeatSpecs.Num());
	BaseBeatCount = BeatSpecs.Num();
	UE_LOG(LogTemp, Display, TEXT("[NightCourse][Stage=Start] spawning runtime actors."));
	SpawnCourseActors();

	ProgressDistance = StoneSpecs.IsValidIndex(0) ? StoneSpecs[0].TrackDistance : 0.f;
	SyncPawnToProgress(true);
	SetPhase(ENightCoursePhase::BaseSegment);
	if (IsRegistered())
	{
		SetComponentTickEnabled(true);
	}

	if (BeatSpecs.Num() > 0)
	{
		TryOpenBeat(0);
	}
	else
	{
		OpenNextBeatOrExit();
	}

	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] StartNight stones=%d beats=%d (stone-chain)"),
			StoneSpecs.Num(), BeatSpecs.Num());
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Start] complete running=%d currentStone=%d activeBeat=%d."),
		bRunning ? 1 : 0,
		CurrentStoneIndex,
		ActiveBeatIndex);
}

bool UNightCourseDirector::HasBranchQueueForRoute(ENightRouteId RouteId) const
{
	if (!Config)
	{
		return false;
	}
	if (Config->CourseRuleData && Config->CourseRuleData->bEnabled)
	{
		return Config->CourseRuleData->HasBranchRoute(RouteId);
	}
	return false;
}

void UNightCourseDirector::BeginForkChoice()
{
	if (!bRunning || !bForkPending || !Config)
	{
		OpenNextBeatOrExit();
		return;
	}

	ENightRouteId LeftRoute = ENightRouteId::None;
	ENightRouteId RightRoute = ENightRouteId::None;
	bool bForcedAB = false;
	UNightForkController::ResolvePairRoutes(
		ActiveForkPair,
		LeftRoute,
		RightRoute,
		bForcedAB);
	if (!HasBranchQueueForRoute(LeftRoute) || !HasBranchQueueForRoute(RightRoute))
	{
		BeginFailure(TEXT("Fork cannot start because one of the selected branch Atom queues is missing."));
		return;
	}

	if (!ForkController)
	{
		ForkController = NewObject<UNightForkController>(this);
		ForkController->OnForkResolved.AddDynamic(
			this,
			&UNightCourseDirector::HandleForkResolved);
	}
	bWindowOpen = false;
	ActiveBeatIndex = INDEX_NONE;
	SetPhase(ENightCoursePhase::ForkChoice);
	ForkController->BeginFork(
				ActiveForkPair,
		FMath::Max(0.01f, Config->ForkTimeoutSeconds),
		Config->bForkTimeoutPickLeft);
}

void UNightCourseDirector::HandleForkResolved(
	ENightRouteId RouteTaken,
	bool bTimedOut)
{
	if (!bRunning || Phase != ENightCoursePhase::ForkChoice)
	{
		return;
	}
	if (bTimedOut && GetDebug().bLogEvents)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[NightCourse] Fork timed out; selected route %d."),
			static_cast<int32>(RouteTaken));
	}

	CurrentRoute = RouteTaken;
	FString BuildError;
	if (!RebuildCourseForSelectedRoute(BuildError))
	{
		BeginFailure(BuildError);
	}
}

void UNightCourseDirector::ChooseForkLeft()
{
	if (!bRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NightCourse] ChooseForkLeft ignored: no course is running."));
		return;
	}
	if (Phase != ENightCoursePhase::ForkChoice)
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] ChooseForkLeft requires the ForkChoice phase (current=%d)."),
			static_cast<int32>(Phase));
		return;
	}
	if (ForkController)
	{
		ForkController->ChooseLeft();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] ChooseForkLeft failed: ForkController is missing."));
	}
}

void UNightCourseDirector::ChooseForkRight()
{
	if (!bRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NightCourse] ChooseForkRight ignored: no course is running."));
		return;
	}
	if (Phase != ENightCoursePhase::ForkChoice)
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] ChooseForkRight requires the ForkChoice phase (current=%d)."),
			static_cast<int32>(Phase));
		return;
	}
	if (ForkController)
	{
		ForkController->ChooseRight();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] ChooseForkRight failed: ForkController is missing."));
	}
}

void UNightCourseDirector::SkipFork()
{
	if (!bRunning || Phase != ENightCoursePhase::ForkChoice)
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] SkipFork requires an active ForkChoice phase."));
		return;
	}
	ChooseForkLeft();
}

void UNightCourseDirector::ForceKeySwap()
{
	if (!bRunning || !bBranchSelected)
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] ForceKeySwap requires an active branch course."));
		return;
	}
	if (!Config || !Config->bEnableKeySwap)
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] ForceKeySwap is disabled by Config."));
		return;
	}
	if (ActiveKeySwapCues.Num() == 0)
	{
		FNightKeySwapCue Cue;
		Cue.TriggerAfterBranchBeats = 0;
		Cue.WarningSeconds = Config ? Config->DefaultKeySwapWarningSeconds : 0.8f;
		Cue.SafetyHoldSeconds = Config ? Config->DefaultKeySwapSafetySeconds : 0.6f;
		Cue.TargetScheme = ENightControlScheme::Swapped;
		ActiveKeySwapCues.Add(Cue);
		NextKeySwapCueIndex = 0;
	}
	BeginKeySwapWarning();
}

float UNightCourseDirector::GetForkSecondsRemaining() const
{
	return ForkController && Phase == ENightCoursePhase::ForkChoice
		? ForkController->GetSecondsRemaining()
		: 0.f;
}

ENightRouteId UNightCourseDirector::GetForkLeftRoute() const
{
	return ForkController ? ForkController->GetLeftRoute() : ENightRouteId::None;
}

ENightRouteId UNightCourseDirector::GetForkRightRoute() const
{
	return ForkController ? ForkController->GetRightRoute() : ENightRouteId::None;
}

FString UNightCourseDirector::GetForkHintText() const
{
	if (!ActiveBootstrap.GiftBuffs.bGuideKite
		|| Phase != ENightCoursePhase::ForkChoice
		|| !ForkController
		|| !Config)
	{
		return FString();
	}

	auto GetBranchLength = [this](ENightRouteId RouteId)
	{
		if (Config->CourseRuleData && Config->CourseRuleData->bEnabled)
		{
			if (const FNightRuleAtomQueue* Queue =
				Config->CourseRuleData->BranchRoutes.Find(RouteId))
			{
				return Queue->TargetAtomCount > 0
					? Queue->TargetAtomCount
					: Queue->Atoms.Num();
			}
		}
		return 0;
	};

	auto FormatRoute = [this, &GetBranchLength](ENightRouteId RouteId)
	{
		FNightRouteRuleRow Rule;
		const bool bHasRule =
			Config->RouteRules && Config->RouteRules->TryGetRule(RouteId, Rule);
		return FString::Printf(
			TEXT("%d:len%d vis%d x%.2f"),
			static_cast<int32>(RouteId),
			GetBranchLength(RouteId),
			bHasRule ? FMath::Max(1, Rule.VisibleBlockCount) : 0,
			bHasRule ? Rule.SoulPenaltyScale : 0.f);
	};

	return FString::Printf(
		TEXT("GUIDE  L[%s]  R[%s]"),
		*FormatRoute(GetForkLeftRoute()),
		*FormatRoute(GetForkRightRoute()));
}

bool UNightCourseDirector::RebuildCourseForSelectedRoute(FString& OutError)
{
	OutError.Reset();
	if (!bRunning || CurrentRoute == ENightRouteId::None)
	{
		OutError = TEXT("Cannot build a branch without an active route selection.");
		return false;
	}

	const int32 PreviousStoneIndex = CurrentStoneIndex;
	const float PreviousProgressDistance = ProgressDistance;
	const TArray<FNightStoneSpec> PreviousStones = StoneSpecs;

	TArray<FNightStoneSpec> NewStones;
	TArray<FNightBeatSpec> NewBeats;
	TArray<FNightBridgeSpec> NewBridges;
	TArray<FNightAtomVisualBinding> NewVisualBindings;
	if (!BuildCourseForPreview(
		NewStones,
		NewBeats,
		NewBridges,
		NewVisualBindings))
	{
		OutError = TEXT("Selected branch composition failed; no partial branch was installed.");
		return false;
	}
	if (NewBeats.Num() <= BaseBeatCount)
	{
		OutError = TEXT("Selected branch did not append a transition and branch beat segment.");
		return false;
	}

	FNightRouteRuleRow NewRouteRule;
	if (!Config->RouteRules)
	{
		OutError = TEXT("A RouteRules asset is required before a branch can start; no hard-coded route fallback is allowed.");
		return false;
	}
	if (!Config->RouteRules->TryGetRule(CurrentRoute, NewRouteRule))
	{
		OutError = FString::Printf(
			TEXT("RouteRules has no authored row for selected route %d."),
			static_cast<int32>(CurrentRoute));
		return false;
	}

	for (int32 Index = 0; Index < FMath::Min(PreviousStones.Num(), NewStones.Num()); ++Index)
	{
		// Preserve the shared base foe state when the selected route rebuilds
		// the course. The planner/proc composer must not reroll resolved actors.
		if (PreviousStones[Index].bHasFoe)
		{
			NewStones[Index].bHasFoe = true;
			NewStones[Index].FoeId = PreviousStones[Index].FoeId;
		}
		else
		{
			NewStones[Index].bHasFoe = false;
			NewStones[Index].FoeId = EFoeId::None;
		}
	}

	ClearSpawnedCourseActors();
	StoneSpecs = MoveTemp(NewStones);
	BeatSpecs = MoveTemp(NewBeats);
	BridgeSpecs = MoveTemp(NewBridges);
	VisualBindings = MoveTemp(NewVisualBindings);
	BeatConsumed.Init(0, BeatSpecs.Num());
	for (int32 Index = 0; Index < FMath::Min(BaseBeatCount, BeatConsumed.Num()); ++Index)
	{
		BeatConsumed[Index] = 1;
	}

	BranchTransitionBeatIndex = BaseBeatCount;
	bBranchHasExplicitTransitionBeat =
		BeatSpecs.IsValidIndex(BranchTransitionBeatIndex)
		&& BeatSpecs[BranchTransitionBeatIndex].FromStoneIndex == PreviousStoneIndex;
	bBranchTransitionConsumed = false;
	bBranchSelected = true;
	bForkPending = false;
	BranchBeatCount = 0;
	CurrentStoneIndex = FMath::Clamp(
		PreviousStoneIndex,
		0,
		FMath::Max(0, StoneSpecs.Num() - 1));
	ProgressDistance = PreviousProgressDistance;
	ActiveBeatIndex = INDEX_NONE;
	bWindowOpen = false;
	bAdvancing = false;

	ActiveRouteRule = NewRouteRule;
	bHasActiveRouteRule = true;

	const EIngredientId EnterDropId = ActiveRouteRule.EnterDropId != EIngredientId::None
		? ActiveRouteRule.EnterDropId
		: Config->DefaultDropId;
	if (ActiveRouteRule.EnterDropCount > 0)
	{
		AddDrop(EnterDropId, ActiveRouteRule.EnterDropCount);
	}

	ActiveKeySwapCues = AuthoredKeySwapCues;
	if (Config->bKeySwapOnlyOnRouteC && CurrentRoute != ENightRouteId::C)
	{
		ActiveKeySwapCues.Reset();
	}
	NextKeySwapCueIndex = 0;
	if (ActiveBootstrap.GiftBuffs.bKeyCoin && ActiveKeySwapCues.Num() > 0)
	{
		NextKeySwapCueIndex = 1;
	}

	SpawnCourseActors();
	SyncPawnToProgress(true);
	BranchEnterBufferEndTime =
		ElapsedSeconds + FMath::Max(0.f, Config->BranchEnterBufferSeconds);
	SetPhase(ENightCoursePhase::BranchEnterBuffer);
	UpdateRouteVisibility();
	return true;
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
	if (bBranchSelected
		&& (BranchTransitionBeatIndex == INDEX_NONE
			|| BeatIndex > BranchTransitionBeatIndex
			|| (!bBranchHasExplicitTransitionBeat && BeatIndex >= BranchTransitionBeatIndex)))
	{
		SetPhase(ENightCoursePhase::BranchSegment);
	}

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
			TargetStone.FoeId = Config->DefaultFoeId;
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
	if (Outcome == ENightJudgeOutcome::WrongButton)
	{
		HandleFailedInput(NodeIndex, Outcome);
		return;
	}
	ResolveBeat(NodeIndex, Outcome);
}

void UNightCourseDirector::HandleFailedInput(
	int32 BeatIndex,
	ENightJudgeOutcome Outcome)
{
	if (!BeatSpecs.IsValidIndex(BeatIndex) || !Config)
	{
		return;
	}

	const FNightBeatSpec& Beat = BeatSpecs[BeatIndex];
	const bool bBranchBeat =
		bBranchSelected
		&& (BranchTransitionBeatIndex == INDEX_NONE
			|| BeatIndex > BranchTransitionBeatIndex
			|| (!bBranchHasExplicitTransitionBeat && BeatIndex >= BranchTransitionBeatIndex));
	const bool bProtectedBySpareLamp =
		bBranchBeat
		&& ActiveBootstrap.GiftBuffs.bSpareLamp
		&& !bSpareLampConsumed;
	if (bProtectedBySpareLamp)
	{
		bSpareLampConsumed = true;
	}

	if (INightFeelBridge* Feel = GetFeel())
	{
		if (!bProtectedBySpareLamp)
		{
			const float BasePenalty = Config->WrongPenalty;
			const float RoutePenaltyScale = bHasActiveRouteRule
				? ActiveRouteRule.SoulPenaltyScale
				: 1.f;
			INightFeelBridge::Execute_ApplySoulPenalty(
				FeelBridgeObject,
				BasePenalty * RoutePenaltyScale,
				Outcome);
		}
		INightFeelBridge::Execute_PlayFailFeedback(
			FeelBridgeObject,
			Outcome,
			Beat.Action);
		if (INightFeelBridge::Execute_GetSoul(FeelBridgeObject) <= 0.f)
		{
			BeginFailure(TEXT("Soul reached zero after a wrong input."));
		}
	}
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
	const bool bBranchBeat =
		bBranchSelected
		&& (BranchTransitionBeatIndex == INDEX_NONE
			|| BeatIndex > BranchTransitionBeatIndex
			|| (!bBranchHasExplicitTransitionBeat && BeatIndex >= BranchTransitionBeatIndex));
	const bool bProtectedBySpareLamp =
		bBranchBeat
		&& Outcome != ENightJudgeOutcome::Success
		&& ActiveBootstrap.GiftBuffs.bSpareLamp
		&& !bSpareLampConsumed;
	if (bProtectedBySpareLamp)
	{
		bSpareLampConsumed = true;
	}
	if (bBranchBeat)
	{
		++BranchBeatCount;
	}

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
			if (!bProtectedBySpareLamp)
			{
				const float RoutePenaltyScale = bHasActiveRouteRule
					? ActiveRouteRule.SoulPenaltyScale
					: 1.f;
				INightFeelBridge::Execute_ApplySoulPenalty(
					FeelBridgeObject,
					Penalty * RoutePenaltyScale,
					Outcome);
			}
			INightFeelBridge::Execute_PlayFailFeedback(FeelBridgeObject, Outcome, Beat.Action);
		}
	}

	const bool bAttackBeat = (Beat.Action == ENightNodeKind::Enemy);
	if (Outcome == ENightJudgeOutcome::Success && bAttackBeat && StoneSpecs.IsValidIndex(Beat.ToStoneIndex))
	{
		EIngredientId DropId = StoneSpecs[Beat.ToStoneIndex].DropId;
		int32 DropCount = StoneSpecs[Beat.ToStoneIndex].DropCount;
		if (bBranchBeat && bHasActiveRouteRule)
		{
			if (!Config->bDropIngredientOnEveryEnemyKill)
			{
				const int32 Rhythm = FMath::Max(1, ActiveRouteRule.DropRhythmEveryN);
				if ((BranchBeatCount % Rhythm) != 0)
				{
					DropCount = 0;
				}
				if (ActiveRouteRule.DropCycle.Num() > 0)
				{
					DropId = ActiveRouteRule.DropCycle[
						(BranchBeatCount - 1) % ActiveRouteRule.DropCycle.Num()];
				}
			}
			DropCount *= FMath::Max(1, ActiveRouteRule.BranchDropCountMul);
		}
		AddDrop(DropId, DropCount);
		if (SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
		{
			SpawnedStones[Beat.ToStoneIndex]->ClearFoe(true);
			SpawnedStones[Beat.ToStoneIndex]->PlayDropBurst(
				DropId,
				DropCount);
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

	if (INightFeelBridge* Feel = GetFeel())
	{
		if (INightFeelBridge::Execute_GetSoul(FeelBridgeObject) <= 0.f)
		{
			BeginFailure(TEXT("Soul reached zero during the course."));
			return;
		}
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
	else
	{
		bAdvancing = false;
		ProgressDistance = AdvanceTargetDistance;
		OpenNextBeatOrExit();
	}
}

void UNightCourseDirector::OnAdvanceArrived()
{
	bAdvancing = false;
	ProgressDistance = AdvanceTargetDistance;
	SyncPawnToProgress(true);
	OpenNextBeatOrExit();
}

bool UNightCourseDirector::HasPendingKeySwap() const
{
	if (!Config
		|| !Config->bEnableKeySwap
		|| !bBranchSelected
		|| !ActiveKeySwapCues.IsValidIndex(NextKeySwapCueIndex))
	{
		return false;
	}
	return BranchBeatCount >= ActiveKeySwapCues[NextKeySwapCueIndex].TriggerAfterBranchBeats;
}

void UNightCourseDirector::ApplyKeySwapCue(const FNightKeySwapCue& Cue)
{
	if (INightFeelBridge* Feel = GetFeel())
	{
		ENightControlScheme Scheme = Cue.TargetScheme;
		if (Cue.bToggle)
		{
			const ENightControlScheme CurrentScheme =
				INightFeelBridge::Execute_GetControlScheme(FeelBridgeObject);
			Scheme = CurrentScheme == ENightControlScheme::Normal
				? ENightControlScheme::Swapped
				: ENightControlScheme::Normal;
		}
		INightFeelBridge::Execute_SetControlScheme(
			FeelBridgeObject,
			Scheme);
	}
}

void UNightCourseDirector::BeginKeySwapWarning()
{
	if (!HasPendingKeySwap())
	{
		return;
	}

	const FNightKeySwapCue Cue = ActiveKeySwapCues[NextKeySwapCueIndex];
	++NextKeySwapCueIndex;
	KeySwapEndTime = ElapsedSeconds + FMath::Max(0.f, Cue.WarningSeconds);
	SetPhase(ENightCoursePhase::KeySwapWarning);
	if (Cue.WarningSeconds <= 0.f)
	{
		ApplyKeySwapCue(Cue);
		KeySwapEndTime = ElapsedSeconds + FMath::Max(0.f, Cue.SafetyHoldSeconds);
		SetPhase(ENightCoursePhase::KeySwapSafetyHold);
	}
}

float UNightCourseDirector::GetKeySwapSecondsRemaining() const
{
	return IsKeySwapWarningActive()
		? FMath::Max(0.f, KeySwapEndTime - ElapsedSeconds)
		: 0.f;
}

void UNightCourseDirector::OpenNextBeatOrExit()
{
	if (Phase == ENightCoursePhase::KeySwapWarning
		|| Phase == ENightCoursePhase::KeySwapSafetyHold
		|| Phase == ENightCoursePhase::ForkChoice)
	{
		return;
	}
	if (HasPendingKeySwap())
	{
		BeginKeySwapWarning();
		return;
	}

	for (int32 Index = 0; Index < BeatSpecs.Num(); ++Index)
	{
		if (!BeatConsumed[Index])
		{
			TryOpenBeat(Index);
			return;
		}
	}

	if (bForkPending)
	{
		BeginForkChoice();
		return;
	}

	ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
	SetPhase(ENightCoursePhase::ExitBuffer);
}

void UNightCourseDirector::UpdateRouteVisibility()
{
	if (!bHasActiveRouteRule)
	{
		return;
	}

	const int32 VisibleCount = FMath::Max(1, ActiveRouteRule.VisibleBlockCount);
	const int32 LastVisibleStone = CurrentStoneIndex + VisibleCount;
	for (int32 StoneIndex = 0; StoneIndex < SpawnedStones.Num(); ++StoneIndex)
	{
		if (SpawnedStones[StoneIndex])
		{
			const bool bVisible = StoneIndex <= LastVisibleStone;
			SpawnedStones[StoneIndex]->SetActorHiddenInGame(!bVisible);
			SpawnedStones[StoneIndex]->SetActorEnableCollision(bVisible);
		}
	}
	for (int32 BindingIndex = 0; BindingIndex < VisualBindings.Num(); ++BindingIndex)
	{
		const FNightAtomVisualBinding& Binding = VisualBindings[BindingIndex];
		if (!SpawnedVisualActors.IsValidIndex(BindingIndex))
		{
			continue;
		}
		AActor* VisualActor = SpawnedVisualActors[BindingIndex];
		if (VisualActor)
		{
			const bool bVisible = Binding.bIsBridge
				? (BridgeSpecs.IsValidIndex(Binding.BridgeIndex)
					&& BridgeSpecs[Binding.BridgeIndex].ToStoneIndex <= LastVisibleStone)
				: (Binding.StoneIndex >= 0 && Binding.StoneIndex <= LastVisibleStone);
			VisualActor->SetActorHiddenInGame(!bVisible);
			VisualActor->SetActorEnableCollision(bVisible);
		}
	}
	for (int32 BridgeIndex = 0; BridgeIndex < BridgeSpecs.Num(); ++BridgeIndex)
	{
		if (SpawnedBridges.IsValidIndex(BridgeIndex) && SpawnedBridges[BridgeIndex])
		{
			const FNightBridgeSpec& Bridge = BridgeSpecs[BridgeIndex];
			const bool bVisible = Bridge.ToStoneIndex <= LastVisibleStone;
			SpawnedBridges[BridgeIndex]->SetActorHiddenInGame(!bVisible);
			SpawnedBridges[BridgeIndex]->SetActorEnableCollision(bVisible);
		}
	}
}

void UNightCourseDirector::UpdateRouteEffects(float DeltaTime)
{
	if (!bHasActiveRouteRule
		|| ActiveRouteRule.DotSoulPerSecond <= 0.f
		|| !bBranchSelected
		|| !bRunning)
	{
		return;
	}

	const bool bRouteEffectPhase =
		Phase == ENightCoursePhase::BranchSegment
		|| Phase == ENightCoursePhase::KeySwapWarning
		|| Phase == ENightCoursePhase::KeySwapSafetyHold;
	if (!bRouteEffectPhase)
	{
		return;
	}

	const bool bTickWhileAdvancing =
		ActiveRouteRule.bReverseFire || !bAdvancing;
	if (!bTickWhileAdvancing)
	{
		return;
	}

	if (INightFeelBridge* Feel = GetFeel())
	{
		const float DrainScale = Config
			? FMath::Max(0.f, Config->ForkSoulDrainScale)
			: 1.f;
		INightFeelBridge::Execute_ApplySoulPenalty(
			FeelBridgeObject,
			ActiveRouteRule.DotSoulPerSecond
				* DrainScale
				* FMath::Max(0.f, DeltaTime),
			ENightJudgeOutcome::Miss);
		if (INightFeelBridge::Execute_GetSoul(FeelBridgeObject) <= 0.f)
		{
			BeginFailure(TEXT("Soul reached zero from the active route effect."));
		}
	}
}

void UNightCourseDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (IsRegistered())
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}
	if (!bRunning || !Config)
	{
		return;
	}

	ElapsedSeconds += DeltaTime;
	OnDebugTick.Broadcast(ElapsedSeconds);
	UpdateRouteEffects(DeltaTime);
	if (!bRunning)
	{
		return;
	}
	if (INightFeelBridge* Feel = GetFeel())
	{
		if (INightFeelBridge::Execute_GetSoul(FeelBridgeObject) <= 0.f)
		{
			BeginFailure(TEXT("Soul reached zero during idle/breathing."));
			return;
		}
	}
	UpdateRouteVisibility();

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

	if (Phase == ENightCoursePhase::ForkChoice)
	{
		if (ForkController)
		{
			ForkController->TickFork(DeltaTime);
		}
		return;
	}

	if (Phase == ENightCoursePhase::BranchEnterBuffer
		&& ElapsedSeconds >= BranchEnterBufferEndTime)
	{
		if (BeatSpecs.IsValidIndex(BranchTransitionBeatIndex))
		{
			bBranchTransitionConsumed = true;
			if (bBranchHasExplicitTransitionBeat)
			{
				BeatConsumed[BranchTransitionBeatIndex] = 1;
				BeginAdvanceToStone(BeatSpecs[BranchTransitionBeatIndex].ToStoneIndex);
			}
			else
			{
				BeginAdvanceToStone(BeatSpecs[BranchTransitionBeatIndex].FromStoneIndex);
			}
		}
		else
		{
			BeginFailure(TEXT("Branch transition beat is missing after branch composition."));
		}
		return;
	}

	if (Phase == ENightCoursePhase::KeySwapWarning
		&& ElapsedSeconds >= KeySwapEndTime)
	{
		const int32 AppliedCueIndex = FMath::Max(0, NextKeySwapCueIndex - 1);
		if (ActiveKeySwapCues.IsValidIndex(AppliedCueIndex))
		{
			const FNightKeySwapCue& Cue = ActiveKeySwapCues[AppliedCueIndex];
			ApplyKeySwapCue(Cue);
			KeySwapEndTime = ElapsedSeconds + FMath::Max(0.f, Cue.SafetyHoldSeconds);
			SetPhase(ENightCoursePhase::KeySwapSafetyHold);
		}
		return;
	}

	if (Phase == ENightCoursePhase::KeySwapSafetyHold
		&& ElapsedSeconds >= KeySwapEndTime)
	{
		SetPhase(ENightCoursePhase::BranchSegment);
		OpenNextBeatOrExit();
		return;
	}

	if (Phase == ENightCoursePhase::ExitBuffer && ElapsedSeconds >= ExitBufferEndTime)
	{
		FNightResult Result;
		Result.bSuccess = true;
		Result.bFailedMidway = false;
		Result.RouteTaken = CurrentRoute;
		Result.Ingredients = CollectedIngredients;
		if (bHasActiveRouteRule && ActiveRouteRule.CarryOutBonus > 0.f)
		{
			for (const FIngredientStack& Stack : BranchCollectedIngredients)
			{
				AddDropToArray(
					Result.Ingredients,
					Stack.Id,
					FMath::CeilToInt(
						static_cast<float>(Stack.Count)
						* ActiveRouteRule.CarryOutBonus));
			}
		}
		if (ActiveBootstrap.GiftBuffs.bTaotieBox
			&& ActiveBootstrap.GiftBuffs.TaotieLockIngredient != EIngredientId::None)
		{
			bool bHasLockIngredient = false;
			for (const FIngredientStack& Stack : Result.Ingredients)
			{
				if (Stack.Id == ActiveBootstrap.GiftBuffs.TaotieLockIngredient
					&& Stack.Count > 0)
				{
					bHasLockIngredient = true;
					break;
				}
			}
			if (!bHasLockIngredient)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[NightCourse] TaotieBox lock ingredient was not collected; suppressing carry-out drops."));
				Result.Ingredients.Reset();
			}
		}
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
	AddDropToArray(CollectedIngredients, Id, Count);
	if (bBranchSelected)
	{
		AddDropToArray(BranchCollectedIngredients, Id, Count);
	}
}

void UNightCourseDirector::AddDropToArray(
	TArray<FIngredientStack>& Target,
	EIngredientId Id,
	int32 Count) const
{
	if (Id == EIngredientId::None || Count <= 0)
	{
		return;
	}
	for (FIngredientStack& Stack : Target)
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
	Target.Add(NewStack);
}

void UNightCourseDirector::FinishNight(const FNightResult& Result)
{
	if (IsRegistered())
	{
		SetComponentTickEnabled(false);
	}
	if (INightFeelBridge* Feel = GetFeel())
	{
		if (ActiveBeatIndex != INDEX_NONE)
		{
			INightFeelBridge::Execute_ClearJudgeRequest(
				FeelBridgeObject,
				ActiveBeatIndex);
		}
	}
	if (ForkController)
	{
		ForkController->CancelFork();
	}
	ClearSpawnedCourseActors();
	bRunning = false;
	bAdvancing = false;
	bWindowOpen = false;
	ActiveBeatIndex = INDEX_NONE;
	bHasResult = true;
	LastResult = Result;
	if (Result.bSuccess && !Result.bFailedMidway)
	{
		LastFailureReason.Reset();
	}
	else if (LastFailureReason.IsEmpty())
	{
		LastFailureReason = TEXT("NightCourse finished unsuccessfully.");
	}
	SetPhase(
		Result.bSuccess && !Result.bFailedMidway
			? ENightCoursePhase::Finished
			: ENightCoursePhase::Failed);
	OnFinished.Broadcast(Result);
	EmitDebugMessage(
		FString::Printf(
			TEXT("NightCourse result: success=%d failedMidway=%d route=%d drops=%d soul=%.1f"),
			Result.bSuccess ? 1 : 0,
			Result.bFailedMidway ? 1 : 0,
			static_cast<int32>(Result.RouteTaken),
			Result.Ingredients.Num(),
			Result.SoulLeft),
		!Result.bSuccess || Result.bFailedMidway);
	if (GetDebug().bLogEvents)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightCourse] Finished success=%d ingredients=%d soul=%.1f"),
			Result.bSuccess ? 1 : 0, Result.Ingredients.Num(), Result.SoulLeft);
	}
}

void UNightCourseDirector::BeginFailure(const FString& Reason)
{
	if (Phase == ENightCoursePhase::Failed)
	{
		return;
	}
	LastFailureReason = Reason.IsEmpty()
		? TEXT("NightCourse failed without a reason.")
		: Reason;
	UE_LOG(LogTemp, Error, TEXT("[NightCourse] Failed: %s"), *LastFailureReason);
	EmitDebugMessage(
		FString::Printf(TEXT("NightCourse failure: %s"), *LastFailureReason),
		true);
	FNightResult Result;
	Result.bSuccess = false;
	Result.bFailedMidway = true;
	Result.RouteTaken = CurrentRoute;
	Result.Ingredients = CollectedIngredients;
	Result.SoulLeft = Config ? Config->StartingSoul : 0.f;
	if (INightFeelBridge* Feel = GetFeel())
	{
		Result.SoulLeft = INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
	}
	FinishNight(Result);
}

void UNightCourseDirector::ResetCourse()
{
	if (INightFeelBridge* Feel = GetFeel())
	{
		if (ActiveBeatIndex != INDEX_NONE)
		{
			INightFeelBridge::Execute_ClearJudgeRequest(
				FeelBridgeObject,
				ActiveBeatIndex);
		}
	}
	if (ForkController)
	{
		ForkController->CancelFork();
	}
	ClearSpawnedCourseActors();
	if (IsRegistered())
	{
		SetComponentTickEnabled(false);
	}
	bRunning = false;
	bDidEnterRuntimeCourse = false;
	bWindowOpen = false;
	bAdvancing = false;
	SetPhase(ENightCoursePhase::Idle);
	ElapsedSeconds = 0.f;
	ProgressDistance = 0.f;
	CurrentStoneIndex = 0;
	ActiveBeatIndex = INDEX_NONE;
	ActiveBootstrap = FNightBootstrap();
	CurrentRoute = ENightRouteId::None;
	ActiveForkPair = ENightForkPair::AB;
	RuntimeSeed = 0;
	bHasRuntimeSeed = false;
	bBuildingRuntimeCourse = false;
	bForkPending = false;
	bBranchSelected = false;
	bHasActiveRouteRule = false;
	BranchBeatCount = 0;
	BeatConsumed.Reset();
	CollectedIngredients.Reset();
	BranchCollectedIngredients.Reset();
	AuthoredKeySwapCues.Reset();
	ActiveKeySwapCues.Reset();
	bHasResult = false;
	LastResult = FNightResult();
	LastFailureReason.Reset();
}

void UNightCourseDirector::DebugForceFinish(bool bSuccess)
{
	if (!bRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NightCourse] DebugForceFinish ignored: no course is running."));
		return;
	}
	FNightResult Result;
	Result.bSuccess = bSuccess;
	Result.bFailedMidway = !bSuccess;
	Result.RouteTaken = CurrentRoute;
	Result.Ingredients = CollectedIngredients;
	if (!bSuccess)
	{
		LastFailureReason = TEXT("DebugForceFinish(false).");
	}
	if (INightFeelBridge* Feel = GetFeel())
	{
		Result.SoulLeft = INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
	}
	FinishNight(Result);
}

void UNightCourseDirector::DebugSkipToExit()
{
	if (!bRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NightCourse] DebugSkipToExit ignored: no course is running."));
		return;
	}
	for (int32 Index = 0; Index < BeatConsumed.Num(); ++Index)
	{
		BeatConsumed[Index] = 1;
	}
	bForkPending = false;
	bWindowOpen = false;
	bAdvancing = false;
	ActiveBeatIndex = INDEX_NONE;
	ExitBufferEndTime = ElapsedSeconds + (Config ? Config->ExitBufferSeconds : 1.f);
	SetPhase(ENightCoursePhase::ExitBuffer);
}
#pragma endregion K2 moonyfli
