#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightCourseRoadsideActor.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Components/ArrowComponent.h"
#include "Components/ActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Algo/Sort.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "CoreGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogNightCourseAtom, Log, All);

#pragma region K2 moonyfli
namespace NightCourseAtom_Private
{
	static void ConfigurePreviewActor(
		AActor* PreviewActor,
		const FNightStoneSpec* StoneSpec,
		int32 StoneIndex,
		const FNightBridgeSpec* BridgeSpec)
	{
		if (!PreviewActor)
		{
			return;
		}

		PreviewActor->SetIsTemporarilyHiddenInEditor(false);
		PreviewActor->SetActorHiddenInGame(false);
		PreviewActor->SetActorEnableCollision(false);

		if (StoneSpec)
		{
			if (ANightCourseStoneActor* StoneActor = Cast<ANightCourseStoneActor>(PreviewActor))
			{
				StoneActor->SetupStone(StoneIndex, *StoneSpec);
				StoneActor->ApplyFoeZCompensation(true);
			}
		}
		else if (BridgeSpec)
		{
			if (ANightBridgeSegmentActor* BridgeActor = Cast<ANightBridgeSegmentActor>(PreviewActor))
			{
				BridgeActor->SetupBridge(
					*BridgeSpec,
					nullptr,
					nullptr,
					FVector::ZeroVector,
					1.f);
			}
		}
	}

	static void GetLandingPointComponents(
		const ANightCourseAtomActor* Atom,
		TArray<UNightAtomLandingPointComponent*>& OutComponents)
	{
		OutComponents.Reset();
		if (!Atom)
		{
			return;
		}

		const_cast<ANightCourseAtomActor*>(Atom)->GetComponents(OutComponents);
		if (OutComponents.Num() > 0)
		{
			return;
		}

		const UBlueprintGeneratedClass* GeneratedClass =
			Cast<UBlueprintGeneratedClass>(Atom->GetClass());
		if (!GeneratedClass)
		{
			return;
		}

		for (UActorComponent* ComponentTemplate : GeneratedClass->ComponentTemplates)
		{
			if (UNightAtomLandingPointComponent* LandingPoint =
				Cast<UNightAtomLandingPointComponent>(ComponentTemplate))
			{
				OutComponents.Add(LandingPoint);
			}
		}

		if (GeneratedClass->SimpleConstructionScript)
		{
			for (USCS_Node* Node : GeneratedClass->SimpleConstructionScript->GetAllNodes())
			{
				if (Node
					&& Cast<UNightAtomLandingPointComponent>(Node->ComponentTemplate)
					&& !OutComponents.Contains(Cast<UNightAtomLandingPointComponent>(Node->ComponentTemplate)))
				{
					OutComponents.Add(Cast<UNightAtomLandingPointComponent>(Node->ComponentTemplate));
				}
			}
		}

	}

	static void GetBridgeVisualComponents(
		const ANightCourseAtomActor* Atom,
		TArray<UNightAtomBridgeVisualComponent*>& OutComponents)
	{
		OutComponents.Reset();
		if (!Atom)
		{
			return;
		}

		const_cast<ANightCourseAtomActor*>(Atom)->GetComponents(OutComponents);
		if (OutComponents.Num() > 0)
		{
			return;
		}

		const UBlueprintGeneratedClass* GeneratedClass =
			Cast<UBlueprintGeneratedClass>(Atom->GetClass());
		if (!GeneratedClass)
		{
			return;
		}

		for (UActorComponent* ComponentTemplate : GeneratedClass->ComponentTemplates)
		{
			if (UNightAtomBridgeVisualComponent* BridgeVisual =
				Cast<UNightAtomBridgeVisualComponent>(ComponentTemplate))
			{
				OutComponents.Add(BridgeVisual);
			}
		}

		if (GeneratedClass->SimpleConstructionScript)
		{
			for (USCS_Node* Node : GeneratedClass->SimpleConstructionScript->GetAllNodes())
			{
				if (Node
					&& Cast<UNightAtomBridgeVisualComponent>(Node->ComponentTemplate)
					&& !OutComponents.Contains(Cast<UNightAtomBridgeVisualComponent>(Node->ComponentTemplate)))
				{
					OutComponents.Add(Cast<UNightAtomBridgeVisualComponent>(Node->ComponentTemplate));
				}
			}
		}
	}

	static void ClearRuntimeFoeAssignments(TArray<FNightStoneSpec>& Stones)
	{
		for (FNightStoneSpec& Stone : Stones)
		{
			// Atom art describes positions only. Enemy identity is selected by
			// the Director from the canonical course DataAsset.
			Stone.bHasFoe = false;
			Stone.FoeId = EFoeId::None;
		}
	}
}

