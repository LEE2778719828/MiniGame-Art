#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightCourseForkAtomActor.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightCourseAtomRouteData.h"
#include "Night/Course/NightCourseRuleData.h"
#include "Night/Course/NightForkController.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightCourseRoadsideActor.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightCourseHUD.h" //add by K2
#include "GameFramework/PlayerController.h" //add by K2
#include "DrawDebugHelpers.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h" //add by K2
#include "Components/StaticMeshComponent.h" //add by K2
#include "Math/RotationMatrix.h"
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

bool UNightCourseDirector::IsForkEnabledForActiveCourse() const
{
	return ActiveBootstrap.bUseCourseQueueOverride
		? ActiveBootstrap.bEnableForkOverride
		: (Config && Config->bEnableFork);
}

int32 UNightCourseDirector::ResolveMainRouteAtomCount(const int32 AuthoredCount) const
{
	return ActiveBootstrap.bUseCourseQueueOverride
		&& ActiveBootstrap.MainRouteAtomCountOverride > 0
		? ActiveBootstrap.MainRouteAtomCountOverride
		: AuthoredCount;
}

int32 UNightCourseDirector::ResolveForkRouteAtomCount(const int32 AuthoredCount) const
{
	return ActiveBootstrap.bUseCourseQueueOverride
		&& ActiveBootstrap.bEnableForkOverride
		&& ActiveBootstrap.ForkRouteAtomCountOverride > 0
		? ActiveBootstrap.ForkRouteAtomCountOverride
		: AuthoredCount;
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
	return Origin + CourseWorldOffset + FVector(Distance, 0.f, 0.f);
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

APostProcessVolume* UNightCourseDirector::ResolveCoursePostProcessVolume()
{
	if (ManagedPostProcessVolume.IsValid())
	{
		return ManagedPostProcessVolume.Get();
	}
	UWorld* World = GetWorld();
	if (!World || !Config)
	{
		return nullptr;
	}

	APostProcessVolume* FirstUnboundVolume = nullptr;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		APostProcessVolume* Volume = *It;
		if (!Volume)
		{
			continue;
		}
		if (!Config->PostProcessVolumeTag.IsNone()
			&& Volume->ActorHasTag(Config->PostProcessVolumeTag))
		{
			ManagedPostProcessVolume = Volume;
			return Volume;
		}
		if (Config->PostProcessVolumeTag.IsNone()
			&& !FirstUnboundVolume
			&& Volume->bUnbound)
		{
			FirstUnboundVolume = Volume;
		}
	}
	ManagedPostProcessVolume = FirstUnboundVolume;
	return FirstUnboundVolume;
}

void UNightCourseDirector::ApplyCoursePostProcessMaterial(UMaterialInterface* Material)
{
	if (!Config)
	{
		return;
	}
	APostProcessVolume* Volume = ResolveCoursePostProcessVolume();
	if (!Volume)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NightCourse][PostProcess] No matching PostProcessVolume; material switch skipped."));
		return;
	}

	TSet<const UObject*> ManagedMaterials;
	if (Config->DefaultPostProcessMaterial)
	{
		ManagedMaterials.Add(Config->DefaultPostProcessMaterial);
	}
	if (Config->RouteRules)
	{
		for (const FNightRouteRuleRow& Row : Config->RouteRules->Rows)
		{
			if (Row.PostProcessMaterial)
			{
				ManagedMaterials.Add(Row.PostProcessMaterial);
			}
		}
	}
	if (ActiveCoursePostProcessMaterial)
	{
		ManagedMaterials.Add(ActiveCoursePostProcessMaterial);
	}

	Volume->Settings.WeightedBlendables.Array.RemoveAll(
		[&ManagedMaterials](const FWeightedBlendable& Blendable)
		{
			return Blendable.Object && ManagedMaterials.Contains(Blendable.Object);
		});
	if (Material && Config->PostProcessMaterialWeight > 0.f)
	{
		Volume->Settings.AddBlendable(Material, Config->PostProcessMaterialWeight);
	}
	ActiveCoursePostProcessMaterial = Material;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][PostProcess] Applied '%s' to '%s' weight=%.2f."),
		*GetNameSafe(Material),
		*GetNameSafe(Volume),
		Config->PostProcessMaterialWeight);
}

void UNightCourseDirector::ApplyDefaultCoursePostProcessMaterial()
{
	ApplyCoursePostProcessMaterial(
		Config ? Config->DefaultPostProcessMaterial.Get() : nullptr);
}
bool UNightCourseDirector::EnsureCourse(FString& OutError)
{
	OutError.Reset();
	StoneSpecs.Reset();
	BeatSpecs.Reset();
	BridgeSpecs.Reset();
	VisualBindings.Reset();
	ForkAtomSpecs.Reset();
	RoadsideSpecs.Reset();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Compose] EnsureCourse begin Config='%s' runtimeContext=%d."),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		bBuildingRuntimeCourse || bRunning ? 1 : 0);
	if (!BuildCourseForPreview(
		StoneSpecs,
		BeatSpecs,
		BridgeSpecs,
		VisualBindings,
		ForkAtomSpecs))
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
		ForkAtomSpecs.Reset();
		return false;
	}
	if (StoneSpecs.Num() <= 0)
	{
		OutError = TEXT("Course composition produced zero stones.");
		UE_LOG(LogTemp, Error, TEXT("[NightCourse][Stage=Compose] EnsureCourse failed: %s."), *OutError);
		return false;
	}
	if (!BuildRoadsideSpecs(RoadsideSpecs))
	{
		OutError = TEXT("Roadside decoration composition failed; see the first roadside error.");
		UE_LOG(LogTemp, Error, TEXT("[NightCourse][Stage=Compose] EnsureCourse failed: %s."), *OutError);
		StoneSpecs.Reset();
		BeatSpecs.Reset();
		BridgeSpecs.Reset();
		VisualBindings.Reset();
		ForkAtomSpecs.Reset();
		RoadsideSpecs.Reset();
		return false;
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Compose] EnsureCourse complete stones=%d beats=%d bridges=%d visualBindings=%d forkAtoms=%d roadside=%d."),
		StoneSpecs.Num(),
		BeatSpecs.Num(),
		BridgeSpecs.Num(),
		VisualBindings.Num(),
		ForkAtomSpecs.Num(),
		RoadsideSpecs.Num());
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
		const FVector& TrackOrigin)
	{
		return WorldLocation.X - TrackOrigin.X;
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

namespace NightCourseRoadside_Private
{
	struct FPathNode
	{
		FVector Location = FVector::ZeroVector;
		int32 StoneIndex = INDEX_NONE;
	};

	struct FPath
	{
		TArray<FPathNode> Nodes;
		TArray<float> CumulativeDistances;
		float Length = 0.f;
	};

	struct FPathSample
	{
		FVector Location = FVector::ZeroVector;
		FVector Tangent = FVector::ForwardVector;
		FVector Right = FVector::RightVector;
		int32 SegmentIndex = INDEX_NONE;
		int32 FromStoneIndex = INDEX_NONE;
		int32 ToStoneIndex = INDEX_NONE;
	};

	struct FResolvedEntry
	{
		UClass* Class = nullptr;
		float Weight = 0.f;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Span = 0.f;
	};

	static FVector ResolveStoneLocation(
		const UNightG1CourseConfig* Config,
		const TArray<FNightStoneSpec>& Stones,
		const int32 StoneIndex)
	{
		if (!Stones.IsValidIndex(StoneIndex))
		{
			return FVector::ZeroVector;
		}
		const FNightStoneSpec& Stone = Stones[StoneIndex];
		if (Stone.bUseWorldPose)
		{
			return Stone.WorldLocation;
		}
		return (Config ? Config->TrackOrigin : FVector::ZeroVector)
			+ FVector(Stone.TrackDistance, 0.f, 0.f);
	}

	static bool BuildPath(
		const UNightG1CourseConfig* Config,
		const TArray<FNightStoneSpec>& Stones,
		FPath& OutPath)
	{
		OutPath = FPath();
		for (int32 StoneIndex = 0; StoneIndex < Stones.Num(); ++StoneIndex)
		{
			const FVector Location = ResolveStoneLocation(Config, Stones, StoneIndex);
			if (OutPath.Nodes.Num() > 0
				&& FVector::DistSquared(OutPath.Nodes.Last().Location, Location)
					<= FMath::Square(KINDA_SMALL_NUMBER))
			{
				continue;
			}

			FPathNode Node;
			Node.Location = Location;
			Node.StoneIndex = StoneIndex;
			OutPath.Nodes.Add(Node);
			if (OutPath.Nodes.Num() == 1)
			{
				OutPath.CumulativeDistances.Add(0.f);
				continue;
			}

			const float SegmentLength = FVector::Distance(
				OutPath.Nodes[OutPath.Nodes.Num() - 2].Location,
				Location);
			OutPath.Length += SegmentLength;
			OutPath.CumulativeDistances.Add(OutPath.Length);
		}
		return OutPath.Nodes.Num() >= 2 && OutPath.Length > KINDA_SMALL_NUMBER;
	}

	static const FNightBridgeSpec* FindBridge(
		const TArray<FNightBridgeSpec>& Bridges,
		const int32 FromStoneIndex,
		const int32 ToStoneIndex)
	{
		for (const FNightBridgeSpec& Bridge : Bridges)
		{
			if (Bridge.FromStoneIndex == FromStoneIndex
				&& Bridge.ToStoneIndex == ToStoneIndex)
			{
				return &Bridge;
			}
		}
		return nullptr;
	}

	static FPathSample SamplePath(
		const FPath& Path,
		const TArray<FNightBridgeSpec>& Bridges,
		const FVector& FallbackForward,
		const float Distance)
	{
		FPathSample Sample;
		if (Path.Nodes.Num() == 0)
		{
			return Sample;
		}
		if (Path.Nodes.Num() == 1 || Path.Length <= KINDA_SMALL_NUMBER)
		{
			Sample.Location = Path.Nodes[0].Location;
			Sample.Tangent = FallbackForward.GetSafeNormal2D();
			if (Sample.Tangent.IsNearlyZero())
			{
				Sample.Tangent = FVector::ForwardVector;
			}
			Sample.Right = FVector::CrossProduct(
				FVector::UpVector,
				Sample.Tangent).GetSafeNormal();
			return Sample;
		}

		const float ClampedDistance = FMath::Clamp(Distance, 0.f, Path.Length);
		int32 SegmentIndex = Path.Nodes.Num() - 2;
		for (int32 Index = 0; Index < Path.Nodes.Num() - 1; ++Index)
		{
			if (ClampedDistance <= Path.CumulativeDistances[Index + 1])
			{
				SegmentIndex = Index;
				break;
			}
		}

		const FVector A = Path.Nodes[SegmentIndex].Location;
		const FVector B = Path.Nodes[SegmentIndex + 1].Location;
		const FVector RawDelta = B - A;
		const float SegmentLength = RawDelta.Size();
		const FVector RawTangent = SegmentLength > KINDA_SMALL_NUMBER
			? RawDelta / SegmentLength
			: FallbackForward.GetSafeNormal2D();
		const float SegmentStart = Path.CumulativeDistances[SegmentIndex];
		const float Alpha = SegmentLength > KINDA_SMALL_NUMBER
			? FMath::Clamp(
				(ClampedDistance - SegmentStart) / SegmentLength,
				0.f,
				1.f)
			: 0.f;

		Sample.Location = FMath::Lerp(A, B, Alpha);
		Sample.Tangent = RawTangent.GetSafeNormal2D();
		if (const FNightBridgeSpec* Bridge = FindBridge(
			Bridges,
			Path.Nodes[SegmentIndex].StoneIndex,
			Path.Nodes[SegmentIndex + 1].StoneIndex))
		{
			const FVector BridgeTangent =
				FRotator(0.f, Bridge->YawDeg, 0.f).Vector().GetSafeNormal2D();
			if (!BridgeTangent.IsNearlyZero())
			{
				Sample.Tangent = FVector::DotProduct(BridgeTangent, RawTangent) < 0.f
					? -BridgeTangent
					: BridgeTangent;
			}
		}
		if (Sample.Tangent.IsNearlyZero())
		{
			Sample.Tangent = FVector::ForwardVector;
		}
		Sample.Right = FVector::CrossProduct(
			FVector::UpVector,
			Sample.Tangent).GetSafeNormal();
		if (Sample.Right.IsNearlyZero())
		{
			Sample.Right = FVector::RightVector;
		}
		Sample.SegmentIndex = SegmentIndex;
		Sample.FromStoneIndex = Path.Nodes[SegmentIndex].StoneIndex;
		Sample.ToStoneIndex = Path.Nodes[SegmentIndex + 1].StoneIndex;
		return Sample;
	}

	static bool ResolveEntries(
		const FNightRoadsideGenerationSettings& Settings,
		const TCHAR* Label,
		TArray<FResolvedEntry>& OutEntries,
		FString& OutError)
	{
		OutEntries.Reset();
		OutError.Reset();
		if (!Settings.bEnabled || Settings.BlueprintPool.Num() == 0)
		{
			return true;
		}

		for (int32 EntryIndex = 0; EntryIndex < Settings.BlueprintPool.Num(); ++EntryIndex)
		{
			const FNightRoadsideBlueprintEntry& Entry =
				Settings.BlueprintPool[EntryIndex];
			if (Entry.Weight <= 0.f)
			{
				continue;
			}

			UClass* PropClass = Entry.Blueprint.LoadSynchronous();
			if (!PropClass
				|| !PropClass->IsChildOf(ANightRoadsideSegmentActor::StaticClass()))
			{
				OutError = FString::Printf(
					TEXT("%s roadside Blueprint entry %d is not a valid ANightRoadsideSegmentActor class."),
					Label,
					EntryIndex);
				return false;
			}

			const ANightRoadsideSegmentActor* Defaults =
				PropClass->GetDefaultObject<ANightRoadsideSegmentActor>();
			FResolvedEntry Resolved;
			Resolved.Class = PropClass;
			Resolved.Weight = Entry.Weight;
			if (!Defaults
				|| !Defaults->GetRoadsideMarkerLocations(
					Resolved.Start,
					Resolved.End))
			{
				OutError = FString::Printf(
					TEXT("%s roadside Blueprint '%s' has no StartMarker/EndMarker pair."),
					Label,
					*GetNameSafe(PropClass));
				return false;
			}
			Resolved.Span = FVector::Distance(Resolved.Start, Resolved.End);
			if (Resolved.Span <= KINDA_SMALL_NUMBER)
			{
				OutError = FString::Printf(
					TEXT("%s roadside Blueprint '%s' has a zero-length marker span."),
					Label,
					*GetNameSafe(PropClass));
				return false;
			}
			OutEntries.Add(Resolved);
		}
		return true;
	}

	static const FResolvedEntry* PickEntry(
		FRandomStream& Rng,
		const TArray<FResolvedEntry>& Entries)
	{
		float TotalWeight = 0.f;
		for (const FResolvedEntry& Entry : Entries)
		{
			TotalWeight += FMath::Max(0.f, Entry.Weight);
		}
		if (TotalWeight <= 0.f)
		{
			return nullptr;
		}

		const float Pick = Rng.FRandRange(0.f, TotalWeight);
		float Accumulated = 0.f;
		for (const FResolvedEntry& Entry : Entries)
		{
			Accumulated += FMath::Max(0.f, Entry.Weight);
			if (Pick <= Accumulated)
			{
				return &Entry;
			}
		}
		return &Entries.Last();
	}

	static int32 MakeSeed(
		const int32 BaseSeed,
		const ENightRoadsideKind Kind,
		const int32 Side,
		const int32 SeedOffset)
	{
		uint32 Hash = static_cast<uint32>(BaseSeed);
		Hash = Hash * 16777619u ^ static_cast<uint32>(SeedOffset);
		Hash = Hash * 16777619u ^ static_cast<uint32>(Kind);
		Hash = Hash * 16777619u ^ static_cast<uint32>(Side + 2);
		return static_cast<int32>(Hash & 0x7fffffffu);
	}

	static bool AppendCategory(
		const UNightG1CourseConfig* Config,
		const FNightRoadsideGenerationSettings& Settings,
		const ENightRoadsideKind Kind,
		const TCHAR* Label,
		const int32 BaseSeed,
		const FPath& Path,
		const TArray<FNightBridgeSpec>& Bridges,
		TArray<FNightRoadsidePropSpec>& OutSpecs,
		FString& OutError)
	{
		TArray<FResolvedEntry> Entries;
		if (!ResolveEntries(Settings, Label, Entries, OutError))
		{
			return false;
		}
		if (Entries.Num() == 0 || Path.Length <= KINDA_SMALL_NUMBER)
		{
			return true;
		}

		const FVector FallbackForward = Config
			? Config->TrackForward
			: FVector::ForwardVector;
		const bool bUseFixedWorldXAxis =
			Kind == ENightRoadsideKind::House;
		const FVector FirstPathLocation = Path.Nodes[0].Location;
		const FVector LastPathLocation = Path.Nodes.Last().Location;
		const float XDelta = LastPathLocation.X - FirstPathLocation.X;
		const float FixedXAxisDirection =
			XDelta < -KINDA_SMALL_NUMBER ? -1.f : 1.f;
		const float GenerationLength =
			bUseFixedWorldXAxis && FMath::Abs(XDelta) > KINDA_SMALL_NUMBER
			? FMath::Abs(XDelta)
			: Path.Length;
		for (const int32 Side : { -1, 1 })
		{
			FRandomStream Rng(MakeSeed(
				BaseSeed,
				Kind,
				Side,
				Settings.RandomSeedOffset));
			float Distance = 0.f;
			int32 Guard = 0;
			while (Distance <= GenerationLength + KINDA_SMALL_NUMBER
				&& Guard++ < 4096)
			{
				const FResolvedEntry* Entry = PickEntry(Rng, Entries);
				if (!Entry)
				{
					break;
				}

				const float SampleDistance = bUseFixedWorldXAxis
					&& GenerationLength > KINDA_SMALL_NUMBER
					? Distance / GenerationLength * Path.Length
					: Distance;
				const FPathSample Sample = SamplePath(
					Path,
					Bridges,
					FallbackForward,
					SampleDistance);
				FVector PlacementLocation = Sample.Location;
				FVector PlacementTangent = Sample.Tangent;
				FVector PlacementRight = Sample.Right;
				if (bUseFixedWorldXAxis)
				{
					// Houses use the translated road centerline location while retaining
					// their existing fixed world-X orientation and world-Y side offset.
					PlacementLocation += FVector::UpVector
						* (Config ? Config->HouseInitialZOffsetCm : 0.f);
					PlacementTangent =
						FVector(FixedXAxisDirection, 0.f, 0.f);
					PlacementRight = FVector::CrossProduct(
						FVector::UpVector,
						PlacementTangent).GetSafeNormal();
				}

				const FVector MirrorScale = Side > 0
					? FVector(1.f, -1.f, 1.f)
					: FVector::OneVector;
				const FVector MirroredMarkerStart(
					Entry->Start.X * MirrorScale.X,
					Entry->Start.Y * MirrorScale.Y,
					Entry->Start.Z * MirrorScale.Z);
				const FVector MirroredMarkerEnd(
					Entry->End.X * MirrorScale.X,
					Entry->End.Y * MirrorScale.Y,
					Entry->End.Z * MirrorScale.Z);
				const FVector LocalMarkerDelta =
					(MirroredMarkerEnd - MirroredMarkerStart).GetSafeNormal();
				if (LocalMarkerDelta.IsNearlyZero())
				{
					OutError = FString::Printf(
						TEXT("%s roadside Blueprint '%s' has invalid marker direction."),
						Label,
						*GetNameSafe(Entry->Class));
					return false;
				}

				const FQuat PathRotation =
					FRotationMatrix::MakeFromXZ(
						PlacementTangent,
						FVector::UpVector).ToQuat();
				const FQuat MarkerAlignment =
					FQuat::FindBetweenNormals(
						LocalMarkerDelta,
						FVector::ForwardVector);
				FQuat WorldRotation = PathRotation * MarkerAlignment;
				if (Kind == ENightRoadsideKind::Pole
					&& Settings.RandomYawRangeDeg > 0.f)
				{
					const float RandomYaw = Rng.FRandRange(
						-Settings.RandomYawRangeDeg,
						Settings.RandomYawRangeDeg);
					WorldRotation =
						FQuat(
							FVector::UpVector,
							FMath::DegreesToRadians(RandomYaw))
						* WorldRotation;
				}

				const float SideOffset = Side < 0
					? Settings.LeftBridgeOffsetCm
					: Settings.RightBridgeOffsetCm;
				const FVector StartWorld =
					PlacementLocation
					+ PlacementRight * (Side < 0 ? -SideOffset : SideOffset)
					+ FVector::UpVector * Settings.ZOffsetCm;
				const FVector ActorLocation =
					StartWorld
					- WorldRotation.RotateVector(MirroredMarkerStart);

				FNightRoadsidePropSpec Spec;
				Spec.Kind = Kind;
				Spec.Side = Side;
				Spec.PathSegmentIndex = Sample.SegmentIndex;
				Spec.FromStoneIndex = Sample.FromStoneIndex;
				Spec.ToStoneIndex = Sample.ToStoneIndex;
				Spec.DistanceAlongPath = SampleDistance;
				Spec.PropClass = Entry->Class;
				Spec.WorldTransform = FTransform(
					WorldRotation,
					ActorLocation,
					MirrorScale);
				OutSpecs.Add(Spec);

				Distance += Entry->Span + FMath::Max(0.f, Settings.SpacingCm);
			}
		}
		return true;
	}
}

bool UNightCourseDirector::BuildCourseForPreview(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges) const
{
	TArray<FNightAtomVisualBinding> IgnoredVisualBindings;
	TArray<FNightForkAtomSpec> IgnoredForkAtoms;
	return BuildCourseForPreview(
		OutStones,
		OutBeats,
		OutBridges,
		IgnoredVisualBindings,
		IgnoredForkAtoms);
}

bool UNightCourseDirector::BuildCourseForPreview(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges,
	TArray<FNightAtomVisualBinding>& OutVisualBindings) const
{
	TArray<FNightForkAtomSpec> IgnoredForkAtoms;
	return BuildCourseForPreview(
		OutStones,
		OutBeats,
		OutBridges,
		OutVisualBindings,
		IgnoredForkAtoms);
}

bool UNightCourseDirector::BuildCourseForPreview(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges,
	TArray<FNightAtomVisualBinding>& OutVisualBindings,
	TArray<FNightForkAtomSpec>& OutForkAtoms) const
{
	OutStones.Reset();
	OutBeats.Reset();
	OutBridges.Reset();
	OutVisualBindings.Reset();
	OutForkAtoms.Reset();
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
	return BuildAtomRouteCourse(
		OutStones,
		OutBeats,
		OutBridges,
		OutVisualBindings,
		OutForkAtoms);
}

bool UNightCourseDirector::BuildRoadsideSpecs(
	TArray<FNightRoadsidePropSpec>& OutSpecs) const
{
	return BuildRoadsideSpecs(
		StoneSpecs,
		BridgeSpecs,
		ForkAtomSpecs,
		OutSpecs);
}

bool UNightCourseDirector::BuildRoadsideSpecs(
	const TArray<FNightStoneSpec>& InStones,
	const TArray<FNightBridgeSpec>& InBridges,
	TArray<FNightRoadsidePropSpec>& OutSpecs) const
{
	return BuildRoadsideSpecs(
		InStones,
		InBridges,
		ForkAtomSpecs,
		OutSpecs);
}

bool UNightCourseDirector::BuildRoadsideSpecs(
	const TArray<FNightStoneSpec>& InStones,
	const TArray<FNightBridgeSpec>& InBridges,
	const TArray<FNightForkAtomSpec>& InForkAtoms,
	TArray<FNightRoadsidePropSpec>& OutSpecs) const
{
	OutSpecs.Reset();
	if (!Config)
	{
		return false;
	}

	NightCourseRoadside_Private::FPath Path;
	if (!NightCourseRoadside_Private::BuildPath(Config, InStones, Path))
	{
		return true;
	}

	const bool bUseRuntimeSeed = bHasRuntimeSeed
		&& (bBuildingRuntimeCourse || bRunning);
	const int32 BaseSeed = bUseRuntimeSeed
		? RuntimeSeed
		: (Config->CourseRuleData ? Config->CourseRuleData->Seed : 1001);
	FString Error;
	if (!NightCourseRoadside_Private::AppendCategory(
		Config,
		Config->HouseRoadside,
		ENightRoadsideKind::House,
		TEXT("House"),
		BaseSeed,
		Path,
		InBridges,
		OutSpecs,
		Error)
		|| !NightCourseRoadside_Private::AppendCategory(
			Config,
			Config->PoleRoadside,
			ENightRoadsideKind::Pole,
			TEXT("Pole"),
			BaseSeed,
			Path,
			InBridges,
			OutSpecs,
			Error))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] Roadside build failed: %s."),
			Error.IsEmpty() ? TEXT("unknown error") : *Error);
		OutSpecs.Reset();
		return false;
	}

	if (Config->ForkHouseExclusionCm > 0.f
		&& InForkAtoms.Num() > 0)
	{
		const UNightG1CourseConfig* CourseConfig = Config;
		float MinForkDistance = TNumericLimits<float>::Max();
		float MaxForkDistance = -TNumericLimits<float>::Max();
		auto IncludeForkPoint = [
			&MinForkDistance,
			&MaxForkDistance,
			CourseConfig](
			const FVector& Point)
		{
			const float Distance = Point.X - CourseConfig->TrackOrigin.X;
			MinForkDistance = FMath::Min(MinForkDistance, Distance);
			MaxForkDistance = FMath::Max(MaxForkDistance, Distance);
		};
		auto IncludeForkBounds = [&IncludeForkPoint](
			const FNightForkAtomSpec& ForkAtom)
		{
			const ANightCourseForkAtomActor* Defaults =
				ForkAtom.ActorClass
				? ForkAtom.ActorClass->GetDefaultObject<ANightCourseForkAtomActor>()
				: nullptr;
			if (!Defaults)
			{
				IncludeForkPoint(ForkAtom.WorldTransform.GetLocation());
				IncludeForkPoint(ForkAtom.LeftExitTransform.GetLocation());
				IncludeForkPoint(ForkAtom.RightExitTransform.GetLocation());
				return;
			}

			FVector LocalMin;
			FVector LocalMax;
			Defaults->GetLocalArtBounds(LocalMin, LocalMax);
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
				IncludeForkPoint(
					ForkAtom.WorldTransform.TransformPosition(LocalCorner));
			}
		};

		for (const FNightForkAtomSpec& ForkAtom : InForkAtoms)
		{
			IncludeForkBounds(ForkAtom);
		}
		for (const FNightStoneSpec& Stone : InStones)
		{
			if (Stone.bForkConnectorVisualOnly)
			{
				IncludeForkPoint(Stone.WorldLocation);
			}
		}

		if (MinForkDistance <= MaxForkDistance)
		{
			const float ToleranceCm =
				FMath::Max(0.f, CourseConfig->ForkHouseExclusionCm);
			MinForkDistance -= ToleranceCm;
			MaxForkDistance += ToleranceCm;
			const int32 RemovedHouseCount = OutSpecs.RemoveAll(
				[CourseConfig, MinForkDistance, MaxForkDistance](
					const FNightRoadsidePropSpec& Spec)
				{
					if (Spec.Kind != ENightRoadsideKind::House)
					{
						return false;
					}
					const float Distance =
						Spec.WorldTransform.GetLocation().X
						- CourseConfig->TrackOrigin.X;
					return Distance >= MinForkDistance
						&& Distance <= MaxForkDistance;
				});
			if (RemovedHouseCount > 0)
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("[NightCourse][Stage=Roadside] removed %d houses inside Fork ArtBounds tolerance %.1fcm (range %.1f..%.1fcm)."),
					RemovedHouseCount,
					ToleranceCm,
					MinForkDistance,
					MaxForkDistance);
			}
		}
	}
	return true;
}

