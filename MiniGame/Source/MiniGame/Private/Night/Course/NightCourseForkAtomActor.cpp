#include "Night/Course/NightCourseForkAtomActor.h"

#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightCourseAtomArtComponents.h"
#include "Algo/Sort.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#pragma region K2 moonyfli
namespace NightCourseForkAtom_Private
{
	static void GetLandingPointComponents(
		const ANightCourseForkAtomActor* ForkAtom,
		TArray<UNightAtomLandingPointComponent*>& OutComponents)
	{
		OutComponents.Reset();
		if (!ForkAtom)
		{
			return;
		}

		const_cast<ANightCourseForkAtomActor*>(ForkAtom)->GetComponents(OutComponents);
		if (OutComponents.Num() > 0)
		{
			return;
		}

		const UBlueprintGeneratedClass* GeneratedClass =
			Cast<UBlueprintGeneratedClass>(ForkAtom->GetClass());
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
					&& !OutComponents.Contains(
						Cast<UNightAtomLandingPointComponent>(Node->ComponentTemplate)))
				{
					OutComponents.Add(
						Cast<UNightAtomLandingPointComponent>(Node->ComponentTemplate));
				}
			}
		}
	}

	static void GetBridgeVisualComponents(
		const ANightCourseForkAtomActor* ForkAtom,
		TArray<UNightAtomBridgeVisualComponent*>& OutComponents)
	{
		OutComponents.Reset();
		if (!ForkAtom)
		{
			return;
		}

		const_cast<ANightCourseForkAtomActor*>(ForkAtom)->GetComponents(OutComponents);
		if (OutComponents.Num() > 0)
		{
			return;
		}

		const UBlueprintGeneratedClass* GeneratedClass =
			Cast<UBlueprintGeneratedClass>(ForkAtom->GetClass());
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
					&& !OutComponents.Contains(
						Cast<UNightAtomBridgeVisualComponent>(Node->ComponentTemplate)))
				{
					OutComponents.Add(
						Cast<UNightAtomBridgeVisualComponent>(Node->ComponentTemplate));
				}
			}
		}
	}
}

ANightCourseForkAtomActor::ANightCourseForkAtomActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ForkRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ForkRoot"));
	SetRootComponent(ForkRoot);

	EntryAnchor = CreateDefaultSubobject<UArrowComponent>(TEXT("EntryAnchor"));
	EntryAnchor->SetupAttachment(ForkRoot);
	EntryAnchor->ArrowColor = FColor::Green;
	EntryAnchor->ArrowSize = 1.5f;

	LeftExitAnchor = CreateDefaultSubobject<UArrowComponent>(TEXT("LeftExitAnchor"));
	LeftExitAnchor->SetupAttachment(ForkRoot);
	LeftExitAnchor->SetRelativeLocation(FVector(900.f, -400.f, 0.f));
	LeftExitAnchor->SetRelativeRotation(FRotator(0.f, -12.f, 0.f));
	LeftExitAnchor->ArrowColor = FColor::Blue;
	LeftExitAnchor->ArrowSize = 1.5f;

	RightExitAnchor = CreateDefaultSubobject<UArrowComponent>(TEXT("RightExitAnchor"));
	RightExitAnchor->SetupAttachment(ForkRoot);
	RightExitAnchor->SetRelativeLocation(FVector(900.f, 400.f, 0.f));
	RightExitAnchor->SetRelativeRotation(FRotator(0.f, 12.f, 0.f));
	RightExitAnchor->ArrowColor = FColor::Yellow;
	RightExitAnchor->ArrowSize = 1.5f;

	ArtBoundsPreview = CreateDefaultSubobject<UBoxComponent>(TEXT("ArtBoundsPreview"));
	ArtBoundsPreview->SetupAttachment(ForkRoot);
	ArtBoundsPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArtBoundsPreview->SetHiddenInGame(true);
	ArtBoundsPreview->SetVisibility(true);
	ArtBoundsPreview->ShapeColor = FColor(80, 180, 255, 70);
}