ANightCourseAtomActor::ANightCourseAtomActor()
{
	PrimaryActorTick.bCanEverTick = false;

	AtomRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AtomRoot"));
	SetRootComponent(AtomRoot);

	EntryAnchor = CreateDefaultSubobject<UArrowComponent>(TEXT("EntryAnchor"));
	EntryAnchor->SetupAttachment(AtomRoot);
	EntryAnchor->ArrowColor = FColor::Green;
	EntryAnchor->ArrowSize = 1.5f;

	ExitAnchor = CreateDefaultSubobject<UArrowComponent>(TEXT("ExitAnchor"));
	ExitAnchor->SetupAttachment(AtomRoot);
	ExitAnchor->SetRelativeLocation(FVector(600.f, 0.f, 0.f));
	ExitAnchor->ArrowColor = FColor::Red;
	ExitAnchor->ArrowSize = 1.5f;

	ArtBoundsPreview = CreateDefaultSubobject<UBoxComponent>(TEXT("ArtBoundsPreview"));
	ArtBoundsPreview->SetupAttachment(AtomRoot);
	ArtBoundsPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArtBoundsPreview->SetHiddenInGame(true);
	ArtBoundsPreview->SetVisibility(true);
	ArtBoundsPreview->ShapeColor = FColor(255, 170, 40, 70);
}

void ANightCourseAtomActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!ArtBoundsPreview)
	{
		return;
	}

	FVector BoundsMin;
	FVector BoundsMax;
	GetLocalArtBounds(BoundsMin, BoundsMax);
	ArtBoundsPreview->SetRelativeLocation((BoundsMin + BoundsMax) * 0.5f);
	ArtBoundsPreview->SetBoxExtent((BoundsMax - BoundsMin) * 0.5f);
	ArtBoundsPreview->SetHiddenInGame(true);
	ArtBoundsPreview->SetVisibility(!GetWorld() || !GetWorld()->IsGameWorld());

#if WITH_EDITOR
	if (GetWorld()
		&& !GetWorld()->IsGameWorld()
		&& !GIsReconstructingBlueprintInstances)
	{
		RebuildAtomVisualPreview();
	}
#endif
}

FTransform ANightCourseAtomActor::GetEntryAnchorTransform() const
{
	return EntryAnchor ? EntryAnchor->GetRelativeTransform() : FTransform::Identity;
}

FTransform ANightCourseAtomActor::GetExitAnchorTransform() const
{
	return ExitAnchor ? ExitAnchor->GetRelativeTransform() : FTransform::Identity;
}