bool UNightCourseDirector::BuildAtomRouteCourse(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges) const
{
	TArray<FNightAtomVisualBinding> IgnoredVisualBindings;
	TArray<FNightForkAtomSpec> IgnoredForkAtoms;
	return BuildAtomRouteCourse(
		OutStones,
		OutBeats,
		OutBridges,
		IgnoredVisualBindings,
		IgnoredForkAtoms);
}

bool UNightCourseDirector::BuildAtomRouteCourse(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges,
	TArray<FNightAtomVisualBinding>& OutVisualBindings) const
{
	TArray<FNightForkAtomSpec> IgnoredForkAtoms;
	return BuildAtomRouteCourse(
		OutStones,
		OutBeats,
		OutBridges,
		OutVisualBindings,
		IgnoredForkAtoms);
}

bool UNightCourseDirector::BuildAtomRouteCourse(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges,
	TArray<FNightAtomVisualBinding>& OutVisualBindings,
	TArray<FNightForkAtomSpec>& OutForkAtoms) const
{
	OutStones.Reset();
	OutBeats.Reset();
	OutBridges.Reset();
	OutVisualBindings.Reset();
	OutForkAtoms.Reset();
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
	// Course handoff anchors are intentionally planar. Atom exits only carry XY;
	// every atom entry/exit used to connect the next atom is pinned to Z=0.
	const float CourseStartZ = 0.f;
	const bool bRuntimeBuildContext = bBuildingRuntimeCourse || bRunning;
	const ENightRouteId BuildRoute = CurrentRoute != ENightRouteId::None
		? CurrentRoute
		: (bRuntimeBuildContext ? ENightRouteId::None : Config->PreviewRoute);
	const ENightRouteId BuildDefaultRoute =
		bRuntimeBuildContext ? ActiveDefaultRoute : Config->PreviewDefaultRoute;
	const FNightRuleAtomQueue* DefaultRouteQueue =
		Rule->RouteModes.Find(BuildDefaultRoute);
	if (!DefaultRouteQueue || DefaultRouteQueue->Atoms.Num() == 0)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] RouteModes has no usable queue for default route %d."),
			static_cast<int32>(BuildDefaultRoute));
		return false;
	}
	const bool bGenerateAllAtomsForTesting =
		Rule->bGenerateAllAtomsForTesting;
	TArray<FNightRuleAtomEntry> PlannerEntries;
	TArray<ENightRouteId> PlannerForkConnectorRoutes;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Compose] Begin Atom composition context=%s seed=%d defaultRoute=%d selectedRoute=%d modeTemplates=%d modeTarget=%d branchRoutes=%d TransitionJumpGapCm=%.1f."),
		bRuntimeBuildContext ? TEXT("Runtime") : TEXT("Preview"),
		bHasRuntimeSeed ? RuntimeSeed : Rule->Seed,
		static_cast<int32>(BuildDefaultRoute),
		static_cast<int32>(BuildRoute),
		DefaultRouteQueue->Atoms.Num(),
		bGenerateAllAtomsForTesting
			? DefaultRouteQueue->Atoms.Num()
			: (DefaultRouteQueue->TargetAtomCount > 0
				? DefaultRouteQueue->TargetAtomCount
				: DefaultRouteQueue->Atoms.Num()),
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

	const int32 AuthoredBaseAtomCount = bGenerateAllAtomsForTesting
		? DefaultRouteQueue->Atoms.Num()
		: ResolveMainRouteAtomCount(
			DefaultRouteQueue->TargetAtomCount > 0
				? DefaultRouteQueue->TargetAtomCount
				: DefaultRouteQueue->Atoms.Num());
	// RouteModes is the authoritative source for the selected pre-fork route
	// length. Keep the CourseConfig override only as a legacy level fallback.
	const int32 ForkIndex = bGenerateAllAtomsForTesting
		? AuthoredBaseAtomCount
		: (ActiveBootstrap.bUseCourseQueueOverride
			? AuthoredBaseAtomCount
			: (Config->ForkAfterBaseAtomIndex != INDEX_NONE
				? Config->ForkAfterBaseAtomIndex
				: AuthoredBaseAtomCount));
	const bool bUsesForkBase =
		BuildRoute != ENightRouteId::None
		|| (IsForkEnabledForActiveCourse() && ForkIndex != INDEX_NONE);
	if (bUsesForkBase
		&& (ForkIndex <= 0 || ForkIndex > AuthoredBaseAtomCount))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Compose] ForkAfterBaseAtomIndex=%d is outside generated main route length %d."),
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
			TEXT("[NightCourse][Stage=Compose] Generated main Atom count is %d; configure the selected RouteModes queue."),
			GeneratedBaseAtomCount);
		return false;
	}

	FRandomStream TemplateSelectionStream(
		(bHasRuntimeSeed ? RuntimeSeed : Rule->Seed) ^ 0x54454D50);
	auto AppendWeightedTemplates =
		[&PlannerEntries, &PlannerForkConnectorRoutes, &TemplateSelectionStream](
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
			PlannerForkConnectorRoutes.Add(ENightRouteId::None);
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
	auto AppendAllTemplates =
		[&PlannerEntries, &PlannerForkConnectorRoutes](
			const TArray<FNightRuleAtomEntry>& Templates,
			const FString& QueueLabel,
			const int32 StartIndex) -> bool
	{
		if (Templates.Num() == 0
			|| StartIndex < 0
			|| StartIndex >= Templates.Num())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=AtomTemplateSelect] %s has no templates for all-Atom test mode."),
				*QueueLabel);
			return false;
		}

		PlannerEntries.Reserve(
			PlannerEntries.Num() + Templates.Num() - StartIndex);
		for (int32 TemplateIndex = StartIndex;
			TemplateIndex < Templates.Num();
			++TemplateIndex)
		{
			PlannerEntries.Add(Templates[TemplateIndex]);
			PlannerForkConnectorRoutes.Add(ENightRouteId::None);
			UE_LOG(
				LogTemp,
				Log,
				TEXT("[NightCourse][Stage=AtomTemplateSelect] %s test slot=%d template=%d key='%s'."),
				*QueueLabel,
				TemplateIndex,
				TemplateIndex,
				*PlannerEntries.Last().AtomKey);
		}
		return true;
	};

	if (!(bGenerateAllAtomsForTesting
			? AppendAllTemplates(
				DefaultRouteQueue->Atoms,
				FString::Printf(
					TEXT("RouteModes[%d]"),
					static_cast<int32>(BuildDefaultRoute)),
				0)
			: AppendWeightedTemplates(
		DefaultRouteQueue->Atoms,
		GeneratedBaseAtomCount,
		FString::Printf(
			TEXT("RouteModes[%d]"),
			static_cast<int32>(BuildDefaultRoute)))))
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
		const int32 BranchAtomCount = bGenerateAllAtomsForTesting
			? BranchQueue->Atoms.Num()
			: ResolveForkRouteAtomCount(
				BranchQueue->TargetAtomCount > 0
					? BranchQueue->TargetAtomCount
					: BranchQueue->Atoms.Num());
		if (BranchAtomCount <= 0)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse] Selected route %d has no branch Atom count."),
				static_cast<int32>(BuildRoute));
			return false;
		}

		// Keep the first branch Atom deterministic and reuse the same Atom as
		// the pre-choice connector shown at this route's fork exit. In inspection
		// mode, authored order is used so every template is visible exactly once.
		FNightRuleAtomEntry FirstBranchEntry;
		int32 ResolvedFirstBranchTemplateIndex = INDEX_NONE;
		if (bGenerateAllAtomsForTesting)
		{
			FirstBranchEntry = BranchQueue->Atoms[0];
			ResolvedFirstBranchTemplateIndex = 0;
		}
		else
		{
			FRandomStream FirstBranchStream(
				(bHasRuntimeSeed ? RuntimeSeed : Rule->Seed)
				^ (0x464F524B + static_cast<int32>(BuildRoute) * 7919));
			if (!NightCourseAtom_Private::SelectWeightedTemplate(
				BranchQueue->Atoms,
				FirstBranchStream,
				FirstBranchEntry,
				ResolvedFirstBranchTemplateIndex))
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse][Stage=AtomTemplateSelect] BranchRoutes[%d] has no positive-weight first connector Atom."),
					static_cast<int32>(BuildRoute));
				return false;
			}
		}
		PlannerEntries.Add(MoveTemp(FirstBranchEntry));
		PlannerForkConnectorRoutes.Add(ENightRouteId::None);
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[NightCourse][Stage=AtomTemplateSelect] BranchRoutes[%d] first connector template=%d."),
			static_cast<int32>(BuildRoute),
			ResolvedFirstBranchTemplateIndex);

		if (BranchAtomCount > 1)
		{
			const bool bAppendedBranchTemplates = bGenerateAllAtomsForTesting
				? AppendAllTemplates(
					BranchQueue->Atoms,
					FString::Printf(
						TEXT("BranchRoutes[%d]"),
						static_cast<int32>(BuildRoute)),
					1)
				: AppendWeightedTemplates(
					BranchQueue->Atoms,
					BranchAtomCount - 1,
					FString::Printf(
						TEXT("BranchRoutes[%d]"),
						static_cast<int32>(BuildRoute)));
			if (!bAppendedBranchTemplates)
			{
				return false;
			}
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
	const bool bForkRouteEnabled =
		IsForkEnabledForActiveCourse() && Rule->BranchRoutes.Num() > 0;
	const ENightForkPair BuildForkPair = bRuntimeBuildContext
		? ActiveForkPair
		: Config->PreviewForkPair;
	UClass* ForkAtomClass = nullptr;
	const ANightCourseForkAtomActor* ForkAtomDefaults = nullptr;
	bool bHasForkAtom = false;
	bool bUseForkExitForNextAtom = false;
	FTransform SelectedForkExit = FTransform::Identity;
	FTransform ForkLeftExitForConnector = FTransform::Identity;
	FTransform ForkRightExitForConnector = FTransform::Identity;
	ENightRouteId ForkLeftRoute = ENightRouteId::None;
	ENightRouteId ForkRightRoute = ENightRouteId::None;
	bool bForkExitTransformsReady = false;
	if (bForkRouteEnabled)
	{
		const TSoftClassPtr<ANightCourseForkAtomActor>* ForkClassRef =
			Config->ForkAtomMap.Find(BuildForkPair);
		if (!ForkClassRef || ForkClassRef->IsNull())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[NightCourse][Stage=ForkAtom] no configured ForkAtomMap entry for pair=%d; using the existing logic-only fork."),
				static_cast<int32>(BuildForkPair));
		}
		else
		{
			FString ForkError;
			ForkAtomClass = Config->ResolveForkAtomClass(BuildForkPair, ForkError);
			if (!ForkAtomClass)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse][Stage=ForkAtom] pair=%d cannot resolve configured Blueprint: %s."),
					static_cast<int32>(BuildForkPair),
					ForkError.IsEmpty() ? TEXT("<unknown>") : *ForkError);
				return false;
			}

			ForkAtomDefaults =
				ForkAtomClass->GetDefaultObject<ANightCourseForkAtomActor>();
			if (!ForkAtomDefaults)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse][Stage=ForkAtom] pair=%d class='%s' has no valid CDO."),
					static_cast<int32>(BuildForkPair),
					*GetNameSafe(ForkAtomClass));
				return false;
			}

			FString ForkValidationError;
			if (!ForkAtomDefaults->ValidateForkAtom(ForkValidationError))
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse][Stage=ForkAtom] pair=%d class='%s' is invalid: %s."),
					static_cast<int32>(BuildForkPair),
					*GetNameSafe(ForkAtomClass),
					*ForkValidationError);
				return false;
			}
			bHasForkAtom = true;
		}
	}

	if (bHasForkAtom)
	{
		bool bForcedAB = false;
		UNightForkController::ResolvePairRoutes(
			BuildForkPair,
			ForkLeftRoute,
			ForkRightRoute,
			bForcedAB);
		(void)bForcedAB;

		// The base course has no selected route yet, so place one complete
		// connector Atom on each authored fork exit. These connector Atoms are
		// visual-only until the player chooses a side; the selected rebuild
		// replaces them with the playable branch course.
		if (BuildRoute == ENightRouteId::None)
		{
			for (const ENightRouteId ConnectorRoute :
				{ForkLeftRoute, ForkRightRoute})
			{
				if (ConnectorRoute == ENightRouteId::None)
				{
					continue;
				}

				const FNightRuleAtomQueue* ConnectorQueue =
					Rule->BranchRoutes.Find(ConnectorRoute);
				if (!ConnectorQueue || ConnectorQueue->Atoms.Num() == 0)
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[NightCourse][Stage=ForkConnector] route=%d has no branch Atom template."),
						static_cast<int32>(ConnectorRoute));
					return false;
				}

				FRandomStream ConnectorStream(
					(bHasRuntimeSeed ? RuntimeSeed : Rule->Seed)
					^ (0x464F524B + static_cast<int32>(ConnectorRoute) * 7919));
				FNightRuleAtomEntry ConnectorEntry;
				int32 ConnectorTemplateIndex = INDEX_NONE;
				if (!NightCourseAtom_Private::SelectWeightedTemplate(
					ConnectorQueue->Atoms,
					ConnectorStream,
					ConnectorEntry,
					ConnectorTemplateIndex))
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[NightCourse][Stage=ForkConnector] route=%d has no positive-weight Atom template."),
						static_cast<int32>(ConnectorRoute));
					return false;
				}

				PlannerEntries.Add(MoveTemp(ConnectorEntry));
				PlannerForkConnectorRoutes.Add(ConnectorRoute);
				UE_LOG(
					LogTemp,
					Display,
					TEXT("[NightCourse][Stage=ForkConnector] queued visual connector route=%d template=%d."),
					static_cast<int32>(ConnectorRoute),
					ConnectorTemplateIndex);
			}
		}
	}

	const int32 AtomCount = PlannerEntries.Num();
	int32 FoeOrdinal = 0;
	FRandomStream AtomSelectionStream(
		(bHasRuntimeSeed ? RuntimeSeed : Rule->Seed) ^ 0x41544F4D);
	FRandomStream RuleRandomStream(
		bHasRuntimeSeed ? RuntimeSeed : Rule->Seed);

	for (int32 AtomSlotIndex = 0; AtomSlotIndex < AtomCount; ++AtomSlotIndex)
	{
		const FNightRuleAtomEntry& PlannerEntry = PlannerEntries[AtomSlotIndex];
		const ENightRouteId ForkConnectorRoute =
			PlannerForkConnectorRoutes.IsValidIndex(AtomSlotIndex)
			? PlannerForkConnectorRoutes[AtomSlotIndex]
			: ENightRouteId::None;
		const bool bIsForkConnector =
			ForkConnectorRoute != ENightRouteId::None;
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
				EIngredientId MappedDropId = EIngredientId::None;
				if (Config->TryGetFoeDropId(
					LocalStones[ActionIndex + 1].FoeId,
					MappedDropId))
				{
					LocalStones[ActionIndex + 1].DropId = MappedDropId;
					LocalStones[ActionIndex + 1].DropCount =
						Config->DefaultDropCount;
				}
				else if (Config->bRandomizeEnemyDrops)
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

		// A pre-choice connector and the first Atom after a selected exit are
		// both hand-off Atoms. The pre-choice connector is visual-only; the
		// selected one becomes part of the playable branch rebuild.
		const bool bIsSelectedBranchEntry = bUseForkExitForNextAtom;
		const bool bIsForkBranchConnection =
			bIsSelectedBranchEntry || bIsForkConnector;
		// Only unselected visual fork connectors may bypass; playable branch
		// Atoms always go through the strict LayoutBounds path below.
		const bool bBypassLayoutBoundsForBranch =
			bIsForkConnector;
		FTransform TargetEntry = InitialEntry;
		FVector TargetEntryLocation = TargetEntry.GetLocation();
		TargetEntryLocation.Z = CourseStartZ;
		TargetEntry.SetLocation(TargetEntryLocation);
		if (!bFirstAtom)
		{
			if (bIsForkConnector)
			{
				if (!bForkExitTransformsReady)
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[NightCourse][Stage=ForkConnector] route=%d has no resolved fork exit transform."),
						static_cast<int32>(ForkConnectorRoute));
					if (AtomInstance)
					{
						AtomInstance->Destroy();
					}
					return false;
				}
				if (ForkConnectorRoute == ForkLeftRoute)
				{
					TargetEntry = ForkLeftExitForConnector;
				}
				else if (ForkConnectorRoute == ForkRightRoute)
				{
					TargetEntry = ForkRightExitForConnector;
				}
				else
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[NightCourse][Stage=ForkConnector] route=%d is not mapped to either fork exit."),
						static_cast<int32>(ForkConnectorRoute));
					if (AtomInstance)
					{
						AtomInstance->Destroy();
					}
					return false;
				}
				const FVector ExitForward =
					TargetEntry.GetRotation().GetForwardVector().GetSafeNormal();
				const FVector SafeExitForward = ExitForward.IsNearlyZero()
					? TrackForward
					: ExitForward;
				TargetEntry.AddToTranslation(
					SafeExitForward * FMath::Max(0.f, Config->BranchEntryGapCm));
			}
			else if (bUseForkExitForNextAtom)
			{
				// The first playable LandingPoint is aligned to the selected
				// exit, then moved forward by the authored safety gap. The
				// bounds solver below may move it farther if the authored
				// Atom art extends behind its first LandingPoint.
				TargetEntry = SelectedForkExit;
				const FVector ExitForward =
					TargetEntry.GetRotation().GetForwardVector().GetSafeNormal();
				const FVector SafeExitForward = ExitForward.IsNearlyZero()
					? TrackForward
					: ExitForward;
				TargetEntry.AddToTranslation(
					SafeExitForward * FMath::Max(0.f, Config->BranchEntryGapCm));
				bUseForkExitForNextAtom = false;
			}
			else
			{
				const FVector JumpForward =
					PreviousExit.GetRotation().GetForwardVector().GetSafeNormal();
				const FVector SafeJumpForward = JumpForward.IsNearlyZero()
					? TrackForward
					: JumpForward;
				TargetEntry.SetLocation(
					PreviousExit.GetLocation()
					+ SafeJumpForward * FMath::Max(0.f, Route->TransitionJumpGapCm));
				TargetEntry.SetRotation(PreviousExit.GetRotation());
			}
		}

		FTransform LocalEntry = AtomSource->GetEntryAnchorTransform();
		const FTransform LocalExit = AtomSource->GetExitAnchorTransform();
		// Normalize after every route/fork offset so no authored exit can leak
		// a Z value into the next atom's entry anchor.
		TargetEntryLocation = TargetEntry.GetLocation();
		TargetEntryLocation.Z = CourseStartZ;
		TargetEntry.SetLocation(TargetEntryLocation);
		const FTransform BaseAtomWorld = NightCourseAtom_Private::MakeAtomWorldTransform(
			TargetEntry,
			LocalEntry);

		TArray<float> CandidateYaws;
		if (BuildRoute != ENightRouteId::None || bIsForkBranchConnection)
		{
			// Keep every selected branch Atom at zero authored yaw offset so the
			// bounds solver has the full translation budget and no random turn
			// can push the branch outside the LayoutBounds.
			CandidateYaws.Add(0.f);
		}
		else if (AtomSource->bAllowDeterministicRandomYaw)
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
		FTransform FallbackAtomWorld = BaseAtomWorld;
		bool bFoundValidTransform = false;
		bool bHasFallbackCandidate = false;
		for (const float CandidateYaw : CandidateYaws)
		{
			const FQuat DeltaRotation = FRotator(0.f, CandidateYaw, 0.f).Quaternion();
			FTransform Candidate = BaseAtomWorld;
			Candidate.SetRotation(DeltaRotation * BaseAtomWorld.GetRotation());
			Candidate.SetLocation(
				TargetEntry.GetLocation()
				+ DeltaRotation.RotateVector(BaseAtomWorld.GetLocation() - TargetEntry.GetLocation()));
			if (!bHasFallbackCandidate)
			{
				FallbackAtomWorld = Candidate;
				bHasFallbackCandidate = true;
			}
			if (bBypassLayoutBoundsForBranch
				|| IsAtomTransformInsideLayoutBounds(AtomSource, Candidate))
			{
				AtomWorld = Candidate;
				bFoundValidTransform = true;
				break;
			}
		}
		if (bBypassLayoutBoundsForBranch && bFoundValidTransform)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[NightCourse][Stage=AtomTransform] slot=%d key='%s' bypassed LayoutBounds for an unselected visual fork connector."),
				AtomSlotIndex,
				*AtomKey);
		}
		if (!bFoundValidTransform)
		{
			if (TryTranslateAtomYIntoLayoutBounds(
				AtomSource,
				FallbackAtomWorld))
			{
				AtomWorld = FallbackAtomWorld;
				bFoundValidTransform = true;
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[NightCourse][Stage=AtomTransform] slot=%d key='%s' zero-yaw Atom translated on Y by %.2fcm to fit LayoutBounds."),
					AtomSlotIndex,
					*AtomKey,
					FallbackAtomWorld.GetLocation().Y - BaseAtomWorld.GetLocation().Y);
			}
			else
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse][Stage=AtomTransform] slot=%d key='%s' has no valid rotation or translated fallback in scene bounds."),
					AtomSlotIndex,
					*AtomKey);
				if (AtomInstance)
				{
					AtomInstance->Destroy();
				}
				return false;
			}
		}

		if (!bIsForkConnector && LocalStones.Num() > 0)
		{
			const float PreviousRouteStoneX =
				!bFirstAtom && OutStones.IsValidIndex(PreviousLastStoneIndex)
				? OutStones[PreviousLastStoneIndex].WorldLocation.X
				: PreviousExit.GetLocation().X;
			const float RequiredFirstX = bFirstAtom
				? Config->TrackOrigin.X
				: FMath::Max(
					PreviousExit.GetLocation().X,
					PreviousRouteStoneX)
					+ FMath::Max(
						1.f,
						bIsSelectedBranchEntry
							? Config->BranchEntryGapCm
							: Route->TransitionJumpGapCm);
			const float FirstLandingX = AtomWorld.TransformPosition(
				NightCourseAtom_Private::GetLocalStoneLocation(LocalStones[0])).X;
			const float XTranslation = FMath::Max(
				0.f,
				RequiredFirstX - FirstLandingX);
			if (XTranslation > KINDA_SMALL_NUMBER)
			{
				AtomWorld.AddToTranslation(FVector(XTranslation, 0.f, 0.f));
				if (!IsAtomTransformInsideLayoutBounds(AtomSource, AtomWorld))
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[NightCourse][Stage=AtomTransform] slot=%d key='%s' cannot move Atom forward on world X without leaving LayoutBounds."),
						AtomSlotIndex,
						*AtomKey);
					if (AtomInstance)
					{
						AtomInstance->Destroy();
					}
					return false;
				}
				UE_LOG(
					LogTemp,
					Display,
					TEXT("[NightCourse][Stage=AtomTransform] slot=%d key='%s' shifted forward on world X by %.2fcm."),
					AtomSlotIndex,
					*AtomKey,
					XTranslation);
			}

			float PreviousLandingX = bFirstAtom
				? -TNumericLimits<float>::Max()
				: FMath::Max(
					PreviousExit.GetLocation().X,
					PreviousRouteStoneX);
			for (const FNightStoneSpec& LocalStone : LocalStones)
			{
				const float LandingX = AtomWorld.TransformPosition(
					NightCourseAtom_Private::GetLocalStoneLocation(LocalStone)).X;
				if (LandingX <= PreviousLandingX + KINDA_SMALL_NUMBER)
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[NightCourse][Stage=AtomTransform] slot=%d key='%s' LandingPoint X regressed (%.2f <= %.2f)."),
						AtomSlotIndex,
						*AtomKey,
						LandingX,
						PreviousLandingX);
					if (AtomInstance)
					{
						AtomInstance->Destroy();
					}
					return false;
				}
				PreviousLandingX = LandingX;
			}
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
				Config->TrackOrigin);
			WorldStone.YawDeg = AtomWorld.TransformRotation(
				FRotator(0.f, LocalStone.YawDeg, 0.f).Quaternion()).Rotator().Yaw;
			WorldStone.bForkConnectorVisualOnly = bIsForkConnector;
			OutStones.Add(WorldStone);
		}

		if (!bIsForkConnector
			&& !bFirstAtom
			&& PreviousLastStoneIndex != INDEX_NONE
			&& StoneOffset < OutStones.Num())
		{
			FNightBeatSpec TransitionBeat;
			TransitionBeat.FromStoneIndex = PreviousLastStoneIndex;
			TransitionBeat.ToStoneIndex = StoneOffset;
			TransitionBeat.Action = ENightNodeKind::Hazard;
			OutBeats.Add(TransitionBeat);
		}

		if (!bIsForkConnector)
		{
			for (FNightBeatSpec LocalBeat : LocalBeats)
			{
				LocalBeat.FromStoneIndex += StoneOffset;
				LocalBeat.ToStoneIndex += StoneOffset;
				OutBeats.Add(LocalBeat);
			}
		}

		for (const FNightBridgeSpec& LocalBridge : LocalBridges)
		{
			FNightBridgeSpec WorldBridge = LocalBridge;
			WorldBridge.FromStoneIndex += StoneOffset;
			WorldBridge.ToStoneIndex += StoneOffset;
			WorldBridge.WorldLocation = AtomWorld.TransformPosition(LocalBridge.WorldLocation);
			WorldBridge.YawDeg = AtomWorld.TransformRotation(
				FRotator(0.f, LocalBridge.YawDeg, 0.f).Quaternion()).Rotator().Yaw;
			WorldBridge.bForkConnectorVisualOnly = bIsForkConnector;
			OutBridges.Add(WorldBridge);
		}

		for (FNightAtomVisualBinding LocalBinding : LocalVisualBindings)
		{
			LocalBinding.AtomKey = AtomKey;
			LocalBinding.AtomSlotIndex = AtomSlotIndex;
			LocalBinding.bForkConnectorVisualOnly = bIsForkConnector;
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

		if (!bIsForkConnector)
		{
			FVector PlanarExitLocation =
				AtomWorld.TransformPosition(LocalExit.GetLocation());
			PlanarExitLocation.Z = CourseStartZ;
			PreviousExit = FTransform(
				AtomWorld.TransformRotation(LocalExit.GetRotation()),
				PlanarExitLocation,
				FVector::OneVector);
			PreviousLastStoneIndex = OutStones.Num() - 1;
			bFirstAtom = false;
		}

		if (bHasForkAtom
			&& AtomSlotIndex == GeneratedBaseAtomCount - 1)
		{
			const FTransform ForkLocalEntry =
				ForkAtomDefaults->GetEntryAnchorTransform();
			FTransform ForkWorld =
				NightCourseAtom_Private::MakeAtomWorldTransform(
					PreviousExit,
					ForkLocalEntry);
			const float ForkEntryYBeforeReset =
				ForkWorld.TransformPosition(ForkLocalEntry.GetLocation()).Y;
			ForkWorld.AddToTranslation(
				FVector(0.f, -ForkEntryYBeforeReset, 0.f));

			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=ForkAtom] pair=%d forced entry Y reset from %.2fcm to 0.00cm."),
				static_cast<int32>(BuildForkPair),
				ForkEntryYBeforeReset);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=ForkAtom] pair=%d kept entry anchor Z at course start %.2fcm."),
				static_cast<int32>(BuildForkPair),
				CourseStartZ);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=ForkAtom] pair=%d bypassed LayoutBounds for the fork presentation and both exit connectors."),
				static_cast<int32>(BuildForkPair));

			TArray<FNightStoneSpec> ForkLocalStones;
			TArray<FNightBeatSpec> ForkLocalBeats;
			TArray<FNightBridgeSpec> ForkLocalBridges;
			TArray<FNightAtomVisualBinding> ForkLocalBindings;
			ForkAtomDefaults->GetLocalCourseSpecs(
				ForkLocalStones,
				ForkLocalBeats,
				ForkLocalBridges,
				ForkLocalBindings,
				BuildRoute,
				ForkLeftRoute,
				ForkRightRoute);

			const int32 ForkStoneOffset = OutStones.Num();
			const int32 ForkBridgeOffset = OutBridges.Num();
			for (FNightStoneSpec& ForkStone : ForkLocalStones)
			{
				if (!ForkStone.bHasFoe)
				{
					continue;
				}

				ForkStone.FoeId = NightCourseAtom_Private::PickFoeId(
					RuleRandomStream,
					ActiveBootstrap,
					Config,
					Config->DefaultFoeId,
					FoeOrdinal++);
				EIngredientId MappedDropId = EIngredientId::None;
				if (Config->TryGetFoeDropId(ForkStone.FoeId, MappedDropId))
				{
					ForkStone.DropId = MappedDropId;
					ForkStone.DropCount = Config->DefaultDropCount;
				}
				else if (Config->bRandomizeEnemyDrops)
				{
					ForkStone.DropId =
						NightCourseAtom_Private::PickIngredientDropId(
							RuleRandomStream,
							Config,
							ForkStone.DropId);
				}
				else if (ForkStone.DropId == EIngredientId::None)
				{
					ForkStone.DropId = Config->DefaultDropId;
				}
				if (ForkStone.DropCount <= 0)
				{
					ForkStone.DropCount = Config->DefaultDropCount;
				}
			}

			for (const FNightStoneSpec& LocalForkStone : ForkLocalStones)
			{
				FNightStoneSpec WorldStone = LocalForkStone;
				const FVector WorldLocation = ForkWorld.TransformPosition(
					NightCourseAtom_Private::GetLocalStoneLocation(LocalForkStone));
				WorldStone.bUseWorldPose = true;
				WorldStone.WorldLocation = WorldLocation;
				WorldStone.TrackDistance = NightCourseAtom_Private::GetTrackDistance(
					WorldLocation,
					Config->TrackOrigin);
				WorldStone.YawDeg = ForkWorld.TransformRotation(
					FRotator(0.f, LocalForkStone.YawDeg, 0.f).Quaternion())
					.Rotator()
					.Yaw;
				OutStones.Add(WorldStone);
			}

			if (ForkLocalStones.Num() > 0)
			{
				if (PreviousLastStoneIndex != INDEX_NONE)
				{
					FNightBeatSpec ForkEntryBeat;
					ForkEntryBeat.FromStoneIndex = PreviousLastStoneIndex;
					ForkEntryBeat.ToStoneIndex = ForkStoneOffset;
					ForkEntryBeat.Action = ForkLocalStones[0].bHasFoe
						? ENightNodeKind::Enemy
						: ENightNodeKind::Hazard;
					OutBeats.Add(ForkEntryBeat);
				}

				for (FNightBeatSpec LocalForkBeat : ForkLocalBeats)
				{
					LocalForkBeat.FromStoneIndex += ForkStoneOffset;
					LocalForkBeat.ToStoneIndex += ForkStoneOffset;
					OutBeats.Add(LocalForkBeat);
				}

				for (const FNightBridgeSpec& LocalForkBridge : ForkLocalBridges)
				{
					FNightBridgeSpec WorldBridge = LocalForkBridge;
					WorldBridge.FromStoneIndex += ForkStoneOffset;
					WorldBridge.ToStoneIndex += ForkStoneOffset;
					WorldBridge.WorldLocation = ForkWorld.TransformPosition(
						LocalForkBridge.WorldLocation);
					WorldBridge.YawDeg = ForkWorld.TransformRotation(
						FRotator(0.f, LocalForkBridge.YawDeg, 0.f).Quaternion())
						.Rotator()
						.Yaw;
					OutBridges.Add(WorldBridge);
				}

				for (FNightAtomVisualBinding LocalForkBinding : ForkLocalBindings)
				{
					LocalForkBinding.AtomKey = FString::Printf(
						TEXT("Fork_%d"),
						static_cast<int32>(BuildForkPair));
					LocalForkBinding.AtomSlotIndex =
						GeneratedBaseAtomCount - 1;
					if (LocalForkBinding.bIsBridge)
					{
						LocalForkBinding.BridgeIndex += ForkBridgeOffset;
					}
					else
					{
						LocalForkBinding.StoneIndex += ForkStoneOffset;
					}
					LocalForkBinding.LocalTransform.SetLocation(
						ForkWorld.TransformPosition(
							LocalForkBinding.LocalTransform.GetLocation()));
					LocalForkBinding.LocalTransform.SetRotation(
						ForkWorld.TransformRotation(
							LocalForkBinding.LocalTransform.GetRotation()));
					LocalForkBinding.LocalTransform.SetScale3D(
						ForkWorld.GetScale3D()
						* LocalForkBinding.LocalTransform.GetScale3D());
					OutVisualBindings.Add(MoveTemp(LocalForkBinding));
				}

				PreviousLastStoneIndex = OutStones.Num() - 1;
				UE_LOG(
					LogTemp,
					Display,
					TEXT("[NightCourse][Stage=ForkAtom] pair=%d imported LandingPoints=%d internalBeats=%d bridges=%d."),
					static_cast<int32>(BuildForkPair),
					ForkLocalStones.Num(),
					ForkLocalBeats.Num() + 1,
					ForkLocalBridges.Num());
			}

			auto MakeWorldAnchor = [&ForkWorld](const FTransform& LocalAnchor)
			{
				return FTransform(
					ForkWorld.TransformRotation(LocalAnchor.GetRotation()),
					ForkWorld.TransformPosition(LocalAnchor.GetLocation()),
					FVector::OneVector);
			};
			auto NormalizeForkExitZ = [CourseStartZ](FTransform Exit)
			{
				FVector ExitLocation = Exit.GetLocation();
				ExitLocation.Z = CourseStartZ;
				Exit.SetLocation(ExitLocation);
				return Exit;
			};
			const FTransform WorldLeftExit = NormalizeForkExitZ(
				MakeWorldAnchor(ForkAtomDefaults->GetLeftExitAnchorTransform()));
			const FTransform WorldRightExit = NormalizeForkExitZ(
				MakeWorldAnchor(ForkAtomDefaults->GetRightExitAnchorTransform()));
			ForkLeftExitForConnector = WorldLeftExit;
			ForkRightExitForConnector = WorldRightExit;
			bForkExitTransformsReady = true;

			FNightForkAtomSpec ForkSpec;
			ForkSpec.ForkPair = BuildForkPair;
			ForkSpec.ActorClass = ForkAtomClass;
			ForkSpec.WorldTransform = ForkWorld;
			ForkSpec.LeftExitTransform = WorldLeftExit;
			ForkSpec.RightExitTransform = WorldRightExit;
			OutForkAtoms.Add(ForkSpec);

			if (BuildRoute != ENightRouteId::None)
			{
				if (BuildRoute == ForkLeftRoute)
				{
					SelectedForkExit = WorldLeftExit;
				}
				else if (BuildRoute == ForkRightRoute)
				{
					SelectedForkExit = WorldRightExit;
				}
				else
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT("[NightCourse][Stage=ForkAtom] preview route=%d is not part of pair=%d; using the left fork exit."),
						static_cast<int32>(BuildRoute),
						static_cast<int32>(BuildForkPair));
					SelectedForkExit = WorldLeftExit;
				}
				bUseForkExitForNextAtom = true;
			}

			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=ForkAtom] pair=%d class='%s' world=%s leftExit=%s rightExit=%s."),
				static_cast<int32>(BuildForkPair),
				*GetNameSafe(ForkAtomClass),
				*ForkWorld.GetLocation().ToCompactString(),
				*WorldLeftExit.GetLocation().ToCompactString(),
				*WorldRightExit.GetLocation().ToCompactString());
		}

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
		TEXT("[NightCourse][Stage=Compose] Atom composition complete atoms=%d stones=%d beats=%d bridges=%d visualBindings=%d forkAtoms=%d."),
		AtomCount,
		OutStones.Num(),
		OutBeats.Num(),
		OutBridges.Num(),
		OutVisualBindings.Num(),
		OutForkAtoms.Num());
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