void ANightCourseForkAtomActor::OnConstruction(const FTransform& Transform)
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
}

FTransform ANightCourseForkAtomActor::GetEntryAnchorTransform() const
{
	return EntryAnchor ? EntryAnchor->GetRelativeTransform() : FTransform::Identity;
}

FTransform ANightCourseForkAtomActor::GetLeftExitAnchorTransform() const
{
	return LeftExitAnchor ? LeftExitAnchor->GetRelativeTransform() : FTransform::Identity;
}

FTransform ANightCourseForkAtomActor::GetRightExitAnchorTransform() const
{
	return RightExitAnchor ? RightExitAnchor->GetRelativeTransform() : FTransform::Identity;
}

void ANightCourseForkAtomActor::GetLocalCourseSpecs(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges,
	TArray<FNightAtomVisualBinding>& OutVisualBindings,
	ENightRouteId ActiveRoute,
	ENightRouteId LeftRoute,
	ENightRouteId RightRoute) const
{
	OutStones.Reset();
	OutBeats.Reset();
	OutBridges.Reset();
	OutVisualBindings.Reset();

	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseForkAtom_Private::GetLandingPointComponents(this, LandingPoints);
	LandingPoints.RemoveAll([](const UNightAtomLandingPointComponent* Point)
	{
		return !Point || !Point->bEnabled;
	});
	LandingPoints.Sort([](
		const UNightAtomLandingPointComponent& A,
		const UNightAtomLandingPointComponent& B)
	{
		return A.OrderIndex < B.OrderIndex;
	});

	TArray<int32> OriginalToActiveIndex;
	OriginalToActiveIndex.Init(INDEX_NONE, LandingPoints.Num());
	TArray<UNightAtomLandingPointComponent*> ActiveLandingPoints;
	ActiveLandingPoints.Reserve(LandingPoints.Num());
	auto IsActiveLandingPoint = [this, ActiveRoute, LeftRoute, RightRoute](
		const UNightAtomLandingPointComponent* Point)
	{
		if (!Point || !bUseLandingPointLanes
			|| Point->ForkLane == ENightForkLandingLane::MainRoad)
		{
			return Point != nullptr;
		}
		if (ActiveRoute == ENightRouteId::None)
		{
			return false;
		}
		if (Point->ForkLane == ENightForkLandingLane::LeftBranch)
		{
			return ActiveRoute == LeftRoute;
		}
		if (Point->ForkLane == ENightForkLandingLane::RightBranch)
		{
			return ActiveRoute == RightRoute;
		}
		return false;
	};
	for (int32 OriginalIndex = 0; OriginalIndex < LandingPoints.Num(); ++OriginalIndex)
	{
		if (IsActiveLandingPoint(LandingPoints[OriginalIndex]))
		{
			OriginalToActiveIndex[OriginalIndex] = ActiveLandingPoints.Num();
			ActiveLandingPoints.Add(LandingPoints[OriginalIndex]);
		}
	}

	OutStones.Reserve(ActiveLandingPoints.Num());
	OutBeats.Reserve(FMath::Max(0, ActiveLandingPoints.Num() - 1));
	OutVisualBindings.Reserve(ActiveLandingPoints.Num());
	for (int32 PointIndex = 0; PointIndex < ActiveLandingPoints.Num(); ++PointIndex)
	{
		const UNightAtomLandingPointComponent* Point = ActiveLandingPoints[PointIndex];
		if (!Point)
		{
			continue;
		}

		FNightStoneSpec Stone;
		Stone.bUseWorldPose = true;
		Stone.WorldLocation = Point->GetRelativeLocation();
		Stone.YawDeg = Point->GetRelativeRotation().Yaw;
		Stone.bHasFoe = Point->bSpawnFoe;
		Stone.FoeId = EFoeId::None;
		OutStones.Add(Stone);

		FNightAtomVisualBinding Binding;
		Binding.StoneIndex = PointIndex;
		Binding.LocalTransform = Point->GetRelativeTransform();
		OutVisualBindings.Add(Binding);
	}

	for (int32 PointIndex = 1; PointIndex < OutStones.Num(); ++PointIndex)
	{
		FNightBeatSpec Beat;
		Beat.FromStoneIndex = PointIndex - 1;
		Beat.ToStoneIndex = PointIndex;
		Beat.Action = ActiveLandingPoints[PointIndex]->bSpawnFoe
			? ENightNodeKind::Enemy
			: ENightNodeKind::Hazard;
		OutBeats.Add(Beat);
	}

	TArray<UNightAtomBridgeVisualComponent*> BridgeVisuals;
	NightCourseForkAtom_Private::GetBridgeVisualComponents(this, BridgeVisuals);
	for (UNightAtomBridgeVisualComponent* BridgeVisual : BridgeVisuals)
	{
		if (!BridgeVisual
			|| !BridgeVisual->bEnabled
			|| !OriginalToActiveIndex.IsValidIndex(BridgeVisual->FromLandingIndex)
			|| !OriginalToActiveIndex.IsValidIndex(BridgeVisual->ToLandingIndex)
			|| OriginalToActiveIndex[BridgeVisual->FromLandingIndex] == INDEX_NONE
			|| OriginalToActiveIndex[BridgeVisual->ToLandingIndex] == INDEX_NONE)
		{
			continue;
		}

		FNightBridgeSpec Bridge;
		Bridge.FromStoneIndex = OriginalToActiveIndex[BridgeVisual->FromLandingIndex];
		Bridge.ToStoneIndex = OriginalToActiveIndex[BridgeVisual->ToLandingIndex];
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
}

void ANightCourseForkAtomActor::GetLocalCourseSpecs(
	TArray<FNightStoneSpec>& OutStones,
	TArray<FNightBeatSpec>& OutBeats,
	TArray<FNightBridgeSpec>& OutBridges,
	TArray<FNightAtomVisualBinding>& OutVisualBindings) const
{
	GetLocalCourseSpecs(
		OutStones,
		OutBeats,
		OutBridges,
		OutVisualBindings,
		ENightRouteId::None,
		ENightRouteId::None,
		ENightRouteId::None);
}

int32 ANightCourseForkAtomActor::GetLandingPointCount() const
{
	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseForkAtom_Private::GetLandingPointComponents(this, LandingPoints);
	int32 EnabledCount = 0;
	for (const UNightAtomLandingPointComponent* Point : LandingPoints)
	{
		if (Point && Point->bEnabled)
		{
			++EnabledCount;
		}
	}
	return EnabledCount;
}

bool ANightCourseForkAtomActor::ValidateForkAtom(FString& OutError) const
{
	OutError.Reset();
	if (!EntryAnchor || !LeftExitAnchor || !RightExitAnchor)
	{
		OutError = TEXT("EntryAnchor, LeftExitAnchor and RightExitAnchor are required.");
		return false;
	}

	if (bUseLocalArtBounds
		&& (LocalArtBoundsMin.X >= LocalArtBoundsMax.X
			|| LocalArtBoundsMin.Y >= LocalArtBoundsMax.Y
			|| LocalArtBoundsMin.Z >= LocalArtBoundsMax.Z))
	{
		OutError = TEXT("Fork Atom local art bounds are inverted or empty.");
		return false;
	}

	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseForkAtom_Private::GetLandingPointComponents(this, LandingPoints);
	LandingPoints.RemoveAll([](const UNightAtomLandingPointComponent* Point)
	{
		return !Point || !Point->bEnabled;
	});
	LandingPoints.Sort([](
		const UNightAtomLandingPointComponent& A,
		const UNightAtomLandingPointComponent& B)
	{
		return A.OrderIndex < B.OrderIndex;
	});
	for (int32 PointIndex = 0; PointIndex < LandingPoints.Num(); ++PointIndex)
	{
		if (LandingPoints[PointIndex]->OrderIndex != PointIndex)
		{
			OutError = FString::Printf(
				TEXT("Fork LandingPoint order must be contiguous from 0; component %d has OrderIndex %d."),
				PointIndex,
				LandingPoints[PointIndex]->OrderIndex);
			return false;
		}
	}

	TArray<UNightAtomBridgeVisualComponent*> BridgeVisuals;
	NightCourseForkAtom_Private::GetBridgeVisualComponents(this, BridgeVisuals);
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
			|| Bridge->LengthScale <= 0.f
			|| !Bridge->VisualPrefab
			|| !Bridge->VisualPrefab->IsChildOf(ANightBridgeSegmentActor::StaticClass()))
		{
			OutError = FString::Printf(
				TEXT("Fork BridgeVisual %d has invalid landing indexes, prefab or LengthScale %.2f."),
				BridgeIndex,
				Bridge->LengthScale);
			return false;
		}

		const float BridgeDistance = FVector::Dist(
			LandingPoints[Bridge->FromLandingIndex]->GetRelativeLocation(),
			LandingPoints[Bridge->ToLandingIndex]->GetRelativeLocation());
		if (BridgeDistance < 1.f)
		{
			OutError = FString::Printf(
				TEXT("Fork BridgeVisual %d connects landing points that are too close."),
				BridgeIndex);
			return false;
		}
	}

	const FVector EntryLocation = EntryAnchor->GetRelativeLocation();
	const FVector LeftLocation = LeftExitAnchor->GetRelativeLocation();
	const FVector RightLocation = RightExitAnchor->GetRelativeLocation();
	if (FVector::DistSquared(EntryLocation, LeftLocation) <= FMath::Square(1.f)
		|| FVector::DistSquared(EntryLocation, RightLocation) <= FMath::Square(1.f)
		|| FVector::DistSquared(LeftLocation, RightLocation) <= FMath::Square(1.f))
	{
		OutError = TEXT("Fork Atom anchors must describe distinct entry, left-exit and right-exit positions.");
		return false;
	}

	return true;
}