bool ANightCourseAtomActor::ValidateAtom(FString& OutError) const
{
	OutError.Reset();
	if (!EntryAnchor || !ExitAnchor)
	{
		OutError = TEXT("EntryAnchor and ExitAnchor are required.");
		return false;
	}
	const int32 LandingPointCount = GetLandingPointCount();
	if (LandingPointCount < 1)
	{
		OutError = TEXT("At least one artist LandingPoint or LocalStone is required.");
		return false;
	}
	if (AtomLengthCm <= 0.f)
	{
		OutError = TEXT("AtomLengthCm must be greater than zero.");
		return false;
	}

	if (MinYawDeg > MaxYawDeg)
	{
		OutError = FString::Printf(
			TEXT("MinYawDeg (%.2f) must not be greater than MaxYawDeg (%.2f)."),
			MinYawDeg,
			MaxYawDeg);
		return false;
	}

	FVector BoundsMin;
	FVector BoundsMax;
	GetLocalArtBounds(BoundsMin, BoundsMax);
	if (BoundsMin.X > BoundsMax.X || BoundsMin.Y > BoundsMax.Y || BoundsMin.Z > BoundsMax.Z)
	{
		OutError = TEXT("Local art bounds are inverted.");
		return false;
	}

	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseAtom_Private::GetLandingPointComponents(this, LandingPoints);
	LandingPoints.RemoveAll([](const UNightAtomLandingPointComponent* Point)
	{
		return !Point || !Point->bEnabled;
	});
	LandingPoints.Sort([](const UNightAtomLandingPointComponent& A, const UNightAtomLandingPointComponent& B)
	{
		return A.OrderIndex < B.OrderIndex;
	});

	for (int32 PointIndex = 0; PointIndex < LandingPoints.Num(); ++PointIndex)
	{
		if (!LandingPoints[PointIndex])
		{
			continue;
		}
		if (LandingPoints[PointIndex]->LandingVisualPrefab
			&& LandingPoints[PointIndex]->LandingVisualPrefab->IsChildOf(ANightBridgeSegmentActor::StaticClass()))
		{
			OutError = FString::Printf(
				TEXT("LandingPoint %d uses a Bridge BP as Temporary Preview Prefab; configure a character BP instead."),
				PointIndex);
			return false;
		}
		if (LandingPoints[PointIndex]->OrderIndex != PointIndex)
		{
			OutError = FString::Printf(
				TEXT("LandingPoint order must be contiguous from 0; component %d has OrderIndex %d."),
				PointIndex,
				LandingPoints[PointIndex]->OrderIndex);
			return false;
		}
	}

	TArray<UNightAtomBridgeVisualComponent*> BridgeVisuals;
	NightCourseAtom_Private::GetBridgeVisualComponents(this, BridgeVisuals);
	for (int32 BridgeIndex = 0; BridgeIndex < BridgeVisuals.Num(); ++BridgeIndex)
	{
		const UNightAtomBridgeVisualComponent* Bridge = BridgeVisuals[BridgeIndex];
		if (!Bridge || !Bridge->bEnabled)
		{
			continue;
		}
		if (!LandingPoints.IsValidIndex(Bridge->FromLandingIndex)
			|| !LandingPoints.IsValidIndex(Bridge->ToLandingIndex)
			|| Bridge->FromLandingIndex == Bridge->ToLandingIndex
			|| Bridge->LengthScale <= 0.f)
		{
			OutError = FString::Printf(
				TEXT("BridgeVisual %d has invalid landing indexes (%d -> %d) or LengthScale %.2f."),
				BridgeIndex,
				Bridge->FromLandingIndex,
				Bridge->ToLandingIndex,
				Bridge->LengthScale);
			return false;
		}
		if (!Bridge->VisualPrefab)
		{
			OutError = FString::Printf(
				TEXT("BridgeVisual %d is missing VisualPrefab."),
				BridgeIndex);
			return false;
		}

		const float BridgeDistance = FVector::Dist(
			LandingPoints[Bridge->FromLandingIndex]->GetRelativeLocation(),
			LandingPoints[Bridge->ToLandingIndex]->GetRelativeLocation());
		if (BridgeDistance < 1.f
			|| BridgeDistance > FMath::Max(AtomLengthCm * 2.f, 2000.f))
		{
			OutError = FString::Printf(
				TEXT("BridgeVisual %d span %.1fcm is outside the valid range for AtomLengthCm %.1f."),
				BridgeIndex,
				BridgeDistance,
				AtomLengthCm);
			return false;
		}
	}

	for (int32 BeatIndex = 0; BeatIndex < LocalBeats.Num(); ++BeatIndex)
	{
		const FNightBeatSpec& Beat = LocalBeats[BeatIndex];
		if (Beat.FromStoneIndex < 0
			|| Beat.ToStoneIndex < 0
			|| Beat.FromStoneIndex >= LandingPointCount
			|| Beat.ToStoneIndex >= LandingPointCount
			|| Beat.FromStoneIndex == Beat.ToStoneIndex)
		{
			OutError = FString::Printf(
				TEXT("LocalBeat %d references invalid stone indexes (%d -> %d)."),
				BeatIndex,
				Beat.FromStoneIndex,
				Beat.ToStoneIndex);
			return false;
		}
	}

	for (int32 BridgeIndex = 0; BridgeIndex < LocalBridges.Num(); ++BridgeIndex)
	{
		const FNightBridgeSpec& Bridge = LocalBridges[BridgeIndex];
		if (Bridge.FromStoneIndex < 0
			|| Bridge.ToStoneIndex < 0
			|| Bridge.FromStoneIndex >= LandingPointCount
			|| Bridge.ToStoneIndex >= LandingPointCount
			|| Bridge.FromStoneIndex == Bridge.ToStoneIndex)
		{
			OutError = FString::Printf(
				TEXT("LocalBridge %d references invalid stone indexes (%d -> %d)."),
				BridgeIndex,
				Bridge.FromStoneIndex,
				Bridge.ToStoneIndex);
			return false;
		}
	}

	return true;
}

void ANightCourseAtomActor::GetLocalCourseSpecs(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges) const
{
	OutStones = LocalStones;
	NightCourseAtom_Private::ClearRuntimeFoeAssignments(OutStones);
	OutBeats = LocalBeats;
	OutBridges = LocalBridges;
}