bool UNightCourseDirector::TryTranslateAtomAlongDirectionIntoLayoutBounds(
	const ANightCourseAtomActor* AtomDefaults,
	const FVector& WorldDirection,
	FTransform& InOutAtomWorld) const
{
	if (!bEnforceLayoutBounds || !LayoutBoundsComponent || !AtomDefaults)
	{
		return false;
	}

	const FVector SafeWorldDirection = WorldDirection.GetSafeNormal();
	if (SafeWorldDirection.IsNearlyZero())
	{
		return false;
	}

	const FTransform BoundsTransform =
		LayoutBoundsComponent->GetComponentTransform();
	const FVector BoundsExtent =
		LayoutBoundsComponent->GetScaledBoxExtent();
	if (BoundsExtent.IsNearlyZero())
	{
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

	const FVector BoundsLocalDirection =
		BoundsTransform.InverseTransformVector(SafeWorldDirection);
	float MinTranslation = -TNumericLimits<float>::Max();
	float MaxTranslation = TNumericLimits<float>::Max();
	for (const FVector& LocalCorner : Corners)
	{
		const FVector BoundsLocal = BoundsTransform.InverseTransformPosition(
			InOutAtomWorld.TransformPosition(LocalCorner));
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Direction = BoundsLocalDirection[Axis];
			const float Extent = BoundsExtent[Axis];
			if (FMath::Abs(Direction) <= KINDA_SMALL_NUMBER)
			{
				if (BoundsLocal[Axis] < -Extent - KINDA_SMALL_NUMBER
					|| BoundsLocal[Axis] > Extent + KINDA_SMALL_NUMBER)
				{
					return false;
				}
				continue;
			}

			float Lower = (-Extent - BoundsLocal[Axis]) / Direction;
			float Upper = (Extent - BoundsLocal[Axis]) / Direction;
			if (Lower > Upper)
			{
				Swap(Lower, Upper);
			}
			MinTranslation = FMath::Max(MinTranslation, Lower);
			MaxTranslation = FMath::Min(MaxTranslation, Upper);
			if (MinTranslation > MaxTranslation + KINDA_SMALL_NUMBER)
			{
				return false;
			}
		}
	}

	const FVector AtomLocalCenter = (LocalMin + LocalMax) * 0.5f;
	const FVector AtomWorldCenter =
		InOutAtomWorld.TransformPosition(AtomLocalCenter);
	const float DesiredTranslation = FVector::DotProduct(
		BoundsTransform.GetLocation() - AtomWorldCenter,
		SafeWorldDirection);
	const float Translation = FMath::Clamp(
		DesiredTranslation,
		MinTranslation,
		MaxTranslation);
	InOutAtomWorld.AddToTranslation(SafeWorldDirection * Translation);
	return IsAtomTransformInsideLayoutBounds(AtomDefaults, InOutAtomWorld);
}