void ANightCourseForkAtomActor::GetLocalArtBounds(
	FVector& OutMin,
	FVector& OutMax) const
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

	if (EntryAnchor)
	{
		IncludePoint(EntryAnchor->GetRelativeLocation());
	}
	if (LeftExitAnchor)
	{
		IncludePoint(LeftExitAnchor->GetRelativeLocation());
	}
	if (RightExitAnchor)
	{
		IncludePoint(RightExitAnchor->GetRelativeLocation());
	}

	TArray<UNightAtomLandingPointComponent*> LandingPoints;
	NightCourseForkAtom_Private::GetLandingPointComponents(this, LandingPoints);
	for (const UNightAtomLandingPointComponent* Point : LandingPoints)
	{
		if (Point && Point->bEnabled)
		{
			IncludePoint(Point->GetRelativeLocation());
		}
	}

	TArray<UNightAtomBridgeVisualComponent*> BridgeVisuals;
	NightCourseForkAtom_Private::GetBridgeVisualComponents(this, BridgeVisuals);
	for (const UNightAtomBridgeVisualComponent* Bridge : BridgeVisuals)
	{
		if (Bridge && Bridge->bEnabled)
		{
			IncludePoint(Bridge->GetRelativeLocation());
		}
	}

	if (!bHasPoint)
	{
		OutMin = FVector(-100.f, -100.f, -100.f);
		OutMax = FVector(100.f, 100.f, 100.f);
		return;
	}

	const FVector Padding(100.f, 100.f, 100.f);
	OutMin -= Padding;
	OutMax += Padding;
}
#pragma endregion K2 moonyfli