void ANightCourseAtomActor::GetLocalArtSpecs(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges,
	TArray<FNightAtomVisualBinding>& OutVisualBindings) const
{
	OutStones.Reset();
	OutBeats = LocalBeats;
	OutBridges.Reset();
	OutVisualBindings.Reset();

	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseAtom_Private::GetLandingPointComponents(this, LandingPoints);
	LandingPoints.RemoveAll([](const UNightAtomLandingPointComponent* Point)
	{
		return !Point || !Point->bEnabled;
	});
	LandingPoints.Sort([](const UNightAtomLandingPointComponent& A, const UNightAtomLandingPointComponent& B)
	{
		return A.OrderIndex < B.OrderIndex;
	});

	if (LandingPoints.Num() == 0)
	{
		OutStones = LocalStones;
		NightCourseAtom_Private::ClearRuntimeFoeAssignments(OutStones);
		OutBridges = LocalBridges;
		return;
	}

	OutStones.Reserve(LandingPoints.Num());
	OutVisualBindings.Reserve(LandingPoints.Num());
	for (int32 PointIndex = 0; PointIndex < LandingPoints.Num(); ++PointIndex)
	{
		const UNightAtomLandingPointComponent* Point = LandingPoints[PointIndex];
		if (!Point)
		{
			continue;
		}

		FNightStoneSpec Stone;
		Stone.bUseWorldPose = true;
		Stone.WorldLocation = Point->GetRelativeLocation();
		Stone.YawDeg = Point->GetRelativeRotation().Yaw;
		Stone.bHasFoe = false;
		OutStones.Add(Stone);

		FNightAtomVisualBinding Binding;
		Binding.StoneIndex = PointIndex;
		Binding.LocalTransform = Point->GetRelativeTransform();
		// LandingPoint preview prefabs are editor-only. Runtime enemy actors
		// are resolved centrally from the course DataAsset.
		Binding.VisualPrefabClass = nullptr;
		OutVisualBindings.Add(Binding);
	}

	TArray<UNightAtomBridgeVisualComponent*> BridgeVisuals;
	NightCourseAtom_Private::GetBridgeVisualComponents(this, BridgeVisuals);
	for (UNightAtomBridgeVisualComponent* BridgeVisual : BridgeVisuals)
	{
		if (!BridgeVisual || !BridgeVisual->bEnabled)
		{
			continue;
		}

		FNightBridgeSpec Bridge;
		Bridge.FromStoneIndex = BridgeVisual->FromLandingIndex;
		Bridge.ToStoneIndex = BridgeVisual->ToLandingIndex;
		Bridge.WorldLocation = BridgeVisual->GetRelativeLocation();
		Bridge.YawDeg = BridgeVisual->GetRelativeRotation().Yaw;
		Bridge.LengthScale = BridgeVisual->LengthScale;
		OutBridges.Add(Bridge);

		FNightAtomVisualBinding Binding;
		Binding.BridgeIndex = OutBridges.Num() - 1;
		Binding.bIsBridge = true;
		Binding.LocalTransform = BridgeVisual->GetRelativeTransform();
		Binding.VisualPrefabClass = BridgeVisual->VisualPrefab;
		OutVisualBindings.Add(Binding);
	}

	// Keep bridge specs authored in the legacy array as an additive compatibility
	// path when an artist has not converted every bridge to a component yet.
	if (BridgeVisuals.Num() == 0)
	{
		OutBridges = LocalBridges;
	}
}

int32 ANightCourseAtomActor::GetLandingPointCount() const
{
	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseAtom_Private::GetLandingPointComponents(this, LandingPoints);
	int32 EnabledCount = 0;
	for (const UNightAtomLandingPointComponent* Point : LandingPoints)
	{
		if (Point && Point->bEnabled)
		{
			++EnabledCount;
		}
	}
	return EnabledCount > 0 ? EnabledCount : LocalStones.Num();
}