bool UNightCourseDirector::TryTranslateAtomYIntoLayoutBounds(
	const ANightCourseAtomActor* AtomDefaults,
	FTransform& InOutAtomWorld) const
{
	if (!bEnforceLayoutBounds || !LayoutBoundsComponent || !AtomDefaults)
	{
		return false;
	}

	const FTransform BoundsTransform =
		LayoutBoundsComponent->GetComponentTransform();
	const FVector BoundsExtent =
		LayoutBoundsComponent->GetScaledBoxExtent();
	if (BoundsExtent.IsNearlyZero())
	{
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

	// Only the world Y coordinate is a fallback degree of freedom. X and Z
	// remain untouched so the Atom keeps its intended longitudinal placement.
	const FVector BoundsLocalWorldY =
		BoundsTransform.InverseTransformVector(FVector(0.f, 1.f, 0.f));
	float MinWorldYTranslation = -TNumericLimits<float>::Max();
	float MaxWorldYTranslation = TNumericLimits<float>::Max();
	for (const FVector& LocalCorner : Corners)
	{
		const FVector BoundsLocal = BoundsTransform.InverseTransformPosition(
			InOutAtomWorld.TransformPosition(LocalCorner));
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Direction = BoundsLocalWorldY[Axis];
			const float Extent = BoundsExtent[Axis];
			if (FMath::Abs(Direction) <= KINDA_SMALL_NUMBER)
			{
				if (BoundsLocal[Axis] < -Extent - KINDA_SMALL_NUMBER
					|| BoundsLocal[Axis] > Extent + KINDA_SMALL_NUMBER)
				{
					return false;
				}
				continue;
			}

			float Lower = (-Extent - BoundsLocal[Axis]) / Direction;
			float Upper = (Extent - BoundsLocal[Axis]) / Direction;
			if (Lower > Upper)
			{
				Swap(Lower, Upper);
			}
			MinWorldYTranslation = FMath::Max(
				MinWorldYTranslation,
				Lower);
			MaxWorldYTranslation = FMath::Min(
				MaxWorldYTranslation,
				Upper);
			if (MinWorldYTranslation > MaxWorldYTranslation
				+ KINDA_SMALL_NUMBER)
			{
				return false;
			}
		}
	}

	const FVector AtomLocalCenter = (LocalMin + LocalMax) * 0.5f;
	const FVector AtomWorldCenter =
		InOutAtomWorld.TransformPosition(AtomLocalCenter);
	const float DesiredWorldYTranslation =
		BoundsTransform.GetLocation().Y - AtomWorldCenter.Y;
	const float WorldYTranslation = FMath::Clamp(
		DesiredWorldYTranslation,
		MinWorldYTranslation,
		MaxWorldYTranslation);
	InOutAtomWorld.AddToTranslation(FVector(0.f, WorldYTranslation, 0.f));
	return IsAtomTransformInsideLayoutBounds(AtomDefaults, InOutAtomWorld);
}

bool UNightCourseDirector::TryTranslateAtomIntoLayoutBounds(
	const ANightCourseAtomActor* AtomDefaults,
	FTransform& InOutAtomWorld) const
{
	if (!bEnforceLayoutBounds || !LayoutBoundsComponent || !AtomDefaults)
	{
		return false;
	}

	const FTransform BoundsTransform =
		LayoutBoundsComponent->GetComponentTransform();
	const FVector BoundsExtent =
		LayoutBoundsComponent->GetScaledBoxExtent();
	if (BoundsExtent.IsNearlyZero())
	{
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

	FVector BoundsLocalMin(
		TNumericLimits<float>::Max(),
		TNumericLimits<float>::Max(),
		TNumericLimits<float>::Max());
	FVector BoundsLocalMax(
		-TNumericLimits<float>::Max(),
		-TNumericLimits<float>::Max(),
		-TNumericLimits<float>::Max());
	for (const FVector& LocalCorner : Corners)
	{
		const FVector BoundsLocal = BoundsTransform.InverseTransformPosition(
			InOutAtomWorld.TransformPosition(LocalCorner));
		BoundsLocalMin.X = FMath::Min(BoundsLocalMin.X, BoundsLocal.X);
		BoundsLocalMin.Y = FMath::Min(BoundsLocalMin.Y, BoundsLocal.Y);
		BoundsLocalMin.Z = FMath::Min(BoundsLocalMin.Z, BoundsLocal.Z);
		BoundsLocalMax.X = FMath::Max(BoundsLocalMax.X, BoundsLocal.X);
		BoundsLocalMax.Y = FMath::Max(BoundsLocalMax.Y, BoundsLocal.Y);
		BoundsLocalMax.Z = FMath::Max(BoundsLocalMax.Z, BoundsLocal.Z);
	}

	FVector BoundsLocalTranslation = FVector::ZeroVector;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float LowerTranslation =
			-BoundsExtent[Axis] - BoundsLocalMin[Axis];
		const float UpperTranslation =
			BoundsExtent[Axis] - BoundsLocalMax[Axis];
		if (LowerTranslation > UpperTranslation + KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float DesiredTranslation =
			-(BoundsLocalMin[Axis] + BoundsLocalMax[Axis]) * 0.5f;
		BoundsLocalTranslation[Axis] = FMath::Clamp(
			DesiredTranslation,
			LowerTranslation,
			UpperTranslation);
	}

	InOutAtomWorld.AddToTranslation(
		BoundsTransform.TransformVector(BoundsLocalTranslation));
	return IsAtomTransformInsideLayoutBounds(AtomDefaults, InOutAtomWorld);
}

bool UNightCourseDirector::IsLocalArtBoundsInsideLayoutBounds(
	const FVector& LocalMin,
	const FVector& LocalMax,
	const FTransform& WorldTransform) const
{
	if (!bEnforceLayoutBounds || !LayoutBoundsComponent)
	{
		return true;
	}

	const FTransform BoundsTransform = LayoutBoundsComponent->GetComponentTransform();
	const FVector BoundsExtent = LayoutBoundsComponent->GetScaledBoxExtent();
	if (BoundsExtent.IsNearlyZero())
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] Layout Bounds component has zero extent."));
		return false;
	}

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
			WorldTransform.TransformPosition(LocalCorner));
		if (FMath::Abs(BoundsLocal.X) > BoundsExtent.X
			|| FMath::Abs(BoundsLocal.Y) > BoundsExtent.Y
			|| FMath::Abs(BoundsLocal.Z) > BoundsExtent.Z)
		{
			return false;
		}
	}
	return true;
}

bool UNightCourseDirector::TryTranslateLocalArtBoundsYIntoLayoutBounds(
	const FVector& LocalMin,
	const FVector& LocalMax,
	FTransform& InOutWorldTransform) const
{
	if (!bEnforceLayoutBounds || !LayoutBoundsComponent)
	{
		return false;
	}

	const FTransform BoundsTransform =
		LayoutBoundsComponent->GetComponentTransform();
	const FVector BoundsExtent =
		LayoutBoundsComponent->GetScaledBoxExtent();
	if (BoundsExtent.IsNearlyZero())
	{
		return false;
	}

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

	// Only the world Y coordinate is a fallback degree of freedom. X and Z
	// remain untouched so the fork keeps its intended longitudinal placement.
	const FVector BoundsLocalWorldY =
		BoundsTransform.InverseTransformVector(FVector(0.f, 1.f, 0.f));
	float MinWorldYTranslation = -TNumericLimits<float>::Max();
	float MaxWorldYTranslation = TNumericLimits<float>::Max();
	for (const FVector& LocalCorner : Corners)
	{
		const FVector BoundsLocal = BoundsTransform.InverseTransformPosition(
			InOutWorldTransform.TransformPosition(LocalCorner));
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Direction = BoundsLocalWorldY[Axis];
			const float Extent = BoundsExtent[Axis];
			if (FMath::Abs(Direction) <= KINDA_SMALL_NUMBER)
			{
				if (BoundsLocal[Axis] < -Extent - KINDA_SMALL_NUMBER
					|| BoundsLocal[Axis] > Extent + KINDA_SMALL_NUMBER)
				{
					return false;
				}
				continue;
			}

			float Lower = (-Extent - BoundsLocal[Axis]) / Direction;
			float Upper = (Extent - BoundsLocal[Axis]) / Direction;
			if (Lower > Upper)
			{
				Swap(Lower, Upper);
			}
			MinWorldYTranslation = FMath::Max(MinWorldYTranslation, Lower);
			MaxWorldYTranslation = FMath::Min(MaxWorldYTranslation, Upper);
			if (MinWorldYTranslation > MaxWorldYTranslation
				+ KINDA_SMALL_NUMBER)
			{
				return false;
			}
		}
	}

	const FVector LocalCenter = (LocalMin + LocalMax) * 0.5f;
	const FVector WorldCenter =
		InOutWorldTransform.TransformPosition(LocalCenter);
	const float DesiredWorldYTranslation =
		BoundsTransform.GetLocation().Y - WorldCenter.Y;
	const float WorldYTranslation = FMath::Clamp(
		DesiredWorldYTranslation,
		MinWorldYTranslation,
		MaxWorldYTranslation);
	InOutWorldTransform.AddToTranslation(FVector(0.f, WorldYTranslation, 0.f));
	return IsLocalArtBoundsInsideLayoutBounds(
		LocalMin,
		LocalMax,
		InOutWorldTransform);
}

AActor* UNightCourseDirector::AcquireRuntimeActor(UClass* ActorClass, const FTransform& Transform)
{
	if (!ActorClass)
	{
		return nullptr;
	}

	for (int32 Index = RuntimeActorPool.Num() - 1; Index >= 0; --Index)
	{
		AActor* Actor = RuntimeActorPool[Index];
		if (!IsValid(Actor))
		{
			RuntimeActorPool.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		if (Actor->GetClass() == ActorClass)
		{
			RuntimeActorPool.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			Actor->SetActorTransform(Transform);
			Actor->SetActorHiddenInGame(false);
			Actor->SetActorEnableCollision(true);
			Actor->SetActorTickEnabled(true);
			return Actor;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AActor>(ActorClass, Transform, Params);
}

void UNightCourseDirector::ReleaseRuntimeActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}
	// Blueprint runtime actors can initialize art in Construction Script/BeginPlay.
	// Those lifecycle events do not run when an Actor is reused, so pool only the
	// two native classes whose complete visible state is reapplied by SetupStone/
	// SetupBridge on every acquire.
	const bool bPoolableNativeActor =
		Actor->GetClass() == ANightCourseStoneActor::StaticClass()
		|| Actor->GetClass() == ANightBridgeSegmentActor::StaticClass();
	if (!bPoolableNativeActor)
	{
		Actor->Destroy();
		return;
	}
	if (!IsValid(Actor) || RuntimeActorPool.Contains(Actor))
	{
		return;
	}
	if (ANightCourseStoneActor* Stone = Cast<ANightCourseStoneActor>(Actor))
	{
		Stone->ClearFoe(false);
	}
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);
	RuntimeActorPool.Add(Actor);
}

void UNightCourseDirector::ResetRuntimeSpawnBudget()
{
	RuntimeSpawnBudgetRemaining = IsRuntimeActorStreamingEnabled() ? 4 : MAX_int32;
}

bool UNightCourseDirector::TryConsumeRuntimeSpawnBudget()
{
	if (RuntimeSpawnBudgetRemaining <= 0)
	{
		return false;
	}
	--RuntimeSpawnBudgetRemaining;
	return true;
}

void UNightCourseDirector::SpawnForkAtom(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World || !ForkAtomSpecs.IsValidIndex(Index))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnForkAtom] index=%d aborted: World='%s' forkAtomCount=%d."),
			Index,
			*GetNameSafe(World),
			ForkAtomSpecs.Num());
		return;
	}
	if (SpawnedForkAtoms.IsValidIndex(Index) && SpawnedForkAtoms[Index])
	{
		return;
	}

	const FNightForkAtomSpec& Spec = ForkAtomSpecs[Index];
	if (!Spec.ActorClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnForkAtom] index=%d pair=%d has no actor class."),
			Index,
			static_cast<int32>(Spec.ForkPair));
		return;
	}

	if (!TryConsumeRuntimeSpawnBudget())
	{
		return;
	}
	ANightCourseForkAtomActor* Actor = Cast<ANightCourseForkAtomActor>(AcquireRuntimeActor(Spec.ActorClass, Spec.WorldTransform));
	if (!Actor)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnForkAtom] index=%d pair=%d failed to spawn class='%s' location=%s."),
			Index,
			static_cast<int32>(Spec.ForkPair),
			*GetNameSafe(Spec.ActorClass),
			*Spec.WorldTransform.GetLocation().ToCompactString());
		return;
	}

	SpawnedForkAtoms[Index] = Actor;
	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("[NightCourse][Stage=SpawnForkAtom] index=%d pair=%d class='%s' location=%s."),
		Index,
		static_cast<int32>(Spec.ForkPair),
		*GetNameSafe(Spec.ActorClass),
		*Spec.WorldTransform.GetLocation().ToCompactString());
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
	if (SpawnedVisualActors.IsValidIndex(BindingIndex)
		&& SpawnedVisualActors[BindingIndex])
	{
		return;
	}

	const FNightAtomVisualBinding& Binding = VisualBindings[BindingIndex];
	if (!Binding.bIsBridge)
	{
		// Enemy landing points are spawned directly by SpawnStoneActor from
		// Config->FoeActorMap. Atom visual bindings own bridges only.
		return;
	}
	TSubclassOf<AActor> VisualClass = Binding.VisualPrefabClass;
	if (!VisualClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse] Bridge visual binding %d has no prefab class (bridge=%d)."),
			BindingIndex,
			Binding.BridgeIndex);
		return;
	}

	if (!VisualClass->IsChildOf(ANightBridgeSegmentActor::StaticClass()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse] Bridge visual binding %d resolves to a non-Bridge BP."),
			BindingIndex);
		return;
	}

	if (!TryConsumeRuntimeSpawnBudget())
	{
		return;
	}
	AActor* VisualActor = AcquireRuntimeActor(VisualClass.Get(), Binding.LocalTransform);
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

	if (BridgeSpecs.IsValidIndex(Binding.BridgeIndex))
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

void UNightCourseDirector::SetStoneFoeVisibility(int32 StoneIndex, bool bVisible)
{
	if (!StoneSpecs.IsValidIndex(StoneIndex)
		|| !SpawnedStones.IsValidIndex(StoneIndex)
		|| !SpawnedStones[StoneIndex])
	{
		return;
	}

	ANightCourseStoneActor* Stone = SpawnedStones[StoneIndex];
	if (bVisible)
	{
		Stone->ShowFoe();
	}
	else if (Stone->Spec.bHasFoe)
	{
		Stone->ClearFoe(false);
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
	if (SpawnedStones.IsValidIndex(Index) && SpawnedStones[Index])
	{
		return;
	}

	UClass* SpawnClass = ANightCourseStoneActor::StaticClass();
	if (StoneSpecs[Index].bHasFoe)
	{
		FString FoeError;
		SpawnClass = Config
			? Config->ResolveFoeActorClass(StoneSpecs[Index].FoeId, FoeError)
			: nullptr;
		if (!SpawnClass)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=SpawnStone] index=%d FoeId=%d has no valid mapped Blueprint: %s."),
				Index,
				static_cast<int32>(StoneSpecs[Index].FoeId),
				FoeError.IsEmpty() ? TEXT("Course Config is null.") : *FoeError);
			return;
		}
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[NightCourse] Foe M%02d stone=%d class=%s"),
			static_cast<int32>(StoneSpecs[Index].FoeId),
			Index,
			*GetNameSafe(SpawnClass));
	}

	const FRotator Facing = Config ? Config->TrackForward.Rotation() : FRotator::ZeroRotator;
	if (!TryConsumeRuntimeSpawnBudget())
	{
		return;
	}
	ANightCourseStoneActor* Stone = Cast<ANightCourseStoneActor>(
		AcquireRuntimeActor(SpawnClass, FTransform(Facing, GetStoneWorldLocation(Index))));
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

bool UNightCourseDirector::SpawnRoadsideActors()
{
	if (SpawnedRoadsideActors.Num() != RoadsideSpecs.Num())
	{
		SpawnedRoadsideActors.Init(nullptr, RoadsideSpecs.Num());
	}
	if (RoadsideSpecs.Num() == 0)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=SpawnRoadside] No World for %d roadside specs."),
			RoadsideSpecs.Num());
		return false;
	}

	const bool bStreaming = IsRuntimeActorStreamingEnabled();
	const float RoadsideVisibleDistance = Config
		? FMath::Max(1.f, Config->RuntimeRoadsideVisibleDistanceCm)
		: TNumericLimits<float>::Max();
	const float TrackOriginX = Config ? Config->TrackOrigin.X : 0.f;

	for (int32 Index = 0; Index < RoadsideSpecs.Num(); ++Index)
	{
		const FNightRoadsidePropSpec& Spec = RoadsideSpecs[Index];
		if (SpawnedRoadsideActors[Index])
		{
			continue;
		}
		if (bStreaming)
		{
			const float TrackDistance = StoneSpecs.IsValidIndex(Spec.ToStoneIndex)
				? StoneSpecs[Spec.ToStoneIndex].TrackDistance
				: Spec.WorldTransform.GetLocation().X - TrackOriginX;
			if (TrackDistance < ProgressDistance - KINDA_SMALL_NUMBER
				|| FMath::Abs(TrackDistance - ProgressDistance) > RoadsideVisibleDistance)
			{
				continue;
			}
		}
		if (!Spec.PropClass)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=SpawnRoadside] index=%d has no Blueprint class."),
				Index);
			for (ANightRoadsideSegmentActor* Spawned : SpawnedRoadsideActors)
			{
				if (Spawned)
				{
					Spawned->Destroy();
				}
			}
			SpawnedRoadsideActors.Reset();
			return false;
		}

		if (!TryConsumeRuntimeSpawnBudget())
		{
			break;
		}
		ANightRoadsideSegmentActor* Actor = Cast<ANightRoadsideSegmentActor>(
			AcquireRuntimeActor(Spec.PropClass, Spec.WorldTransform));
		if (!Actor)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=SpawnRoadside] index=%d failed to spawn class='%s'."),
				Index,
				*GetNameSafe(Spec.PropClass));
			for (ANightRoadsideSegmentActor* Spawned : SpawnedRoadsideActors)
			{
				if (Spawned)
				{
					Spawned->Destroy();
				}
			}
			SpawnedRoadsideActors.Reset();
			return false;
		}

		Actor->SetActorEnableCollision(false);
		SpawnedRoadsideActors[Index] = Actor;
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[NightCourse][Stage=SpawnRoadside] index=%d kind=%d side=%d class='%s' fromStone=%d toStone=%d."),
			Index,
			static_cast<int32>(Spec.Kind),
			Spec.Side,
			*GetNameSafe(Spec.PropClass),
			Spec.FromStoneIndex,
			Spec.ToStoneIndex);
	}
	return true;
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
	if (SpawnedBridges.IsValidIndex(Index) && SpawnedBridges[Index])
	{
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

	// A BridgeVisual component owns its own actor. If an authored visual is
	// absent, keep a native compatibility actor instead of consulting removed
	// Config bridge-class fallbacks.
	UClass* BridgeClass = ANightBridgeSegmentActor::StaticClass();
	if (!TryConsumeRuntimeSpawnBudget())
	{
		return;
	}
	ANightBridgeSegmentActor* Bridge = Cast<ANightBridgeSegmentActor>(AcquireRuntimeActor(
		BridgeClass, FTransform(FRotator(0.f, BridgeSpecs[Index].YawDeg, 0.f), BridgeSpecs[Index].WorldLocation)));
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
	for (ANightRoadsideSegmentActor* Roadside : SpawnedRoadsideActors)
	{
		if (Roadside)
		{
			ReleaseRuntimeActor(Roadside);
		}
	}
	for (AActor* Actor : SpawnedVisualActors)
	{
		if (Actor)
		{
			ReleaseRuntimeActor(Actor);
		}
	}
	for (ANightCourseForkAtomActor* ForkAtom : SpawnedForkAtoms)
	{
		if (ForkAtom)
		{
			ReleaseRuntimeActor(ForkAtom);
		}
	}
	for (ANightBridgeSegmentActor* Bridge : SpawnedBridges)
	{
		if (Bridge)
		{
			ReleaseRuntimeActor(Bridge);
		}
	}
	for (ANightCourseStoneActor* Stone : SpawnedStones)
	{
		if (Stone)
		{
			ReleaseRuntimeActor(Stone);
		}
	}
	SpawnedRoadsideActors.Reset();
	SpawnedVisualActors.Reset();
	SpawnedForkAtoms.Reset();
	SpawnedBridges.Reset();
	SpawnedStones.Reset();
}

void UNightCourseDirector::QueueSpawnedCourseActorsForDeferredDestroy()
{
	int32 QueuedCount = 0;
	auto QueueActor = [this, &QueuedCount](AActor* Actor)
	{
		if (!Actor || DeferredRuntimeActors.Contains(Actor))
		{
			return;
		}
		ReleaseRuntimeActor(Actor);
		++QueuedCount;
	};
	for (ANightRoadsideSegmentActor* Actor : SpawnedRoadsideActors)
	{
		QueueActor(Actor);
	}
	for (AActor* Actor : SpawnedVisualActors)
	{
		QueueActor(Actor);
	}
	for (ANightCourseForkAtomActor* Actor : SpawnedForkAtoms)
	{
		QueueActor(Actor);
	}
	for (ANightBridgeSegmentActor* Actor : SpawnedBridges)
	{
		QueueActor(Actor);
	}
	for (ANightCourseStoneActor* Actor : SpawnedStones)
	{
		QueueActor(Actor);
	}
	SpawnedRoadsideActors.Reset();
	SpawnedVisualActors.Reset();
	SpawnedForkAtoms.Reset();
	SpawnedBridges.Reset();
	SpawnedStones.Reset();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Streaming] returned %d old runtime actors to pool; pool=%d."),
		QueuedCount,
		RuntimeActorPool.Num());
}

void UNightCourseDirector::HideDeferredRuntimeActors()
{
	for (AActor* Actor : DeferredRuntimeActors)
	{
		if (Actor)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
	}
}
void UNightCourseDirector::DestroyDeferredRuntimeActors()
{
	// Destroy a bounded slice so route selection never pays the full cleanup
	// cost in one frame. The queue is force-cleared at FinishNight/ResetCourse.
	constexpr int32 DestroyBudgetPerFrame = 8;
	int32 DestroyedCount = 0;
	while (DeferredRuntimeActors.Num() > 0 && DestroyedCount < DestroyBudgetPerFrame)
	{
		AActor* Actor = DeferredRuntimeActors.Pop(EAllowShrinking::No);
		if (Actor)
		{
			Actor->Destroy();
			++DestroyedCount;
		}
	}
}

void UNightCourseDirector::ClearDeferredRuntimeActors()
{
	for (AActor* Actor : DeferredRuntimeActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	DeferredRuntimeActors.Reset();
}

bool UNightCourseDirector::SpawnCourseActors()
{
	SpawnedStones.Init(nullptr, StoneSpecs.Num());
	SpawnedBridges.Init(nullptr, BridgeSpecs.Num());
	SpawnedVisualActors.Init(nullptr, VisualBindings.Num());
	SpawnedForkAtoms.Init(nullptr, ForkAtomSpecs.Num());
	SpawnedRoadsideActors.Init(nullptr, RoadsideSpecs.Num());
	UE_LOG(
		LogTemp,
		Display,
	TEXT("[NightCourse][Stage=Spawn] Begin actors stones=%d bridges=%d visualBindings=%d forkAtoms=%d roadside=%d."),
		StoneSpecs.Num(),
		BridgeSpecs.Num(),
		VisualBindings.Num(),
		ForkAtomSpecs.Num(),
		RoadsideSpecs.Num());

	// Automation tests exercise the Director state machine without owning a
	// UWorld. Keep that supported as a headless composition mode; a real
	// runtime Host always has a world and therefore takes the spawn path below.
	if (!GetWorld())
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[NightCourse][Stage=Spawn] No World; skipping headless actor materialization."));
		return true;
	}

	const bool bStreaming = IsRuntimeActorStreamingEnabled();
	// Initial course materialization must be complete before the pawn is placed.
	RuntimeSpawnBudgetRemaining = MAX_int32;
	const int32 FirstKeepStone = bStreaming
		? GetRuntimeKeepFromStone()
		: 0;
	const int32 LastSpawnStone = GetRuntimeSpawnThroughStone();
	for (int32 Index = 0; Index < ForkAtomSpecs.Num(); ++Index)
	{
		SpawnForkAtom(Index);
	}
	for (int32 Index = 0; Index < BridgeSpecs.Num(); ++Index)
	{
		const FNightBridgeSpec& Bridge = BridgeSpecs[Index];
		if (!bStreaming
			|| Bridge.bForkConnectorVisualOnly
			|| (Bridge.ToStoneIndex >= FirstKeepStone
				&& Bridge.ToStoneIndex <= LastSpawnStone))
		{
			SpawnBridgeActor(Index);
		}
	}
	for (int32 Index = 0; Index < StoneSpecs.Num(); ++Index)
	{
		if (!bStreaming
			|| StoneSpecs[Index].bForkConnectorVisualOnly
			|| (Index >= FirstKeepStone && Index <= LastSpawnStone))
		{
			SpawnStoneActor(Index);
		}
	}
	for (int32 Index = 0; Index < VisualBindings.Num(); ++Index)
	{
		const FNightAtomVisualBinding& Binding = VisualBindings[Index];
		bool bShouldSpawn = !bStreaming || Binding.bForkConnectorVisualOnly;
		if (Binding.bIsBridge && BridgeSpecs.IsValidIndex(Binding.BridgeIndex))
		{
			const int32 ToStoneIndex = BridgeSpecs[Binding.BridgeIndex].ToStoneIndex;
			bShouldSpawn = bShouldSpawn || (!bStreaming
				|| (ToStoneIndex >= FirstKeepStone
					&& ToStoneIndex <= LastSpawnStone));
		}
		else if (Binding.StoneIndex != INDEX_NONE)
		{
			bShouldSpawn = bShouldSpawn || (!bStreaming
				|| (Binding.StoneIndex >= FirstKeepStone
					&& Binding.StoneIndex <= LastSpawnStone));
		}
		if (bShouldSpawn)
		{
			SpawnVisualBinding(Index);
		}
	}
	if (!SpawnRoadsideActors())
	{
		return false;
	}

	int32 MissingStoneActors = 0;
	for (int32 StoneIndex = 0; StoneIndex < SpawnedStones.Num(); ++StoneIndex)
	{
		const bool bExpected = !bStreaming
			|| (StoneSpecs.IsValidIndex(StoneIndex)
				&& (StoneSpecs[StoneIndex].bForkConnectorVisualOnly
					|| (StoneIndex >= FirstKeepStone
						&& StoneIndex <= LastSpawnStone)));
		if (bExpected && !SpawnedStones[StoneIndex])
		{
			++MissingStoneActors;
		}
	}
	int32 MissingForkAtomActors = 0;
	for (const ANightCourseForkAtomActor* ForkAtom : SpawnedForkAtoms)
	{
		MissingForkAtomActors += ForkAtom ? 0 : 1;
	}
	int32 MissingBridgeActors = 0;
	for (int32 BridgeIndex = 0; BridgeIndex < SpawnedBridges.Num(); ++BridgeIndex)
	{
		const bool bExpected = !bStreaming
			|| (BridgeSpecs.IsValidIndex(BridgeIndex)
				&& (BridgeSpecs[BridgeIndex].bForkConnectorVisualOnly
					|| (BridgeSpecs[BridgeIndex].ToStoneIndex >= FirstKeepStone
						&& BridgeSpecs[BridgeIndex].ToStoneIndex <= LastSpawnStone)));
		if (!bExpected)
		{
			continue;
		}
		if (SpawnedBridges[BridgeIndex])
		{
			continue;
		}

		// An authored Atom bridge is itself the runtime bridge actor and is
		// stored in SpawnedVisualActors rather than SpawnedBridges. Do not
		// report that valid actor as a missing native fallback.
		bool bHasAuthoredBridgeActor = false;
		for (int32 BindingIndex = 0;
			BindingIndex < VisualBindings.Num();
			++BindingIndex)
		{
			const FNightAtomVisualBinding& Binding = VisualBindings[BindingIndex];
			if (!Binding.bIsBridge || Binding.BridgeIndex != BridgeIndex)
			{
				continue;
			}
			bHasAuthoredBridgeActor =
				SpawnedVisualActors.IsValidIndex(BindingIndex)
				&& Cast<ANightBridgeSegmentActor>(
					SpawnedVisualActors[BindingIndex]) != nullptr;
			break;
		}
		MissingBridgeActors += bHasAuthoredBridgeActor ? 0 : 1;
	}
	int32 SpawnedVisualCount = 0;
	for (const AActor* Visual : SpawnedVisualActors)
	{
		SpawnedVisualCount += Visual ? 1 : 0;
	}
	const bool bSpawnSucceeded =
		MissingStoneActors == 0
		&& MissingForkAtomActors == 0
		&& MissingBridgeActors == 0;
	if (bSpawnSucceeded)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=Spawn] Complete stones=%d/%d forkAtoms=%d/%d bridges=%d/%d visuals=%d/%d missingStone=%d missingForkAtom=%d missingBridge=%d."),
			StoneSpecs.Num() - MissingStoneActors,
			StoneSpecs.Num(),
			ForkAtomSpecs.Num() - MissingForkAtomActors,
			ForkAtomSpecs.Num(),
			BridgeSpecs.Num() - MissingBridgeActors,
			BridgeSpecs.Num(),
			SpawnedVisualCount,
			VisualBindings.Num(),
			MissingStoneActors,
			MissingForkAtomActors,
			MissingBridgeActors);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Spawn] Complete stones=%d/%d forkAtoms=%d/%d bridges=%d/%d visuals=%d/%d missingStone=%d missingForkAtom=%d missingBridge=%d."),
			StoneSpecs.Num() - MissingStoneActors,
			StoneSpecs.Num(),
			ForkAtomSpecs.Num() - MissingForkAtomActors,
			ForkAtomSpecs.Num(),
			BridgeSpecs.Num() - MissingBridgeActors,
			BridgeSpecs.Num(),
			SpawnedVisualCount,
			VisualBindings.Num(),
			MissingStoneActors,
			MissingForkAtomActors,
			MissingBridgeActors);
	}
	return bSpawnSucceeded;
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
		|| Config->ForkHouseExclusionCm < 0.f
		|| Config->RuntimeSpawnAheadStoneCount <= 0
		|| Config->RuntimeUnloadBehindDistanceCm < 0.f
		|| Config->RuntimeRoadsideVisibleDistanceCm <= 0.f
		|| Config->BranchSpawnBatchSize <= 0
		|| Config->DefaultKeySwapWarningSeconds < 0.f
		|| Config->DefaultKeySwapSafetySeconds < 0.f)
	{
		OutError = TEXT("Course Config contains a negative value or non-positive branch gap.");
		return FailValidation();
	}
	if (Config->DefaultFoeId == EFoeId::None)
	{
		OutError = TEXT("Course Config DefaultFoeId must be a mapped foe ID.");
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
	if (!Config->ValidateFoeActorMap(OutError))
	{
		return FailValidation();
	}
	if (!Config->ValidateFoeDropMap(OutError))
	{
		return FailValidation();
	}
	if (!Config->ValidateForkAtomMap(OutError))
	{
		return FailValidation();
	}
	if (!Config->ValidateRoadsideConfiguration(OutError))
	{
		return FailValidation();
	}
	TSet<EFoeId> RequiredFoeIds;
	if (Config->DefaultFoeId != EFoeId::None)
	{
		RequiredFoeIds.Add(Config->DefaultFoeId);
	}
	for (const EFoeId FoeId : Config->FoeWeightPool)
	{
		if (FoeId != EFoeId::None)
		{
			RequiredFoeIds.Add(FoeId);
		}
	}
	for (const EFoeId FoeId : ActiveBootstrap.FoeWeightOverride)
	{
		if (FoeId != EFoeId::None)
		{
			RequiredFoeIds.Add(FoeId);
		}
	}
	for (const EFoeId FoeId : RequiredFoeIds)
	{
		FString FoeError;
		if (!Config->ResolveFoeActorClass(FoeId, FoeError))
		{
			OutError = FoeError;
			return FailValidation();
		}
	}

	const int32 BaseRouteLength = ResolveMainRouteAtomCount(
		Config->CourseRuleData->GetRouteModeLength(ActiveDefaultRoute));
	if (BaseRouteLength <= 0)
	{
		OutError = FString::Printf(
			TEXT("RouteModes has no usable queue for default route %d."),
			static_cast<int32>(ActiveDefaultRoute));
		return FailValidation();
	}
	// RouteModes.TargetAtomCount is authoritative. The CourseConfig field is
	// retained only as a legacy fallback for old level instances.
	const int32 ForkIndex = ActiveBootstrap.bUseCourseQueueOverride
		? BaseRouteLength
		: (Config->ForkAfterBaseAtomIndex != INDEX_NONE
			? Config->ForkAfterBaseAtomIndex
			: BaseRouteLength);
	if (ForkIndex != INDEX_NONE
		&& (ForkIndex <= 0 || ForkIndex > BaseRouteLength))
	{
		OutError = FString::Printf(
			TEXT("ForkAfterBaseAtomIndex=%d is invalid for base route length %d."),
			ForkIndex,
			BaseRouteLength);
		return FailValidation();
	}
	if (IsForkEnabledForActiveCourse()
		&& ForkIndex != INDEX_NONE
		&& Config->CourseRuleData->BranchRoutes.Num() == 0)
	{
		OutError = TEXT("ForkAfterBaseAtomIndex is set but no branch Atom queues are authored.");
		return FailValidation();
	}
	if (IsForkEnabledForActiveCourse() && Config->CourseRuleData->BranchRoutes.Num() > 0)
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
	if (IsForkEnabledForActiveCourse() && bRuleBranch)
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
		TEXT("[NightCourse][Stage=Validate] OK defaultRoute=%d routeModeLength=%d branchRoutes=%d forkIndex=%d."),
		static_cast<int32>(ActiveDefaultRoute),
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
		TEXT("[NightCourse][Stage=TryStart] request Config='%s' running=%d Level=%d DefaultRoute=%d Seed=%d ForkPair=%d."),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		bRunning ? 1 : 0,
		static_cast<int32>(Bootstrap.LevelId),
		static_cast<int32>(Bootstrap.DefaultRoute),
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
		TEXT("[NightCourse][Stage=Start] begin Config='%s' Level=%d DefaultRoute=%d Seed=%d ForkPair=%d."),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		static_cast<int32>(Bootstrap.LevelId),
		static_cast<int32>(Bootstrap.DefaultRoute),
		Bootstrap.Seed,
		static_cast<int32>(Bootstrap.ForkPair));
	if (!Config)
	{
		BeginFailure(TEXT("StartNight failed: Config is null."));
		return;
	}

	ClearSpawnedCourseActors();
	ClearDeferredRuntimeActors();
	ActiveBootstrap = Bootstrap;
	if (Bootstrap.DefaultRoute == ENightRouteId::None)
	{
		BeginFailure(TEXT("StartNight failed: DefaultRoute must be A, B or C."));
		return;
	}
	ActiveDefaultRoute = Bootstrap.DefaultRoute;
	ActiveBootstrap.DefaultRoute = ActiveDefaultRoute;
	ActiveForkPair = Bootstrap.ForkPair;
	AuthoredKeySwapCues = Config->KeySwapCues;
	for (const FNightLevelCourseRule& LevelRule : Config->LevelRules)
	{
		if (LevelRule.LevelId != Bootstrap.LevelId)
		{
			continue;
		}
		if (LevelRule.bUseKeySwapCues)
		{
			AuthoredKeySwapCues = LevelRule.KeySwapCues;
		}
		break;
	}
	CurrentRoute = ENightRouteId::None;
	CourseWorldOffset = FVector::ZeroVector;
	bBranchSelected = false;
	bSpareLampConsumed = false;
	RemainingGiftShieldCharges = FMath::Max(0, ActiveBootstrap.GiftBuffs.MatchShieldCharges);
	GiftDashInvulnerableEndTime = 0.f;
	bNearDeathGiftConsumed = false;
	bBranchTransitionConsumed = false;
	bBranchRemainderLoaded = false;
	bBranchRemainderPreGenerated = false;
	BranchBeatCount = 0;
	BaseBeatCount = 0;
	BranchTransitionBeatIndex = INDEX_NONE;
	NextKeySwapCueIndex = 0;
	BranchEnterBufferEndTime = 0.f;
	KeySwapEndTime = 0.f;
	ActiveKeySwapCues = AuthoredKeySwapCues;
	bHasActiveRouteRule = false;
	PreparedBranchRoutes.Reset();
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
		TEXT("[NightCourse][Stage=Start] resolved runtimeSeed=%d defaultRoute=%d activeForkPair=%d levelRules=%d."),
		RuntimeSeed,
		static_cast<int32>(ActiveDefaultRoute),
		static_cast<int32>(ActiveForkPair),
		Config->LevelRules.Num());
		if (INightFeelBridge* Feel = GetFeel())
	{
		INightFeelBridge::Execute_SetControlScheme(
			FeelBridgeObject,
			ENightControlScheme::Normal);

		// A failed Night is retried in the same map. Refill the transient Feel
		// state here so a Soul-zero failure does not immediately fail again.
		const float StartingSoul = FMath::Max(0.f, Config->StartingSoul);
		const float CurrentSoul = INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
		if (CurrentSoul < StartingSoul)
		{
			INightFeelBridge::Execute_RestoreSoul(
				FeelBridgeObject,
				StartingSoul - CurrentSoul,
				StartingSoul);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=Start] Restored Soul for a fresh Night: %.1f -> %.1f."),
				CurrentSoul,
				INightFeelBridge::Execute_GetSoul(FeelBridgeObject));
		}
	}
	FString ValidationError;
	if (!ValidateConfiguration(ValidationError))
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse] StartNight configuration invalid: %s"), *ValidationError);
		BeginFailure(ValidationError);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("[NightCourse][Stage=Start] configuration validation passed."));
	ApplyDefaultCoursePostProcessMaterial();

	ENightRouteId LeftRoute = ENightRouteId::None;
	ENightRouteId RightRoute = ENightRouteId::None;
	bool bForcedAB = false;
	UNightForkController::ResolvePairRoutes(
		ActiveForkPair,
		LeftRoute,
		RightRoute,
		bForcedAB);
	bForkPending = false;
	if (IsForkEnabledForActiveCourse()
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
		TEXT("[NightCourse][Stage=Start] composition passed stones=%d beats=%d bridges=%d visuals=%d forkAtoms=%d."),
		StoneSpecs.Num(),
		BeatSpecs.Num(),
		BridgeSpecs.Num(),
		VisualBindings.Num(),
		ForkAtomSpecs.Num());

	bRunning = true;
	bDidEnterRuntimeCourse = true;
	ElapsedSeconds = 0.f;
	CurrentStoneIndex = 0;
	// Runtime streaming uses ProgressDistance while spawning its first window.
	// Seed it from the first composed stone so all distance gates share one origin.
	ProgressDistance = StoneSpecs.IsValidIndex(0)
		? StoneSpecs[0].TrackDistance
		: 0.f;
	if (RemainingGiftShieldCharges > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("[Gift][BlessedAmulet] Match shield armed with %d charge(s)."), RemainingGiftShieldCharges);
	}
	if (ActiveBootstrap.GiftBuffs.PreForkGatherAmountBonus > 0.f)
	{
		UE_LOG(LogTemp, Display, TEXT("[Gift][WindfallWealth] Pre-fork ingredient amount bonus active: +%.0f%%."), ActiveBootstrap.GiftBuffs.PreForkGatherAmountBonus * 100.f);
	}
	if (ActiveBootstrap.GiftBuffs.NearDeathHealAmount > 0.f)
	{
		UE_LOG(LogTemp, Display, TEXT("[Gift][WildMilk] Near-death heal armed: +%.1f Soul at <= %.1f."), ActiveBootstrap.GiftBuffs.NearDeathHealAmount, ActiveBootstrap.GiftBuffs.NearDeathThreshold);
	}
	ActiveBeatIndex = INDEX_NONE;
	bWindowOpen = false;
	bAdvancing = false;
	CollectedIngredients.Reset();
	BranchCollectedIngredients.Reset();
	BeatConsumed.Init(0, BeatSpecs.Num());
	BaseBeatCount = BeatSpecs.Num();
	UE_LOG(LogTemp, Display, TEXT("[NightCourse][Stage=Start] spawning runtime actors."));
	if (!SpawnCourseActors())
	{
		BeginFailure(TEXT("Runtime actor spawning failed; verify FoeActorMap, fork Atom mappings and bridge bindings."));
		return;
	}

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