void ANightCourseAtomActor::GetLocalArtBounds(FVector& OutMin, FVector& OutMax) const
{
	if (bUseLocalArtBounds)
	{
		OutMin = LocalArtBoundsMin;
		OutMax = LocalArtBoundsMax;
		return;
	}

	bool bHasPoint = false;
	OutMin = FVector::ZeroVector;
	OutMax = FVector::ZeroVector;
	auto IncludePoint = [&OutMin, &OutMax, &bHasPoint](const FVector& Point)
	{
		if (!bHasPoint)
		{
			OutMin = Point;
			OutMax = Point;
			bHasPoint = true;
			return;
		}
		OutMin = OutMin.ComponentMin(Point);
		OutMax = OutMax.ComponentMax(Point);
	};

	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseAtom_Private::GetLandingPointComponents(this, LandingPoints);
	for (const UNightAtomLandingPointComponent* Point : LandingPoints)
	{
		if (Point && Point->bEnabled)
		{
			IncludePoint(Point->GetRelativeLocation());
		}
	}

	TArray<UNightAtomBridgeVisualComponent*> BridgeVisuals;
	NightCourseAtom_Private::GetBridgeVisualComponents(this, BridgeVisuals);
	for (const UNightAtomBridgeVisualComponent* Bridge : BridgeVisuals)
	{
		if (Bridge && Bridge->bEnabled)
		{
			IncludePoint(Bridge->GetRelativeLocation());
		}
	}
	float HousePreviewY = EntryAnchor
		? EntryAnchor->GetRelativeLocation().Y
		: 0.f;
	for (const UNightAtomLandingPointComponent* Point : LandingPoints)
	{
		if (Point && Point->bEnabled)
		{
			HousePreviewY = Point->GetRelativeLocation().Y;
			break;
		}
	}

	auto IncludeRoadsidePreview = [&IncludePoint](
		const TSubclassOf<ANightRoadsideSegmentActor> RoadsideClass,
		const FVector& Anchor,
		const float YawDeg,
		const float SideOffset,
		const float ZOffset,
		const bool bUseFixedWorldXAxis,
		const bool bMirror)
	{
		if (!RoadsideClass)
		{
			return;
		}
		const ANightRoadsideSegmentActor* Defaults =
			RoadsideClass->GetDefaultObject<ANightRoadsideSegmentActor>();
		FVector Start;
		FVector End;
		if (!Defaults
			|| !Defaults->GetRoadsideMarkerLocations(Start, End))
		{
			return;
		}
		const float EffectiveYawDeg =
			bUseFixedWorldXAxis ? 0.f : YawDeg;
		const FRotator EffectiveRotation(
			0.f,
			EffectiveYawDeg,
			0.f);
		const FVector MirrorScale = bMirror
			? FVector(1.f, -1.f, 1.f)
			: FVector::OneVector;
		const FVector MirroredStart(
			Start.X * MirrorScale.X,
			Start.Y * MirrorScale.Y,
			Start.Z * MirrorScale.Z);
		const FVector MirroredEnd(
			End.X * MirrorScale.X,
			End.Y * MirrorScale.Y,
			End.Z * MirrorScale.Z);
		const FQuat MarkerAlignment =
			FQuat::FindBetweenNormals(
				(MirroredEnd - MirroredStart).GetSafeNormal(),
				FVector::ForwardVector);
		const FQuat Rotation =
			EffectiveRotation.Quaternion() * MarkerAlignment;
		const FVector Forward =
			EffectiveRotation.Vector().GetSafeNormal2D();
		const FVector SafeForward = Forward.IsNearlyZero()
			? FVector::ForwardVector
			: Forward;
		const FVector Right = FVector::CrossProduct(
			FVector::UpVector,
			SafeForward).GetSafeNormal();
		const FVector SafeRight = Right.IsNearlyZero()
			? FVector::RightVector
			: Right;
		const FTransform Transform(
			Rotation,
			Anchor
				+ SafeRight * SideOffset
				+ FVector::UpVector * ZOffset
				- Rotation.RotateVector(MirroredStart),
			MirrorScale);
		IncludePoint(Transform.TransformPosition(Start));
		IncludePoint(Transform.TransformPosition(End));
	};

	for (const UNightAtomBridgeVisualComponent* Bridge : BridgeVisuals)
	{
		if (!Bridge || !Bridge->bEnabled)
		{
			continue;
		}
		const float YawDeg = Bridge->GetRelativeRotation().Yaw;
		const FVector HouseAnchor(
			Bridge->GetRelativeLocation().X,
			HousePreviewY,
			Bridge->GetRelativeLocation().Z);
		IncludeRoadsidePreview(
			HouseRoadsidePreviewPrefab,
			HouseAnchor,
			YawDeg,
			-RoadsidePreviewLeftOffsetCm,
			RoadsidePreviewZOffsetCm,
			true,
			false);
		IncludeRoadsidePreview(
			HouseRoadsidePreviewPrefab,
			HouseAnchor,
			YawDeg,
			RoadsidePreviewRightOffsetCm,
			RoadsidePreviewZOffsetCm,
			true,
			true);
		IncludeRoadsidePreview(
			PoleRoadsidePreviewPrefab,
			Bridge->GetRelativeLocation(),
			YawDeg,
			-RoadsidePreviewLeftOffsetCm,
			RoadsidePreviewZOffsetCm,
			false,
			false);
		IncludeRoadsidePreview(
			PoleRoadsidePreviewPrefab,
			Bridge->GetRelativeLocation(),
			YawDeg,
			RoadsidePreviewRightOffsetCm,
			RoadsidePreviewZOffsetCm,
			false,
			true);
	}

	if (LandingPoints.Num() == 0 && BridgeVisuals.Num() == 0)
	{
		for (const FNightStoneSpec& Stone : LocalStones)
		{
			IncludePoint(Stone.bUseWorldPose ? Stone.WorldLocation : FVector(Stone.TrackDistance, 0.f, 0.f));
		}
	}

	IncludePoint(EntryAnchor ? EntryAnchor->GetRelativeLocation() : FVector::ZeroVector);
	IncludePoint(ExitAnchor ? ExitAnchor->GetRelativeLocation() : FVector(AtomLengthCm, 0.f, 0.f));

	if (!bHasPoint)
	{
		OutMin = FVector(-100.f, -250.f, -200.f);
		OutMax = FVector(AtomLengthCm + 100.f, 250.f, 300.f);
		return;
	}

	const FVector Padding(100.f, 100.f, 100.f);
	OutMin -= Padding;
	OutMax += Padding;
}