bool UNightCourseDirector::BuildPreparedBranchRoute(
	ENightRouteId RouteId,
	FNightPreparedBranchRoute& OutPrepared,
	FString& OutError)
{
	OutPrepared = FNightPreparedBranchRoute();
	OutError.Reset();
	if (!Config || !Config->RouteRules || RouteId == ENightRouteId::None)
	{
		OutError = TEXT("A valid Config, RouteRules asset and route are required to prepare a branch.");
		return false;
	}

	const ENightRouteId PreviousRoute = CurrentRoute;
	CurrentRoute = RouteId;
	TArray<FNightStoneSpec> NewStones;
	TArray<FNightBeatSpec> NewBeats;
	TArray<FNightBridgeSpec> NewBridges;
	TArray<FNightAtomVisualBinding> NewVisualBindings;
	TArray<FNightForkAtomSpec> NewForkAtomSpecs;
	const bool bBuilt = BuildCourseForPreview(
		NewStones,
		NewBeats,
		NewBridges,
		NewVisualBindings,
		NewForkAtomSpecs);
	CurrentRoute = PreviousRoute;
	if (!bBuilt)
	{
		OutError = FString::Printf(
			TEXT("Could not prepare route %d before fork selection."),
			static_cast<int32>(RouteId));
		return false;
	}

	// The Atom composer already anchors the first branch Atom to the selected fork exit.
	// Keep the shared pre-fork course in authored world space; globally moving it causes
	// a visible scene/pawn jump when the selected route is installed.
	CourseWorldOffset = FVector::ZeroVector;
	TArray<FNightRoadsidePropSpec> NewRoadsideSpecs;
	if (!BuildRoadsideSpecs(
		NewStones,
		NewBridges,
		NewForkAtomSpecs,
		NewRoadsideSpecs))
	{
		OutError = FString::Printf(
			TEXT("Could not prepare roadside props for route %d."),
			static_cast<int32>(RouteId));
		return false;
	}
	if (NewBeats.Num() <= BaseBeatCount)
	{
		OutError = FString::Printf(
			TEXT("Prepared route %d did not append a transition and branch segment."),
			static_cast<int32>(RouteId));
		return false;
	}

	FNightRouteRuleRow NewRouteRule;
	if (!Config->RouteRules->TryGetRule(RouteId, NewRouteRule))
	{
		OutError = FString::Printf(
			TEXT("RouteRules has no authored row for prepared route %d."),
			static_cast<int32>(RouteId));
		return false;
	}

	OutPrepared.bValid = true;
	OutPrepared.Stones = MoveTemp(NewStones);
	OutPrepared.Beats = MoveTemp(NewBeats);
	OutPrepared.Bridges = MoveTemp(NewBridges);
	OutPrepared.VisualBindings = MoveTemp(NewVisualBindings);
	OutPrepared.ForkAtoms = MoveTemp(NewForkAtomSpecs);
	OutPrepared.RoadsideSpecs = MoveTemp(NewRoadsideSpecs);
	OutPrepared.RouteRule = NewRouteRule;
	OutPrepared.CourseWorldOffset = FVector::ZeroVector;
	return true;
}

bool UNightCourseDirector::PrepareBranchRoutesForFork(FString& OutError)
{
	OutError.Reset();
	PreparedBranchRoutes.Reset();

	ENightRouteId LeftRoute = ENightRouteId::None;
	ENightRouteId RightRoute = ENightRouteId::None;
	bool bForcedAB = false;
	UNightForkController::ResolvePairRoutes(
		ActiveForkPair,
		LeftRoute,
		RightRoute,
		bForcedAB);
	(void)bForcedAB;

	for (const ENightRouteId RouteId : {LeftRoute, RightRoute})
	{
		FNightPreparedBranchRoute Prepared;
		if (!BuildPreparedBranchRoute(RouteId, Prepared, OutError))
		{
			PreparedBranchRoutes.Reset();
			return false;
		}
		PreparedBranchRoutes.Add(RouteId, MoveTemp(Prepared));
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=ForkPrepare] prepared left=%d right=%d before showing fork choice."),
		static_cast<int32>(LeftRoute),
		static_cast<int32>(RightRoute));
	return true;
}

void UNightCourseDirector::BeginBranchRoutePreparation()
{
	PreparedBranchRoutes.Reset();
	BranchRoutePreparationOrder.Reset();
	NextBranchRoutePreparationIndex = 0;
	bBranchRoutePreparationActive = false;
	bBranchSelectionPending = false;
	PendingBranchRoute = ENightRouteId::None;

	ENightRouteId LeftRoute = ENightRouteId::None;
	ENightRouteId RightRoute = ENightRouteId::None;
	bool bForcedAB = false;
	UNightForkController::ResolvePairRoutes(
		ActiveForkPair,
		LeftRoute,
		RightRoute,
		bForcedAB);
	(void)bForcedAB;
	for (const ENightRouteId RouteId : {LeftRoute, RightRoute})
	{
		if (RouteId != ENightRouteId::None
			&& !BranchRoutePreparationOrder.Contains(RouteId))
		{
			BranchRoutePreparationOrder.Add(RouteId);
		}
	}

	bBranchRoutePreparationActive = BranchRoutePreparationOrder.Num() > 0;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=ForkPrepare] started incremental preparation routes=%d; fork choice is responsive."),
		BranchRoutePreparationOrder.Num());
}

bool UNightCourseDirector::PrepareNextBranchRoute(FString& OutError)
{
	OutError.Reset();
	if (!bBranchRoutePreparationActive)
	{
		return true;
	}
	if (!BranchRoutePreparationOrder.IsValidIndex(NextBranchRoutePreparationIndex))
	{
		bBranchRoutePreparationActive = false;
		return true;
	}

	const ENightRouteId RouteId =
		BranchRoutePreparationOrder[NextBranchRoutePreparationIndex];
	FNightPreparedBranchRoute Prepared;
	const double PrepareStartSeconds = FPlatformTime::Seconds();
	if (!BuildPreparedBranchRoute(RouteId, Prepared, OutError))
	{
		PreparedBranchRoutes.Reset();
		bBranchRoutePreparationActive = false;
		return false;
	}
	PreparedBranchRoutes.Add(RouteId, MoveTemp(Prepared));
	++NextBranchRoutePreparationIndex;
	const double PrepareMilliseconds =
		(FPlatformTime::Seconds() - PrepareStartSeconds) * 1000.0;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=ForkPrepare] route=%d prepared in %.2f ms (%d/%d)."),
		static_cast<int32>(RouteId),
		PrepareMilliseconds,
		NextBranchRoutePreparationIndex,
		BranchRoutePreparationOrder.Num());

	if (NextBranchRoutePreparationIndex >= BranchRoutePreparationOrder.Num())
	{
		bBranchRoutePreparationActive = false;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=ForkPrepare] incremental preparation complete; cached routes=%d."),
			PreparedBranchRoutes.Num());
	}
	return true;
}

void UNightCourseDirector::ProcessBranchRoutePreparation()
{
	if (!bBranchRoutePreparationActive)
	{
		return;
	}

	FString PrepareError;
	if (!PrepareNextBranchRoute(PrepareError))
	{
		BeginFailure(PrepareError.IsEmpty()
			? TEXT("Incremental branch route preparation failed.")
			: PrepareError);
		return;
	}
	if (bBranchRoutePreparationActive || !bBranchSelectionPending)
	{
		return;
	}

	CurrentRoute = PendingBranchRoute;
	bBranchSelectionPending = false;
	PendingBranchRoute = ENightRouteId::None;
	FString BuildError;
	if (!RebuildCourseForSelectedRoute(BuildError))
	{
		BeginFailure(BuildError);
	}
}