void ANightCourseAtomActor::RebuildAtomVisualPreview()
{
#if WITH_EDITOR
	if (GIsReconstructingBlueprintInstances)
	{
		return;
	}
#endif

	for (UChildActorComponent* PreviewComponent : PreviewVisualComponents)
	{
		if (PreviewComponent)
		{
			PreviewComponent->DestroyComponent();
		}
	}
	PreviewVisualComponents.Reset();

	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseAtom_Private::GetLandingPointComponents(this, LandingPoints);
	LandingPoints.RemoveAll([](const UNightAtomLandingPointComponent* Point)
	{
		return !Point || !Point->bEnabled;
	});
	LandingPoints.Sort([](const UNightAtomLandingPointComponent& A, const UNightAtomLandingPointComponent& B)
	{
		return A.OrderIndex < B.OrderIndex;
	});
	const float HousePreviewY = LandingPoints.Num() > 0
		? LandingPoints[0]->GetRelativeLocation().Y
		: (EntryAnchor ? EntryAnchor->GetRelativeLocation().Y : 0.f);

	auto AddPreview = [this](
		USceneComponent* AttachParent,
		TSubclassOf<AActor> VisualClass,
		const FTransform& LocalTransform,
		const FNightStoneSpec* StoneSpec,
		int32 StoneIndex,
		const FNightBridgeSpec* BridgeSpec,
		const FString& Name)
	{
		if (!AttachParent)
		{
			return;
		}
		if (!VisualClass)
		{
			UE_LOG(
				LogNightCourseAtom,
				Warning,
				TEXT("Atom '%s' preview '%s' has no visual prefab class."),
				*GetName(),
				*Name);
			return;
		}
		if (StoneSpec && VisualClass->IsChildOf(ANightBridgeSegmentActor::StaticClass()))
		{
			UE_LOG(
				LogNightCourseAtom,
				Error,
				TEXT("Atom '%s' preview '%s' uses a Bridge BP on a LandingPoint; configure a character BP."),
				*GetName(),
				*Name);
			return;
		}

		UChildActorComponent* Preview = NewObject<UChildActorComponent>(
			this,
			MakeUniqueObjectName(this, UChildActorComponent::StaticClass(), FName(*Name)),
			RF_Transient);
		Preview->SetupAttachment(AttachParent);
		Preview->SetRelativeTransform(LocalTransform);
		Preview->SetChildActorClass(VisualClass);
		AddInstanceComponent(Preview);
		Preview->RegisterComponent();
		Preview->SetVisibility(true);
		Preview->SetHiddenInGame(false);
		NightCourseAtom_Private::ConfigurePreviewActor(
			Preview->GetChildActor(),
			StoneSpec,
			StoneIndex,
			BridgeSpec);
		// SetupBridge/SetupStone may touch the child actor transform. The
		// component transform is the authored source of truth for BP preview.
		Preview->SetRelativeTransform(LocalTransform);
		PreviewVisualComponents.Add(Preview);
	};

	auto AddRoadsidePair = [this, &AddPreview](
		const TSubclassOf<ANightRoadsideSegmentActor> RoadsideClass,
		const ENightRoadsideKind Kind,
		const FVector& Anchor,
		const float YawDeg,
		const int32 PreviewIndex)
	{
		if (!RoadsideClass)
		{
			return;
		}
		const TSubclassOf<AActor> PreviewClass = RoadsideClass;
		const ANightRoadsideSegmentActor* Defaults =
			RoadsideClass->GetDefaultObject<ANightRoadsideSegmentActor>();
		FVector MarkerStart;
		FVector MarkerEnd;
		if (!Defaults
			|| !Defaults->GetRoadsideMarkerLocations(MarkerStart, MarkerEnd))
		{
			return;
		}
		const float EffectiveYawDeg =
			Kind == ENightRoadsideKind::House ? 0.f : YawDeg;
		const FRotator EffectiveRotation(
			0.f,
			EffectiveYawDeg,
			0.f);
		const FVector Forward =
			EffectiveRotation.Vector().GetSafeNormal2D();
		const FVector SafeForward = Forward.IsNearlyZero()
			? FVector::ForwardVector
			: Forward;
		const FVector Right = FVector::CrossProduct(
			FVector::UpVector,
			SafeForward).GetSafeNormal();
		const FVector SafeRight = Right.IsNearlyZero()
			? FVector::RightVector
			: Right;
		const float LeftOffset = FMath::Max(0.f, RoadsidePreviewLeftOffsetCm);
		const float RightOffset = FMath::Max(0.f, RoadsidePreviewRightOffsetCm);
		const FVector ZOffset = FVector::UpVector * RoadsidePreviewZOffsetCm;
		const FVector LeftMirrorScale = FVector::OneVector;
		const FVector RightMirrorScale = FVector(1.f, -1.f, 1.f);
		const FString KindName = Kind == ENightRoadsideKind::House
			? TEXT("House")
			: TEXT("Pole");
		auto AddSidePreview = [this, &AddPreview, &Anchor, &ZOffset, &SafeRight,
			&PreviewClass, &EffectiveRotation, &MarkerStart, &MarkerEnd,
			&KindName,
			&LeftMirrorScale, &RightMirrorScale, PreviewIndex](
			const bool bRightSide,
			const float SideOffset)
		{
			const FVector MirrorScale = bRightSide
				? RightMirrorScale
				: LeftMirrorScale;
			const FVector MirroredMarkerStart(
				MarkerStart.X * MirrorScale.X,
				MarkerStart.Y * MirrorScale.Y,
				MarkerStart.Z * MirrorScale.Z);
			const FVector MirroredMarkerEnd(
				MarkerEnd.X * MirrorScale.X,
				MarkerEnd.Y * MirrorScale.Y,
				MarkerEnd.Z * MirrorScale.Z);
			const FQuat Rotation =
				EffectiveRotation.Quaternion()
				* FQuat::FindBetweenNormals(
					(MirroredMarkerEnd - MirroredMarkerStart).GetSafeNormal(),
					FVector::ForwardVector);
			AddPreview(
				AtomRoot,
				PreviewClass,
				FTransform(
					Rotation,
					Anchor
						+ SafeRight * SideOffset
						+ ZOffset
						- Rotation.RotateVector(MirroredMarkerStart),
					MirrorScale),
				nullptr,
				INDEX_NONE,
				nullptr,
				FString::Printf(
					TEXT("RoadsidePreview_%s_%s_%d"),
					*KindName,
					bRightSide ? TEXT("Right") : TEXT("Left"),
					PreviewIndex));
		};

		AddSidePreview(false, -LeftOffset);
		AddSidePreview(true, RightOffset);
	};

	for (int32 Index = 0; Index < LandingPoints.Num(); ++Index)
	{
		if (LandingPoints[Index])
		{
			UNightAtomLandingPointComponent* Point = LandingPoints[Index];
			FNightStoneSpec PreviewStone;
			PreviewStone.bUseWorldPose = true;
			PreviewStone.WorldLocation = Point->GetRelativeLocation();
			PreviewStone.YawDeg = Point->GetRelativeRotation().Yaw;
			const TSubclassOf<AActor> PreviewClass =
				Point->LandingVisualPrefab;
			PreviewStone.bHasFoe =
				PreviewClass
				&& PreviewClass->IsChildOf(ANightCourseStoneActor::StaticClass());
			AddPreview(
				AtomRoot,
				PreviewClass,
				Point->GetRelativeTransform(),
				&PreviewStone,
				Index,
				nullptr,
				FString::Printf(TEXT("LandingPreview_%d"), Index));
		}
	}

	TArray<UNightAtomBridgeVisualComponent*> BridgeVisuals;
	NightCourseAtom_Private::GetBridgeVisualComponents(this, BridgeVisuals);
	for (int32 Index = 0; Index < BridgeVisuals.Num(); ++Index)
	{
		if (BridgeVisuals[Index] && BridgeVisuals[Index]->bEnabled)
		{
			const UNightAtomBridgeVisualComponent* BridgeVisual = BridgeVisuals[Index];
			FNightBridgeSpec PreviewBridge;
			PreviewBridge.FromStoneIndex = BridgeVisual->FromLandingIndex;
			PreviewBridge.ToStoneIndex = BridgeVisual->ToLandingIndex;
			PreviewBridge.WorldLocation = BridgeVisual->GetRelativeLocation();
			PreviewBridge.YawDeg = BridgeVisual->GetRelativeRotation().Yaw;
			PreviewBridge.LengthScale = BridgeVisual->LengthScale;
			AddPreview(
				AtomRoot,
				BridgeVisual->VisualPrefab,
				BridgeVisual->GetRelativeTransform(),
				nullptr,
				INDEX_NONE,
				&PreviewBridge,
				FString::Printf(TEXT("BridgePreview_%d"), Index));
		}
	}

	int32 RoadsidePreviewIndex = 0;
	for (const UNightAtomBridgeVisualComponent* BridgeVisual : BridgeVisuals)
	{
		if (!BridgeVisual || !BridgeVisual->bEnabled)
		{
			continue;
		}
		const FVector Anchor = BridgeVisual->GetRelativeLocation();
		FVector HouseAnchor = Anchor;
		HouseAnchor.Y = HousePreviewY;
		const float YawDeg = BridgeVisual->GetRelativeRotation().Yaw;
		AddRoadsidePair(
			HouseRoadsidePreviewPrefab,
			ENightRoadsideKind::House,
			HouseAnchor,
			YawDeg,
			RoadsidePreviewIndex);
		AddRoadsidePair(
			PoleRoadsidePreviewPrefab,
			ENightRoadsideKind::Pole,
			Anchor,
			YawDeg,
			RoadsidePreviewIndex);
		++RoadsidePreviewIndex;
	}
	if (RoadsidePreviewIndex == 0)
	{
		const FVector Start = LandingPoints.Num() > 0 && LandingPoints[0]
			? LandingPoints[0]->GetRelativeLocation()
			: (EntryAnchor ? EntryAnchor->GetRelativeLocation() : FVector::ZeroVector);
		const FVector End = LandingPoints.Num() > 1 && LandingPoints.Last()
			? LandingPoints.Last()->GetRelativeLocation()
			: (ExitAnchor
				? ExitAnchor->GetRelativeLocation()
				: FVector(AtomLengthCm, 0.f, 0.f));
		const FVector Delta = End - Start;
		const float YawDeg = Delta.IsNearlyZero() ? 0.f : Delta.Rotation().Yaw;
		const FVector Anchor = (Start + End) * 0.5f;
		FVector HouseAnchor = Anchor;
		HouseAnchor.Y = HousePreviewY;
		AddRoadsidePair(
			HouseRoadsidePreviewPrefab,
			ENightRoadsideKind::House,
			HouseAnchor,
			YawDeg,
			0);
		AddRoadsidePair(
			PoleRoadsidePreviewPrefab,
			ENightRoadsideKind::Pole,
			Anchor,
			YawDeg,
			0);
	}
}

void ANightCourseAtomActor::ValidateAtomForEditor()
{
	FString Error;
	if (ValidateAtom(Error))
	{
		UE_LOG(LogNightCourseAtom, Display, TEXT("Atom '%s' is valid (%d landing points)."), *GetName(), GetLandingPointCount());
	}
	else
	{
		UE_LOG(LogNightCourseAtom, Error, TEXT("Atom '%s' validation failed: %s"), *GetName(), *Error);
	}
}

void ANightCourseAtomActor::SnapFirstLastLandingToAnchors()
{
	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseAtom_Private::GetLandingPointComponents(this, LandingPoints);
	LandingPoints.RemoveAll([](const UNightAtomLandingPointComponent* Point)
	{
		return !Point || !Point->bEnabled;
	});
	LandingPoints.Sort([](const UNightAtomLandingPointComponent& A, const UNightAtomLandingPointComponent& B)
	{
		return A.OrderIndex < B.OrderIndex;
	});

	if (LandingPoints.Num() == 0)
	{
		UE_LOG(LogNightCourseAtom, Warning, TEXT("Atom '%s' has no enabled LandingPoint components to snap."), *GetName());
		return;
	}

	Modify();
	LandingPoints[0]->Modify();
	LandingPoints[0]->SetRelativeTransform(EntryAnchor ? EntryAnchor->GetRelativeTransform() : FTransform::Identity);
	if (LandingPoints.Num() > 1)
	{
		LandingPoints.Last()->Modify();
		LandingPoints.Last()->SetRelativeTransform(ExitAnchor ? ExitAnchor->GetRelativeTransform() : FTransform::Identity);
	}
	MarkPackageDirty();
}
#pragma endregion K2 moonyfli