bool UNightCourseDirector::InstallPreparedBranchRoute(
	FNightPreparedBranchRoute&& Prepared,
	int32 PreviousStoneIndex,
	float PreviousProgressDistance,
	const TArray<FNightStoneSpec>& PreviousStones,
	FString& OutError)
{
	OutError.Reset();
	if (!Prepared.bValid || Prepared.Beats.Num() <= BaseBeatCount)
	{
		OutError = TEXT("Prepared branch route is invalid or missing its transition segment.");
		return false;
	}

	const int32 SharedStoneCount = FMath::Min(
		PreviousStoneIndex + 1,
		Prepared.Stones.Num());
	for (int32 Index = 0; Index < SharedStoneCount; ++Index)
	{
		if (PreviousStones.IsValidIndex(Index) && PreviousStones[Index].bHasFoe)
		{
			Prepared.Stones[Index].bHasFoe = true;
			Prepared.Stones[Index].FoeId = PreviousStones[Index].FoeId;
		}
		else
		{
			Prepared.Stones[Index].bHasFoe = false;
			Prepared.Stones[Index].FoeId = EFoeId::None;
		}
	}

	QueueSpawnedCourseActorsForDeferredDestroy();
	StoneSpecs = MoveTemp(Prepared.Stones);
	BeatSpecs = MoveTemp(Prepared.Beats);
	BridgeSpecs = MoveTemp(Prepared.Bridges);
	VisualBindings = MoveTemp(Prepared.VisualBindings);
	ForkAtomSpecs = MoveTemp(Prepared.ForkAtoms);
	RoadsideSpecs = MoveTemp(Prepared.RoadsideSpecs);
	ActiveRouteRule = Prepared.RouteRule;
	bHasActiveRouteRule = true;
	CourseWorldOffset = Prepared.CourseWorldOffset;
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
	bBranchRemainderLoaded = false;
	bBranchRemainderPreGenerated = false;
	bForkPending = false;
	BranchBeatCount = 0;
	CurrentStoneIndex = FMath::Clamp(
		PreviousStoneIndex,
		0,
		FMath::Max(0, StoneSpecs.Num() - 1));
	ProgressDistance = FMath::Max(
		PreviousProgressDistance,
		StoneSpecs.IsValidIndex(PreviousStoneIndex)
			? StoneSpecs[PreviousStoneIndex].TrackDistance
			: PreviousProgressDistance);
	ActiveBeatIndex = INDEX_NONE;
	bWindowOpen = false;
	bAdvancing = false;

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

	ApplyCoursePostProcessMaterial(
		ActiveRouteRule.PostProcessMaterial
			? ActiveRouteRule.PostProcessMaterial.Get()
			: Config->DefaultPostProcessMaterial.Get());

	if (!SpawnCourseActors())
	{
		OutError = TEXT("Prepared branch runtime actor spawning failed; verify branch bindings.");
		return false;
	}
	// The shared pre-fork stone remains in place; do not teleport the runner during route install.
	HideDeferredRuntimeActors();
	BranchEnterBufferEndTime =
		ElapsedSeconds + FMath::Max(0.f, Config->BranchEnterBufferSeconds);
	SetPhase(ENightCoursePhase::BranchEnterBuffer);
	// 提前打开分支余量流式生成窗口：让分支首石之后的 atom 在 BranchEnterBuffer
	// 期间就生成并可见，主角到达时地面已经存在（消除“只有衔接处一个”的卡顿 / 悬空）。
	bBranchRemainderPreGenerated = true;
	UpdateRouteVisibility();
	return true;
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
	// Build only the route selected by the player. Pre-generating both branch
	// routes here allowed an unchosen route's LayoutBounds failure to abort the
	// whole course before the fork choice was even resolved.
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
	if (bBranchRoutePreparationActive)
	{
		bBranchSelectionPending = true;
		PendingBranchRoute = RouteTaken;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=ForkResolve] route=%d queued until incremental preparation completes (timedOut=%d)."),
			static_cast<int32>(RouteTaken),
			bTimedOut ? 1 : 0);
		return;
	}
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
				return ResolveForkRouteAtomCount(
					Queue->TargetAtomCount > 0
						? Queue->TargetAtomCount
						: Queue->Atoms.Num());
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

	if (FNightPreparedBranchRoute* CachedRoute = PreparedBranchRoutes.Find(CurrentRoute))
	{
		FNightPreparedBranchRoute Prepared = MoveTemp(*CachedRoute);
		PreparedBranchRoutes.Remove(CurrentRoute);
		return InstallPreparedBranchRoute(
			MoveTemp(Prepared),
			PreviousStoneIndex,
			PreviousProgressDistance,
			PreviousStones,
			OutError);
	}

	TArray<FNightStoneSpec> NewStones;
	TArray<FNightBeatSpec> NewBeats;
	TArray<FNightBridgeSpec> NewBridges;
	TArray<FNightAtomVisualBinding> NewVisualBindings;
	TArray<FNightForkAtomSpec> NewForkAtomSpecs;
	TArray<FNightRoadsidePropSpec> NewRoadsideSpecs;
	if (!BuildCourseForPreview(
		NewStones,
		NewBeats,
		NewBridges,
		NewVisualBindings,
		NewForkAtomSpecs))
	{
		OutError = TEXT("Selected branch composition failed; no partial branch was installed.");
		return false;
	}

	// The Atom composer already anchors the first branch Atom to the selected fork exit.
	// Keep the shared pre-fork course in authored world space; globally moving it causes
	// a visible scene/pawn jump when the selected route is installed.
	CourseWorldOffset = FVector::ZeroVector;
	if (!BuildRoadsideSpecs(
		NewStones,
		NewBridges,
		NewForkAtomSpecs,
		NewRoadsideSpecs))
	{
		OutError = TEXT("Selected branch roadside composition failed; no partial branch was installed.");
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

	const int32 SharedStoneCount = FMath::Min(
		PreviousStoneIndex + 1,
		NewStones.Num());
	for (int32 Index = 0; Index < SharedStoneCount; ++Index)
	{
		// Preserve the shared base foe state when the selected route rebuilds
		// the course. Do not include the visual-only fork connector stones
		// appended after PreviousStoneIndex; they are replaced by the selected
		// branch and must keep the branch planner's newly resolved foe state.
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
	ForkAtomSpecs = MoveTemp(NewForkAtomSpecs);
	RoadsideSpecs = MoveTemp(NewRoadsideSpecs);
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
	bBranchRemainderLoaded = false;
	bBranchRemainderPreGenerated = false;
	bForkPending = false;
	BranchBeatCount = 0;
	CurrentStoneIndex = FMath::Clamp(
		PreviousStoneIndex,
		0,
		FMath::Max(0, StoneSpecs.Num() - 1));
	ProgressDistance = FMath::Max(
		PreviousProgressDistance,
		StoneSpecs.IsValidIndex(PreviousStoneIndex)
			? StoneSpecs[PreviousStoneIndex].TrackDistance
			: PreviousProgressDistance);
	ActiveBeatIndex = INDEX_NONE;
	bWindowOpen = false;
	bAdvancing = false;

	ActiveRouteRule = NewRouteRule;
	bHasActiveRouteRule = true;
	ApplyCoursePostProcessMaterial(
		ActiveRouteRule.PostProcessMaterial
			? ActiveRouteRule.PostProcessMaterial.Get()
			: Config->DefaultPostProcessMaterial.Get());

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

	if (!SpawnCourseActors())
	{
		OutError = TEXT("Branch runtime actor spawning failed; verify FoeActorMap, fork Atom mappings and bridge bindings.");
		return false;
	}
	// The shared pre-fork stone remains in place; do not teleport the runner during route install.
	HideDeferredRuntimeActors();
	BranchEnterBufferEndTime =
		ElapsedSeconds + FMath::Max(0.f, Config->BranchEnterBufferSeconds);
	SetPhase(ENightCoursePhase::BranchEnterBuffer);
	// 提前打开分支余量流式生成窗口：让分支首石之后的 atom 在 BranchEnterBuffer
	// 期间就生成并可见，主角到达时地面已经存在（消除“只有衔接处一个”的卡顿 / 悬空）。
	bBranchRemainderPreGenerated = true;
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
		const FNightStoneSpec& TargetStone = StoneSpecs[Beat.ToStoneIndex];
		if (!TargetStone.bHasFoe)
		{
			BeginFailure(TEXT("Enemy beat target was composed without a mapped foe actor."));
			return;
		}
		SetStoneFoeVisibility(Beat.ToStoneIndex, true);
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
		&& !bSpareLampConsumed
		&& ElapsedSeconds >= GiftDashInvulnerableEndTime;
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
			ApplyGiftAwareSoulPenalty(
				BasePenalty * RoutePenaltyScale,
				Outcome,
				true,
				true,
				TEXT("WrongInput"));
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
		&& !bSpareLampConsumed
		&& ElapsedSeconds >= GiftDashInvulnerableEndTime;
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
				ApplyGiftAwareSoulPenalty(
					Penalty * RoutePenaltyScale,
					Outcome,
					true,
					true,
					Outcome == ENightJudgeOutcome::Miss
						? TEXT("Miss")
						: TEXT("WrongInput"));
			}
			INightFeelBridge::Execute_PlayFailFeedback(FeelBridgeObject, Outcome, Beat.Action);
		}
	}

	const bool bAttackBeat = (Beat.Action == ENightNodeKind::Enemy);
	if (Outcome == ENightJudgeOutcome::Success && bAttackBeat && StoneSpecs.IsValidIndex(Beat.ToStoneIndex))
	{
		EIngredientId DropId = StoneSpecs[Beat.ToStoneIndex].DropId;
		int32 DropCount = StoneSpecs[Beat.ToStoneIndex].DropCount;
		EIngredientId MappedDropId = EIngredientId::None;
		const bool bHasMappedFoeDrop =
			Config
			&& StoneSpecs[Beat.ToStoneIndex].bHasFoe
			&& Config->TryGetFoeDropId(
				StoneSpecs[Beat.ToStoneIndex].FoeId,
				MappedDropId);
		if (bHasMappedFoeDrop)
		{
			DropId = MappedDropId;
			DropCount = Config->DefaultDropCount;
		}
		if (bBranchBeat && bHasActiveRouteRule)
		{
			if (!Config->bDropIngredientOnEveryEnemyKill)
			{
				const int32 Rhythm = FMath::Max(1, ActiveRouteRule.DropRhythmEveryN);
				if ((BranchBeatCount % Rhythm) != 0)
				{
					DropCount = 0;
				}
				if (!bHasMappedFoeDrop && ActiveRouteRule.DropCycle.Num() > 0)
				{
					DropId = ActiveRouteRule.DropCycle[
						(BranchBeatCount - 1) % ActiveRouteRule.DropCycle.Num()];
				}
			}
			DropCount *= FMath::Max(1, ActiveRouteRule.BranchDropCountMul);
		}
		AddDrop(DropId, DropCount);
#pragma region K2 moonyfli
		if (RunnerPawn)
		{
			if (APlayerController* PC = Cast<APlayerController>(RunnerPawn->GetController()))
			{
				if (ANightCourseHUD* NightHUD = Cast<ANightCourseHUD>(PC->GetHUD()))
				{
					NightHUD->NotifyFoeKilled(
						StoneSpecs[Beat.ToStoneIndex].FoeId,
						DropId != EIngredientId::None && DropCount > 0);
				}
			}
		}
#pragma endregion K2 moonyfli
		if (SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
		{
#pragma region K2 moonyfli
			// Read the foe visual before ClearFoe hides it: the HUD flight starts at the kill point.
			ANightCourseStoneActor* DropStone = SpawnedStones[Beat.ToStoneIndex];
			FVector DropWorldLocation = DropStone->GetActorLocation();
			if (DropStone->FoeSkeletalMeshComponent
				&& DropStone->FoeSkeletalMeshComponent->IsVisible())
			{
				DropWorldLocation =
					DropStone->FoeSkeletalMeshComponent->GetComponentLocation();
			}
			else if (DropStone->FoeCapsule && DropStone->FoeCapsule->IsVisible())
			{
				DropWorldLocation = DropStone->FoeCapsule->GetComponentLocation();
			}
#pragma endregion K2 moonyfli
			SpawnedStones[Beat.ToStoneIndex]->ClearFoe(true);
			SpawnedStones[Beat.ToStoneIndex]->PlayDropBurst(
				DropId,
				DropCount);
#pragma region K2 moonyfli
			if (DropId != EIngredientId::None && DropCount > 0)
			{
				OnIngredientDropped.Broadcast(DropId, DropCount, DropWorldLocation);
			}
#pragma endregion K2 moonyfli
		}
		StoneSpecs[Beat.ToStoneIndex].bHasFoe = false;
		SetStoneFoeVisibility(Beat.ToStoneIndex, false);
	}
	else if (Outcome != ENightJudgeOutcome::Success && bAttackBeat
		&& SpawnedStones.IsValidIndex(Beat.ToStoneIndex) && SpawnedStones[Beat.ToStoneIndex])
	{
		// Wrong/Miss still advances onto the stone but foe can stay or clear — clear to keep chain readable.
		SpawnedStones[Beat.ToStoneIndex]->ClearFoe(false);
		StoneSpecs[Beat.ToStoneIndex].bHasFoe = false;
		SetStoneFoeVisibility(Beat.ToStoneIndex, false);
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

	const float TargetDistance = StoneSpecs[StoneIndex].TrackDistance;
	if (TargetDistance < ProgressDistance - KINDA_SMALL_NUMBER)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Advance] rejected backward X movement: stone=%d targetX=%.2f progressX=%.2f."),
			StoneIndex,
			TargetDistance,
			ProgressDistance);
		BeginFailure(TEXT("Route X regressed; backward movement was blocked."));
		return;
	}
	bAdvancing = true;
	CurrentStoneIndex = StoneIndex;
	AdvanceTargetDistance = TargetDistance;
	if (RunnerPawn)
	{
		// 镜头调度：把"真实行进方向"从仅岔路过渡推广到整条路线。
		// 每个石间推进都用 起点→目标石 的真实朝向，使挂在 pawn 上的 SpringArm 镜头
		// 随动作/转向微微旋转、跟随，而不是始终朝向固定全局 TrackForward。
		// 速度仍由 bAnimDrivenAdvance 按动画锚点反推；仅岔路过渡改用 ForkTransitionAdvanceSpeed 直驱。
		const bool bForkTransition = bBranchSelected && bBranchTransitionConsumed;
		const FVector FromLoc = RunnerPawn->GetActorLocation();
		const FVector ToLoc = GetStoneWorldLocation(StoneIndex);
		FRotator TargetRotation = Config->TrackForward.Rotation();
		if (FVector::DistSquared(FromLoc, ToLoc) > KINDA_SMALL_NUMBER)
		{
			TargetRotation = (ToLoc - FromLoc).Rotation();
		}
		float TargetSpeed = Config->AdvanceSpeed;
		if (bForkTransition)
		{
			TargetSpeed = Config->ForkTransitionAdvanceSpeed;
		}
		RunnerPawn->BeginTrackAdvance(
			GetStoneWorldLocation(StoneIndex),
			TargetRotation,
			TargetSpeed,
			/*bUseRawSpeed=*/bForkTransition);
	}
	else
	{
		bAdvancing = false;
		ProgressDistance = FMath::Max(ProgressDistance, AdvanceTargetDistance);
		if (bBranchSelected
			&& bBranchTransitionConsumed
			&& Phase == ENightCoursePhase::BranchEnterBuffer)
		{
			RevealRemainingBranchCourse();
		}
		OpenNextBeatOrExit();
	}
}

void UNightCourseDirector::OnAdvanceArrived()
{
	bAdvancing = false;
	ProgressDistance = FMath::Max(ProgressDistance, AdvanceTargetDistance);
	SyncPawnToProgress(true);
	if (bBranchSelected
		&& bBranchTransitionConsumed
		&& Phase == ENightCoursePhase::BranchEnterBuffer)
	{
		RevealRemainingBranchCourse();
	}
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

bool UNightCourseDirector::IsRuntimeActorStreamingEnabled() const
{
	return GetWorld() && Config && Config->bEnableRuntimeActorStreaming;
}

int32 UNightCourseDirector::GetRuntimeSpawnThroughStone() const
{
	if (!IsRuntimeActorStreamingEnabled())
	{
		return MAX_int32;
	}

	if (bBranchSelected && (bBranchRemainderLoaded || bBranchRemainderPreGenerated) && bHasActiveRouteRule)
	{
		const float MaxVisibleDistance = ProgressDistance
			+ FMath::Max(1.f, ActiveRouteRule.VisibleDistanceCm);
		int32 LastStone = FMath::Clamp(
			CurrentStoneIndex,
			0,
			FMath::Max(0, StoneSpecs.Num() - 1));
		for (int32 StoneIndex = LastStone + 1;
			StoneIndex < StoneSpecs.Num();
			++StoneIndex)
		{
			// Branch atoms use the monotonic world-X progress axis. Never include
			// a candidate that is already behind the player, even if a later
			// array element is still inside the streaming window.
			if (StoneSpecs[StoneIndex].TrackDistance
				>= ProgressDistance - KINDA_SMALL_NUMBER
				&& StoneSpecs[StoneIndex].TrackDistance <= MaxVisibleDistance)
			{
				LastStone = StoneIndex;
			}
		}
		return LastStone;
	}

	const int32 Ahead = bBranchSelected
		? FMath::Max(1, Config->BranchSpawnBatchSize)
		: FMath::Max(1, Config->RuntimeSpawnAheadStoneCount);
	int32 LastStone = CurrentStoneIndex + Ahead;
	if (bBranchSelected
		&& !bBranchTransitionConsumed
		&& BeatSpecs.IsValidIndex(BranchTransitionBeatIndex))
	{
		LastStone = FMath::Max(
			LastStone,
			BeatSpecs[BranchTransitionBeatIndex].ToStoneIndex);
	}
	return LastStone;
}

int32 UNightCourseDirector::GetRuntimeKeepFromStone() const
{
	if (!IsRuntimeActorStreamingEnabled())
	{
		return 0;
	}

	const float MinKeepDistance = ProgressDistance
		- FMath::Max(0.f, Config->RuntimeUnloadBehindDistanceCm);
	const int32 ClampedCurrent = FMath::Clamp(
		CurrentStoneIndex,
		0,
		FMath::Max(0, StoneSpecs.Num() - 1));
	int32 FirstKeepStone = ClampedCurrent;
	for (int32 StoneIndex = 0; StoneIndex <= ClampedCurrent; ++StoneIndex)
	{
		if (StoneSpecs.IsValidIndex(StoneIndex)
			&& StoneSpecs[StoneIndex].TrackDistance >= MinKeepDistance)
		{
			FirstKeepStone = FMath::Min(FirstKeepStone, StoneIndex);
		}
	}
	return FirstKeepStone;
}

void UNightCourseDirector::DestroyRuntimeActorsBehindPlayer()
{
	if (!IsRuntimeActorStreamingEnabled())
	{
		return;
	}

	const float UnloadBeforeDistance = ProgressDistance
		- FMath::Max(0.f, Config->RuntimeUnloadBehindDistanceCm);
	const auto GetWorldDistance = [this](const FVector& Location)
	{
		return Location.X - Config->TrackOrigin.X;
	};
	const auto IsPassedStoneBehind = [this, UnloadBeforeDistance](const int32 StoneIndex)
	{
		return StoneSpecs.IsValidIndex(StoneIndex)
			&& StoneIndex < CurrentStoneIndex
			&& StoneSpecs[StoneIndex].TrackDistance < UnloadBeforeDistance;
	};

	for (int32 StoneIndex = 0; StoneIndex < SpawnedStones.Num(); ++StoneIndex)
	{
		if (SpawnedStones[StoneIndex] && IsPassedStoneBehind(StoneIndex))
		{
			ReleaseRuntimeActor(SpawnedStones[StoneIndex]);
			SpawnedStones[StoneIndex] = nullptr;
		}
	}
	for (int32 BridgeIndex = 0; BridgeIndex < SpawnedBridges.Num(); ++BridgeIndex)
	{
		if (!SpawnedBridges[BridgeIndex] || !BridgeSpecs.IsValidIndex(BridgeIndex))
		{
			continue;
		}
		const FNightBridgeSpec& Bridge = BridgeSpecs[BridgeIndex];
		const float Distance = StoneSpecs.IsValidIndex(Bridge.ToStoneIndex)
			? StoneSpecs[Bridge.ToStoneIndex].TrackDistance
			: GetWorldDistance(Bridge.WorldLocation);
		const bool bPassed = Bridge.ToStoneIndex != INDEX_NONE
			? Bridge.ToStoneIndex < CurrentStoneIndex
			: Distance < ProgressDistance;
		if (bPassed && Distance < UnloadBeforeDistance)
		{
			ReleaseRuntimeActor(SpawnedBridges[BridgeIndex]);
			SpawnedBridges[BridgeIndex] = nullptr;
		}
	}
	for (int32 BindingIndex = 0; BindingIndex < SpawnedVisualActors.Num(); ++BindingIndex)
	{
		if (!SpawnedVisualActors[BindingIndex] || !VisualBindings.IsValidIndex(BindingIndex))
		{
			continue;
		}
		const FNightAtomVisualBinding& Binding = VisualBindings[BindingIndex];
		int32 OwnerStoneIndex = Binding.StoneIndex;
		if (Binding.bIsBridge && BridgeSpecs.IsValidIndex(Binding.BridgeIndex))
		{
			OwnerStoneIndex = BridgeSpecs[Binding.BridgeIndex].ToStoneIndex;
		}
		const float Distance = StoneSpecs.IsValidIndex(OwnerStoneIndex)
			? StoneSpecs[OwnerStoneIndex].TrackDistance
			: GetWorldDistance(Binding.LocalTransform.GetLocation());
		const bool bPassed = OwnerStoneIndex != INDEX_NONE
			? OwnerStoneIndex < CurrentStoneIndex
			: Distance < ProgressDistance;
		if (bPassed && Distance < UnloadBeforeDistance)
		{
			ReleaseRuntimeActor(SpawnedVisualActors[BindingIndex]);
			SpawnedVisualActors[BindingIndex] = nullptr;
		}
	}
	for (int32 RoadsideIndex = 0; RoadsideIndex < SpawnedRoadsideActors.Num(); ++RoadsideIndex)
	{
		if (!SpawnedRoadsideActors[RoadsideIndex] || !RoadsideSpecs.IsValidIndex(RoadsideIndex))
		{
			continue;
		}
		const FNightRoadsidePropSpec& Spec = RoadsideSpecs[RoadsideIndex];
		const float Distance = StoneSpecs.IsValidIndex(Spec.ToStoneIndex)
			? StoneSpecs[Spec.ToStoneIndex].TrackDistance
			: GetWorldDistance(Spec.WorldTransform.GetLocation());
		const bool bPassed = Spec.ToStoneIndex != INDEX_NONE
			? Spec.ToStoneIndex < CurrentStoneIndex
			: Distance < ProgressDistance;
		if (bPassed && Distance < UnloadBeforeDistance)
		{
			ReleaseRuntimeActor(SpawnedRoadsideActors[RoadsideIndex]);
			SpawnedRoadsideActors[RoadsideIndex] = nullptr;
		}
	}
	for (int32 ForkIndex = 0; ForkIndex < SpawnedForkAtoms.Num(); ++ForkIndex)
	{
		if (!SpawnedForkAtoms[ForkIndex] || !ForkAtomSpecs.IsValidIndex(ForkIndex))
		{
			continue;
		}
		// 岔口模型延迟销毁：必须等分支地面已流式生成（bBranchRemainderLoaded）
		// 且主角沿真实 3D 距离明显离开岔口之后再销毁，避免主角落到半空。
		if (!bBranchRemainderLoaded)
		{
			continue;
		}
		const FVector ForkWorldLoc =
			ForkAtomSpecs[ForkIndex].WorldTransform.GetLocation();
		const float KeepMargin = Config->RuntimeUnloadBehindDistanceCm + 800.f;
		if (RunnerPawn
			&& FVector::Dist(RunnerPawn->GetActorLocation(), ForkWorldLoc) < KeepMargin)
		{
			continue;
		}
		if (!RunnerPawn && GetWorldDistance(ForkWorldLoc) < UnloadBeforeDistance)
		{
			continue;
		}
		ReleaseRuntimeActor(SpawnedForkAtoms[ForkIndex]);
		SpawnedForkAtoms[ForkIndex] = nullptr;
	}
}
void UNightCourseDirector::StreamRuntimeCourseActors()
{
	if (!IsRuntimeActorStreamingEnabled())
	{
		return;
	}
	ResetRuntimeSpawnBudget();

	const int32 FirstKeepStone = GetRuntimeKeepFromStone();
	const int32 LastSpawnStone = GetRuntimeSpawnThroughStone();
	const float UnloadBeforeDistance = ProgressDistance
		- FMath::Max(0.f, Config->RuntimeUnloadBehindDistanceCm);
	const auto IsPassedStoneBehind = [this, UnloadBeforeDistance](const int32 StoneIndex)
	{
		return StoneSpecs.IsValidIndex(StoneIndex)
			&& StoneIndex < CurrentStoneIndex
			&& StoneSpecs[StoneIndex].TrackDistance < UnloadBeforeDistance;
	};
	const auto IsWorldLocationBehind = [this, UnloadBeforeDistance](const FVector& Location)
	{
		return Location.X - Config->TrackOrigin.X < UnloadBeforeDistance;
	};
	const auto IsAtOrAheadOfProgress = [this](const float Distance)
	{
		return Distance >= ProgressDistance - KINDA_SMALL_NUMBER;
	};
	for (int32 StoneIndex = FirstKeepStone;
		StoneIndex < StoneSpecs.Num() && StoneIndex <= LastSpawnStone;
		++StoneIndex)
	{
		if (!IsPassedStoneBehind(StoneIndex)
			&& IsAtOrAheadOfProgress(StoneSpecs[StoneIndex].TrackDistance))
		{
			SpawnStoneActor(StoneIndex);
		}
	}
	for (int32 BridgeIndex = 0; BridgeIndex < BridgeSpecs.Num(); ++BridgeIndex)
	{
		const FNightBridgeSpec& Bridge = BridgeSpecs[BridgeIndex];
		const float BridgeDistance = StoneSpecs.IsValidIndex(Bridge.ToStoneIndex)
			? StoneSpecs[Bridge.ToStoneIndex].TrackDistance
			: Bridge.WorldLocation.X - Config->TrackOrigin.X;
		const bool bBehindUnloadBoundary = Bridge.ToStoneIndex != INDEX_NONE
			? IsPassedStoneBehind(Bridge.ToStoneIndex)
			: IsWorldLocationBehind(Bridge.WorldLocation);
		if (!bBehindUnloadBoundary
			&& IsAtOrAheadOfProgress(BridgeDistance)
			&& (Bridge.bForkConnectorVisualOnly
				|| (Bridge.ToStoneIndex >= FirstKeepStone
					&& Bridge.ToStoneIndex <= LastSpawnStone)))
		{
			SpawnBridgeActor(BridgeIndex);
		}
	}
	for (int32 BindingIndex = 0;
		BindingIndex < VisualBindings.Num();
		++BindingIndex)
	{
		const FNightAtomVisualBinding& Binding = VisualBindings[BindingIndex];
		bool bShouldSpawn = Binding.bForkConnectorVisualOnly;
		int32 OwnerStoneIndex = Binding.StoneIndex;
		if (Binding.bIsBridge && BridgeSpecs.IsValidIndex(Binding.BridgeIndex))
		{
			const int32 ToStoneIndex = BridgeSpecs[Binding.BridgeIndex].ToStoneIndex;
			OwnerStoneIndex = ToStoneIndex;
			bShouldSpawn = bShouldSpawn
				|| (ToStoneIndex >= FirstKeepStone
					&& ToStoneIndex <= LastSpawnStone);
		}
		else if (Binding.StoneIndex != INDEX_NONE)
		{
			bShouldSpawn = Binding.StoneIndex >= FirstKeepStone
				&& Binding.StoneIndex <= LastSpawnStone;
		}
		const bool bBehindUnloadBoundary = OwnerStoneIndex != INDEX_NONE
			? IsPassedStoneBehind(OwnerStoneIndex)
			: IsWorldLocationBehind(Binding.LocalTransform.GetLocation());
		const float OwnerDistance = StoneSpecs.IsValidIndex(OwnerStoneIndex)
			? StoneSpecs[OwnerStoneIndex].TrackDistance
			: Binding.LocalTransform.GetLocation().X - Config->TrackOrigin.X;
		if (bShouldSpawn
			&& IsAtOrAheadOfProgress(OwnerDistance)
			&& !bBehindUnloadBoundary)
		{
			SpawnVisualBinding(BindingIndex);
		}
	}
	SpawnRoadsideActors();
	DestroyRuntimeActorsBehindPlayer();
}

void UNightCourseDirector::UpdateRouteVisibility()
{
	// Streaming is also required on the shared base route. There is no active
	// branch rule before the fork is selected, but CurrentStoneIndex still
	// advances and must keep materialising the next runtime actor window.
	StreamRuntimeCourseActors();
	const float RoadsideDistanceCm = Config
		? FMath::Max(1.f, Config->RuntimeRoadsideVisibleDistanceCm)
		: 1.f;
	UpdateRoadsideVisibility(ProgressDistance, RoadsideDistanceCm);

	if (!bHasActiveRouteRule)
	{
		return;
	}

	const bool bUseBranchDistance = bBranchSelected;
	const int32 VisibleCount = FMath::Max(1, ActiveRouteRule.VisibleBlockCount);
	const int32 LastVisibleStone = CurrentStoneIndex + VisibleCount;
	const float RunnerDistance = ProgressDistance;
	const float VisibleDistanceCm = FMath::Max(
		1.f,
		ActiveRouteRule.VisibleDistanceCm);
	const float TrackOriginX = Config ? Config->TrackOrigin.X : 0.f;
	const auto GetWorldTrackDistance = [TrackOriginX](
		const FVector& WorldLocation)
	{
		return WorldLocation.X - TrackOriginX;
	};
	const auto IsWithinBranchDistance = [RunnerDistance, VisibleDistanceCm](
		const float TrackDistance)
	{
		return FMath::Abs(TrackDistance - RunnerDistance)
			<= VisibleDistanceCm;
	};

	for (int32 StoneIndex = 0; StoneIndex < SpawnedStones.Num(); ++StoneIndex)
	{
		if (SpawnedStones[StoneIndex])
		{
			const bool bVisible = bUseBranchDistance
				? (StoneSpecs.IsValidIndex(StoneIndex)
					&& IsWithinBranchDistance(
						StoneSpecs[StoneIndex].TrackDistance))
				: (StoneSpecs.IsValidIndex(StoneIndex)
					&& (StoneSpecs[StoneIndex].bForkConnectorVisualOnly
						|| StoneIndex <= LastVisibleStone));
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
			float OwnerTrackDistance = 0.f;
			bool bHasOwnerDistance = false;
			if (Binding.bIsBridge
				&& BridgeSpecs.IsValidIndex(Binding.BridgeIndex))
			{
				const FNightBridgeSpec& Bridge = BridgeSpecs[Binding.BridgeIndex];
				if (StoneSpecs.IsValidIndex(Bridge.ToStoneIndex))
				{
					OwnerTrackDistance = StoneSpecs[Bridge.ToStoneIndex].TrackDistance;
					bHasOwnerDistance = true;
				}
				else
				{
					OwnerTrackDistance = GetWorldTrackDistance(
						Bridge.WorldLocation);
					bHasOwnerDistance = true;
				}
			}
			else if (StoneSpecs.IsValidIndex(Binding.StoneIndex))
			{
				OwnerTrackDistance = StoneSpecs[Binding.StoneIndex].TrackDistance;
				bHasOwnerDistance = true;
			}
			else
			{
				OwnerTrackDistance = GetWorldTrackDistance(
					Binding.LocalTransform.GetLocation());
				bHasOwnerDistance = true;
			}

			const bool bVisible = bUseBranchDistance
				? (bHasOwnerDistance
					&& IsWithinBranchDistance(OwnerTrackDistance))
				: (Binding.bForkConnectorVisualOnly
					|| (Binding.bIsBridge
						? (BridgeSpecs.IsValidIndex(Binding.BridgeIndex)
							&& (BridgeSpecs[Binding.BridgeIndex].bForkConnectorVisualOnly
								|| BridgeSpecs[Binding.BridgeIndex].ToStoneIndex <= LastVisibleStone))
						: (Binding.StoneIndex >= 0
							&& Binding.StoneIndex <= LastVisibleStone)));
			VisualActor->SetActorHiddenInGame(!bVisible);
			VisualActor->SetActorEnableCollision(bVisible);
		}
	}
	for (int32 BridgeIndex = 0; BridgeIndex < BridgeSpecs.Num(); ++BridgeIndex)
	{
		if (SpawnedBridges.IsValidIndex(BridgeIndex) && SpawnedBridges[BridgeIndex])
		{
			const FNightBridgeSpec& Bridge = BridgeSpecs[BridgeIndex];
			const float BridgeTrackDistance = StoneSpecs.IsValidIndex(Bridge.ToStoneIndex)
				? StoneSpecs[Bridge.ToStoneIndex].TrackDistance
				: GetWorldTrackDistance(Bridge.WorldLocation);
			const bool bVisible = bUseBranchDistance
				? IsWithinBranchDistance(BridgeTrackDistance)
				: (Bridge.bForkConnectorVisualOnly
					|| Bridge.ToStoneIndex <= LastVisibleStone);
			SpawnedBridges[BridgeIndex]->SetActorHiddenInGame(!bVisible);
			SpawnedBridges[BridgeIndex]->SetActorEnableCollision(bVisible);
		}
	}
	for (int32 ForkIndex = 0; ForkIndex < SpawnedForkAtoms.Num(); ++ForkIndex)
	{
		if (SpawnedForkAtoms[ForkIndex]
			&& ForkAtomSpecs.IsValidIndex(ForkIndex))
		{
			const bool bVisible = !bUseBranchDistance
				|| IsWithinBranchDistance(GetWorldTrackDistance(
					ForkAtomSpecs[ForkIndex].WorldTransform.GetLocation()));
			SpawnedForkAtoms[ForkIndex]->SetActorHiddenInGame(!bVisible);
			SpawnedForkAtoms[ForkIndex]->SetActorEnableCollision(bVisible);
		}
	}
}
void UNightCourseDirector::RevealRemainingBranchCourse()
{
	if (!bBranchSelected || bBranchRemainderLoaded)
	{
		return;
	}

	bBranchRemainderLoaded = true;
	const float DashSeconds = FMath::Max(
		0.f,
		ActiveBootstrap.GiftBuffs.PostForkInvulnerableSeconds);
	if (DashSeconds > 0.f)
	{
		GiftDashInvulnerableEndTime = ElapsedSeconds + DashSeconds;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[Gift][BossPie] Post-fork invulnerability started for %.2fs (ends at %.2fs)."),
			DashSeconds,
			GiftDashInvulnerableEndTime);
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=BranchLoad] arrived at the branch connector Atom; enabling incremental branch streaming."));
	UpdateRouteVisibility();
}

void UNightCourseDirector::UpdateRoadsideVisibility(
	const float RunnerDistance,
	const float VisibleDistanceCm)
{
	const float TrackOriginX = Config ? Config->TrackOrigin.X : 0.f;
	for (int32 Index = 0; Index < RoadsideSpecs.Num(); ++Index)
	{
		if (!SpawnedRoadsideActors.IsValidIndex(Index)
			|| !SpawnedRoadsideActors[Index])
		{
			continue;
		}
		const FNightRoadsidePropSpec& Spec = RoadsideSpecs[Index];
		const float TrackDistance = StoneSpecs.IsValidIndex(Spec.ToStoneIndex)
			? StoneSpecs[Spec.ToStoneIndex].TrackDistance
			: Spec.WorldTransform.GetLocation().X - TrackOriginX;
		const bool bVisible = FMath::Abs(TrackDistance - RunnerDistance)
			<= FMath::Max(1.f, VisibleDistanceCm);
		SpawnedRoadsideActors[Index]->SetActorHiddenInGame(!bVisible);
		SpawnedRoadsideActors[Index]->SetActorEnableCollision(false);
	}
}
float UNightCourseDirector::ApplyGiftAwareSoulPenalty(
	const float Amount,
	const ENightJudgeOutcome Reason,
	const bool bCanConsumeShield,
	const bool bAffectedByDashInvulnerability,
	const TCHAR* Source)
{
	INightFeelBridge* Feel = GetFeel();
	if (!Feel || Amount <= 0.f)
	{
		return Feel
			? INightFeelBridge::Execute_GetSoul(FeelBridgeObject)
			: 0.f;
	}

	if (bAffectedByDashInvulnerability
		&& ElapsedSeconds < GiftDashInvulnerableEndTime)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[Gift][BossPie] Blocked %.2f Soul damage from %s (%.2fs remaining)."),
			Amount,
			Source ? Source : TEXT("Unknown"),
			GiftDashInvulnerableEndTime - ElapsedSeconds);
		return INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
	}

	if (bCanConsumeShield && RemainingGiftShieldCharges > 0)
	{
		--RemainingGiftShieldCharges;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[Gift][BlessedAmulet] Shield blocked %.2f Soul damage from %s; %d charge(s) remain."),
			Amount,
			Source ? Source : TEXT("Unknown"),
			RemainingGiftShieldCharges);
		return INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
	}

	INightFeelBridge::Execute_ApplySoulPenalty(
		FeelBridgeObject,
		Amount,
		Reason);
	TryTriggerNearDeathGift();
	return INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
}

void UNightCourseDirector::TryTriggerNearDeathGift()
{
	if (bNearDeathGiftConsumed
		|| ActiveBootstrap.GiftBuffs.NearDeathHealAmount <= 0.f
		|| ActiveBootstrap.GiftBuffs.NearDeathThreshold <= 0.f)
	{
		return;
	}
	INightFeelBridge* Feel = GetFeel();
	if (!Feel)
	{
		return;
	}
	const float BeforeSoul = INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
	if (BeforeSoul > ActiveBootstrap.GiftBuffs.NearDeathThreshold)
	{
		return;
	}

	bNearDeathGiftConsumed = true;
	INightFeelBridge::Execute_RestoreSoul(
		FeelBridgeObject,
		ActiveBootstrap.GiftBuffs.NearDeathHealAmount,
		Config ? Config->StartingSoul : BeforeSoul + ActiveBootstrap.GiftBuffs.NearDeathHealAmount);
	const float AfterSoul = INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[Gift][WildMilk] Near-death heal consumed: Soul %.2f -> %.2f (threshold %.2f)."),
		BeforeSoul,
		AfterSoul,
		ActiveBootstrap.GiftBuffs.NearDeathThreshold);
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
		ApplyGiftAwareSoulPenalty(
			ActiveRouteRule.DotSoulPerSecond
				* DrainScale
				* FMath::Max(0.f, DeltaTime),
			ENightJudgeOutcome::Miss,
			false,
			true,
			TEXT("RouteDoT"));
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
	TryTriggerNearDeathGift();
	if (INightFeelBridge* Feel = GetFeel())
	{
		if (INightFeelBridge::Execute_GetSoul(FeelBridgeObject) <= 0.f)
		{
			BeginFailure(TEXT("Soul reached zero during idle/breathing."));
			return;
		}
	}
	DestroyDeferredRuntimeActors();
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
		ProcessBranchRoutePreparation();
		if (Phase == ENightCoursePhase::ForkChoice && ForkController)
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
		TArray<FIngredientFloatStack> FinalCollectedAmounts = CollectedIngredients;
		if (bHasActiveRouteRule && ActiveRouteRule.CarryOutBonus > 0.f)
		{
			for (const FIngredientFloatStack& Stack : BranchCollectedIngredients)
			{
				AddFloatDropBonus(
					FinalCollectedAmounts,
					Stack.Id,
					Stack.Amount * ActiveRouteRule.CarryOutBonus);
			}
		}
		Result.Ingredients = QuantizeCollectedIngredients(FinalCollectedAmounts);
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
#pragma region K2 moonyfli
		if (const UNightFeelStubComponent* FeelStub = Cast<UNightFeelStubComponent>(FeelBridgeObject))
		{
			Result.MaxCombo = FeelStub->MaxCombo;
		}
#pragma endregion K2 moonyfli
		FinishNight(Result);
	}
}

void UNightCourseDirector::AddDrop(EIngredientId Id, float Amount)
{
	if (Id == EIngredientId::None || Amount <= 0.f)
	{
		return;
	}

	float AppliedAmount = Amount;
	if (!bBranchSelected
		&& ActiveBootstrap.GiftBuffs.PreForkGatherAmountBonus > 0.f)
	{
		AppliedAmount *= 1.f
			+ ActiveBootstrap.GiftBuffs.PreForkGatherAmountBonus;
	}
	AddDropToArray(CollectedIngredients, Id, AppliedAmount);
	if (bBranchSelected)
	{
		AddDropToArray(BranchCollectedIngredients, Id, AppliedAmount);
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Drop] Id=%d base=%.3f applied=%.3f preForkBonus=%.2f runningTotalFloat=%.3f."),
		static_cast<int32>(Id),
		Amount,
		AppliedAmount,
		!bBranchSelected ? ActiveBootstrap.GiftBuffs.PreForkGatherAmountBonus : 0.f,
		[&]()
		{
			for (const FIngredientFloatStack& Stack : CollectedIngredients)
			{
				if (Stack.Id == Id)
				{
					return Stack.Amount;
				}
			}
			return 0.f;
		}());
}

void UNightCourseDirector::AddDropToArray(
	TArray<FIngredientFloatStack>& Target,
	EIngredientId Id,
	float Amount) const
{
	if (Id == EIngredientId::None || Amount <= 0.f)
	{
		return;
	}
	for (FIngredientFloatStack& Stack : Target)
	{
		if (Stack.Id == Id)
		{
			Stack.Amount += Amount;
			return;
		}
	}
	FIngredientFloatStack NewStack;
	NewStack.Id = Id;
	NewStack.Amount = Amount;
	Target.Add(NewStack);
}

void UNightCourseDirector::AddFloatDropBonus(
	TArray<FIngredientFloatStack>& Target,
	EIngredientId Id,
	float Amount) const
{
	AddDropToArray(Target, Id, Amount);
}

TArray<FIngredientStack> UNightCourseDirector::QuantizeCollectedIngredients(
	const TArray<FIngredientFloatStack>& Source) const
{
	TArray<FIngredientStack> Result;
	for (const FIngredientFloatStack& FloatStack : Source)
	{
		if (FloatStack.Id == EIngredientId::None || FloatStack.Amount <= 0.f)
		{
			continue;
		}
		const int32 WholeCount = FMath::Max(
			0,
			FMath::FloorToInt(FloatStack.Amount + KINDA_SMALL_NUMBER));
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][DropQuantize] Id=%d float=%.3f -> DayCount=%d (floor at Night->Day boundary)."),
			static_cast<int32>(FloatStack.Id),
			FloatStack.Amount,
			WholeCount);
		if (WholeCount <= 0)
		{
			continue;
		}
		FIngredientStack Stack;
		Stack.Id = FloatStack.Id;
		Stack.Count = WholeCount;
		Result.Add(Stack);
	}
	return Result;
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
	ClearDeferredRuntimeActors();
	ApplyDefaultCoursePostProcessMaterial();
	bRunning = false;
	PreparedBranchRoutes.Reset();
	bBranchRoutePreparationActive = false;
	BranchRoutePreparationOrder.Reset();
	bBranchSelectionPending = false;
	PendingBranchRoute = ENightRouteId::None;
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
	Result.Ingredients = QuantizeCollectedIngredients(CollectedIngredients);
	Result.SoulLeft = Config ? Config->StartingSoul : 0.f;
	if (INightFeelBridge* Feel = GetFeel())
	{
		Result.SoulLeft = INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
	}
#pragma region K2 moonyfli
	if (const UNightFeelStubComponent* FeelStub = Cast<UNightFeelStubComponent>(FeelBridgeObject))
	{
		Result.MaxCombo = FeelStub->MaxCombo;
	}
#pragma endregion K2 moonyfli
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
	ClearDeferredRuntimeActors();
	ApplyDefaultCoursePostProcessMaterial();
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
	ActiveDefaultRoute = ENightRouteId::A;
	CourseWorldOffset = FVector::ZeroVector;
	ActiveForkPair = ENightForkPair::AB;
	RuntimeSeed = 0;
	bHasRuntimeSeed = false;
	bBuildingRuntimeCourse = false;
	bForkPending = false;
	bBranchSelected = false;
	bSpareLampConsumed = false;
	RemainingGiftShieldCharges = 0;
	GiftDashInvulnerableEndTime = 0.f;
	bNearDeathGiftConsumed = false;
	bBranchRemainderLoaded = false;
	bBranchRemainderPreGenerated = false;
	bHasActiveRouteRule = false;
	PreparedBranchRoutes.Reset();
	bBranchRoutePreparationActive = false;
	BranchRoutePreparationOrder.Reset();
	bBranchSelectionPending = false;
	PendingBranchRoute = ENightRouteId::None;
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
	Result.Ingredients = QuantizeCollectedIngredients(CollectedIngredients);
	if (!bSuccess)
	{
		LastFailureReason = TEXT("DebugForceFinish(false).");
	}
	if (INightFeelBridge* Feel = GetFeel())
	{
		Result.SoulLeft = INightFeelBridge::Execute_GetSoul(FeelBridgeObject);
	}
#pragma region K2 moonyfli
	if (const UNightFeelStubComponent* FeelStub = Cast<UNightFeelStubComponent>(FeelBridgeObject))
	{
		Result.MaxCombo = FeelStub->MaxCombo;
	}
#pragma endregion K2 moonyfli
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
