#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightCoursePawn.h" //add by K2
#include "Night/Course/NightFoeShatterComponent.h"
#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h" //add by K2

#pragma region K2 moonyfli
ANightCourseStoneActor::ANightCourseStoneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ArtRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArtRoot"));
	SetRootComponent(ArtRoot);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(ArtRoot);
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlatformMesh->SetRelativeLocation(FVector::ZeroVector);
	PlatformMesh->SetRelativeScale3D(FVector::OneVector);
	// Course footholds are rendered by bridge segments; this component remains
	// only as a compatibility hook for old stone-chain layouts.
	PlatformMesh->SetHiddenInGame(true);
	PlatformMesh->SetVisibility(false);

	FoeCapsule = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FoeCapsule"));
	FoeCapsule->SetupAttachment(ArtRoot);
	FoeCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FoeCapsule->SetRelativeLocation(FVector::ZeroVector);
	FoeCapsule->SetRelativeScale3D(FVector::OneVector);
	FoeCapsule->SetHiddenInGame(true);
	FoeCapsule->SetVisibility(false);

	FoeSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FoeSkeletalMesh"));
	FoeSkeletalMeshComponent->SetupAttachment(ArtRoot);
	FoeSkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FoeSkeletalMeshComponent->SetHiddenInGame(true);
	FoeSkeletalMeshComponent->SetVisibility(false);

	FoeShatter = CreateDefaultSubobject<UNightFoeShatterComponent>(TEXT("FoeShatter"));
	FoeShatter->SetupAttachment(ArtRoot);
}

void ANightCourseStoneActor::SetupStone(int32 InIndex, const FNightStoneSpec& InSpec)
{
	StoneIndex = InIndex;
	Spec = InSpec;
	bClearingFoe = false;
	FoeClearAlpha = 1.f;
	if (FoeShatter)
	{
		FoeShatter->ClearShards();
	}
	ApplyConfiguredFoeVisual();

	const bool bShowFoe = Spec.bHasFoe;
	const bool bUseSkeletalFoe =
		FoeSkeletalMeshComponent && FoeSkeletalMeshComponent->GetSkeletalMeshAsset();
	FoeCapsule->SetHiddenInGame(!bShowFoe || bUseSkeletalFoe);
	FoeCapsule->SetVisibility(bShowFoe && !bUseSkeletalFoe);
	if (!bUseSkeletalFoe &&
		(!FoeCapsule->GetStaticMesh()) &&
		(!FMath::IsNearlyEqual(FoeScale, 0.6f) ||
			!FMath::IsNearlyEqual(FoeYawOffsetDeg, 90.f) ||
			!FMath::IsNearlyEqual(FoeHeightOffsetCm, 70.f) ||
			!FoePivotOffsetCm.IsNearlyZero()))
	{
		FoeCapsule->SetRelativeScale3D(FVector(FoeScale));
	}
	if (FoeSkeletalMeshComponent)
	{
		FoeSkeletalMeshComponent->SetHiddenInGame(!bShowFoe || !bUseSkeletalFoe);
		FoeSkeletalMeshComponent->SetVisibility(bShowFoe && bUseSkeletalFoe);
	}
	FoeRuntimeBaseScale = FoeCapsule
		? FoeCapsule->GetRelativeScale3D()
		: FVector::OneVector;
	FoeSkeletalRuntimeBaseScale = FoeSkeletalMeshComponent
		? FoeSkeletalMeshComponent->GetRelativeScale3D()
		: FVector::OneVector;
	FoeRuntimeBaseLocation = FoeCapsule
		? FoeCapsule->GetRelativeLocation()
		: FVector::ZeroVector;
	FoeSkeletalRuntimeBaseLocation = FoeSkeletalMeshComponent
		? FoeSkeletalMeshComponent->GetRelativeLocation()
		: FVector::ZeroVector;
	bFoeVisualBaseTransformCached = true;
	ApplyFoeZCompensation(false);
	ApplyColors();
}

void ANightCourseStoneActor::ApplyFoeZCompensation(bool bPreview)
{
	if (!bFoeVisualBaseTransformCached)
	{
		FoeRuntimeBaseLocation = FoeCapsule
			? FoeCapsule->GetRelativeLocation()
			: FVector::ZeroVector;
		FoeSkeletalRuntimeBaseLocation = FoeSkeletalMeshComponent
			? FoeSkeletalMeshComponent->GetRelativeLocation()
			: FVector::ZeroVector;
		bFoeVisualBaseTransformCached = true;
	}

	const bool bApplyCompensation =
		!bPreview || bApplyFoeZCompensationInPreview;
	const float AppliedOffset = bApplyCompensation
		? FoeZCompensationCm
		: 0.f;
	if (FoeCapsule)
	{
		FVector Location = FoeRuntimeBaseLocation;
		Location.Z += AppliedOffset;
		FoeCapsule->SetRelativeLocation(Location);
	}
	if (FoeSkeletalMeshComponent)
	{
		FVector Location = FoeSkeletalRuntimeBaseLocation;
		Location.Z += AppliedOffset;
		FoeSkeletalMeshComponent->SetRelativeLocation(Location);
	}
}

void ANightCourseStoneActor::ApplyConfiguredFoeVisual()
{
	const bool bExplicitTransform =
		!FMath::IsNearlyEqual(FoeScale, 0.6f) ||
		!FMath::IsNearlyEqual(FoeYawOffsetDeg, 90.f) ||
		!FMath::IsNearlyEqual(FoeHeightOffsetCm, 70.f) ||
		!FoePivotOffsetCm.IsNearlyZero();

	const bool bHadComponentSkeletalMesh =
		FoeSkeletalMeshComponent && FoeSkeletalMeshComponent->GetSkeletalMeshAsset();
	if (FoeSkeletalMeshComponent)
	{
		if (!FoeSkeletalMeshComponent->GetSkeletalMeshAsset() && FoeSkeletalMesh)
		{
			FoeSkeletalMeshComponent->SetSkeletalMesh(FoeSkeletalMesh);
		}
		if (FoeSkeletalMeshComponent->GetSkeletalMeshAsset())
		{
			if (!bHadComponentSkeletalMesh && (FoeSkeletalMesh || bExplicitTransform))
			{
				FoeSkeletalMeshComponent->SetRelativeRotation(FRotator(0.f, FoeYawOffsetDeg, 0.f));
				FoeSkeletalMeshComponent->SetRelativeScale3D(FVector(FoeScale));
				FoeSkeletalMeshComponent->SetRelativeLocation(
					FVector(0.f, 0.f, FoeHeightOffsetCm) + FoePivotOffsetCm);
			}
			if (FoeMaterial)
			{
				FoeSkeletalMeshComponent->SetMaterial(0, FoeMaterial);
			}
			FoeCapsule->SetVisibility(false);
			FoeCapsule->SetHiddenInGame(true);
			return;
		}
	}

	// A mesh assigned directly on the BP's inherited component is authoritative.
	// This prevents the optional profile slot from overwriting an editor change.
	if (FoeCapsule && FoeCapsule->GetStaticMesh())
	{
		if (FoeMaterial)
		{
			for (int32 MaterialIndex = 0; MaterialIndex < FoeCapsule->GetNumMaterials(); ++MaterialIndex)
			{
				FoeCapsule->SetMaterial(MaterialIndex, FoeMaterial);
			}
		}
		return;
	}

	if (FoeStaticMesh)
	{
		ApplyFoeMesh(FoeStaticMesh, FoeMaterial);
	}
}

void ANightCourseStoneActor::ApplyFoeMesh(UStaticMesh* Mesh, UMaterialInterface* MaterialOverride)
{
	if (!FoeCapsule || !Mesh)
	{
		return;
	}

	FoeCapsule->SetStaticMesh(Mesh);
	// ArtSubmit models use a different forward axis than the course lane.
	FoeCapsule->SetRelativeRotation(FRotator(0.f, FoeYawOffsetDeg, 0.f));
	const FVector MeshCenter = Mesh->GetBounds().Origin;
	FoeCapsule->SetRelativeScale3D(FVector(FoeScale));
	const FRotator FoeRotation(0.f, FoeYawOffsetDeg, 0.f);
	FoeCapsule->SetRelativeLocation(
		FVector(0.f, 0.f, FoeHeightOffsetCm)
		+ FoeRotation.RotateVector(-MeshCenter * FoeScale));
	UMaterialInterface* FoeMat = MaterialOverride;
	if (FoeMat)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < FoeCapsule->GetNumMaterials(); ++MaterialIndex)
		{
			FoeCapsule->SetMaterial(MaterialIndex, FoeMat);
		}
		if (UMaterialInstanceDynamic* MID =
			FoeCapsule->CreateAndSetMaterialInstanceDynamicFromMaterial(0, FoeMat))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FoeColor);
		}
	}
}

void ANightCourseStoneActor::SetFoeArtTransform(
	float YawOffsetDeg,
	float Scale,
	float HeightOffsetCm,
	const FVector& PivotOffsetCm)
{
	FoeYawOffsetDeg = YawOffsetDeg;
	FoeScale = FMath::Max(0.01f, Scale);
	FoeHeightOffsetCm = HeightOffsetCm;
	FoePivotOffsetCm = PivotOffsetCm;
	if (FoeCapsule)
	{
		FoeCapsule->SetRelativeRotation(FRotator(0.f, FoeYawOffsetDeg, 0.f));
		const FVector MeshCenter = FoeCapsule->GetStaticMesh()
			? FoeCapsule->GetStaticMesh()->GetBounds().Origin
			: FVector::ZeroVector;
		const FRotator FoeRotation(0.f, FoeYawOffsetDeg, 0.f);
		FoeCapsule->SetRelativeLocation(
			FVector(0.f, 0.f, FoeHeightOffsetCm)
			+ FoeRotation.RotateVector((FoePivotOffsetCm - MeshCenter) * FoeScale));
		FoeCapsule->SetRelativeScale3D(FVector(FoeScale));
		FoeRuntimeBaseScale = FoeCapsule->GetRelativeScale3D();
		FoeRuntimeBaseLocation = FoeCapsule->GetRelativeLocation();
		bFoeVisualBaseTransformCached = true;
		ApplyFoeZCompensation(false);
	}
}

void ANightCourseStoneActor::ShowFoe()
{
	Spec.bHasFoe = true;
	bClearingFoe = false;
	FoeClearAlpha = 1.f;
	const bool bUseSkeletalFoe =
		FoeSkeletalMeshComponent && FoeSkeletalMeshComponent->GetSkeletalMeshAsset();
	if (FoeCapsule)
	{
		FoeCapsule->SetHiddenInGame(bUseSkeletalFoe);
		FoeCapsule->SetVisibility(!bUseSkeletalFoe);
	}
	if (FoeSkeletalMeshComponent)
	{
		FoeSkeletalMeshComponent->SetHiddenInGame(!bUseSkeletalFoe);
		FoeSkeletalMeshComponent->SetVisibility(bUseSkeletalFoe);
	}
	ApplyColors();
}

void ANightCourseStoneActor::SetTrackPose(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	SetActorLocationAndRotation(WorldLocation, WorldRotation);
}

void ANightCourseStoneActor::SetHighlight(bool bHighlight)
{
	ApplyColors();
	if (bHighlight)
	{
		TintMesh(PlatformMesh, FLinearColor(1.f, 0.85f, 0.2f));
	}
}

void ANightCourseStoneActor::ClearFoe(bool bAnimate)
{
	const bool bCapsuleVisible = FoeCapsule && FoeCapsule->IsVisible();
	const bool bSkeletalVisible =
		FoeSkeletalMeshComponent
		&& FoeSkeletalMeshComponent->GetSkeletalMeshAsset()
		&& FoeSkeletalMeshComponent->IsVisible();
	if (!Spec.bHasFoe && !bCapsuleVisible && !bSkeletalVisible)
	{
		return;
	}

	Spec.bHasFoe = false;

#pragma region K2 moonyfli
	FVector HitLoc = GetActorLocation() + FVector(0.f, 0.f, FoeHeightOffsetCm);
	UMeshComponent* FoeMesh = nullptr;
	if (bSkeletalVisible && FoeSkeletalMeshComponent)
	{
		HitLoc = FoeSkeletalMeshComponent->GetComponentLocation();
		FoeMesh = FoeSkeletalMeshComponent;
	}
	else if (bCapsuleVisible && FoeCapsule)
	{
		HitLoc = FoeCapsule->GetComponentLocation();
		FoeMesh = FoeCapsule;
	}

	ANightCoursePawn* Hero = Cast<ANightCoursePawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Hero)
	{
		Hero->PlayAttackVFX(HitLoc);
	}

	bool bPlayedShatter = false;
	if (bAnimate && FoeShatter)
	{
		FNightFoeShatterRequest Request;
		Request.Origin = HitLoc;
		Request.ImpulseDir = Hero ? Hero->GetActorForwardVector() : GetActorForwardVector();
		if (Hero)
		{
			const FVector Through = (HitLoc - Hero->GetActorLocation()).GetSafeNormal();
			if (!Through.IsNearlyZero())
			{
				Request.ImpulseDir = (Request.ImpulseDir * 0.65f + Through * 0.35f).GetSafeNormal();
			}
		}
		Request.Tint = FoeColor;
		Request.Material = FoeMaterial;
		Request.SourceMeshComponent = FoeMesh;
		Request.SourceStaticMesh = FoeStaticMesh;
		if (const UStaticMeshComponent* SourceStatic = Cast<UStaticMeshComponent>(FoeMesh))
		{
			if (SourceStatic->GetStaticMesh())
			{
				Request.SourceStaticMesh = SourceStatic->GetStaticMesh();
			}
		}
		if (FoeMesh)
		{
			if (!Request.Material)
			{
				Request.Material = FoeMesh->GetMaterial(0);
			}
			Request.Origin = FoeMesh->Bounds.Origin;
			Request.Extent = FoeMesh->Bounds.BoxExtent;
		}
		if (Hero)
		{
			Request.Combo = Hero->GetSlashCombo();
			Request.VFXTier = Hero->ResolveAttackVFXTier();
			Request.bTier5 = Request.Combo >= Hero->SimulatedVFXTier5Combo;
		}
		bPlayedShatter = FoeShatter->PlayBurst(Request);
	}
#pragma endregion K2 moonyfli

	PlayFoeClearedVFX();
	PlaySlashVFX();

	if (!bAnimate || bPlayedShatter)
	{
		if (FoeCapsule)
		{
			FoeCapsule->SetHiddenInGame(true);
			FoeCapsule->SetVisibility(false);
		}
		if (FoeSkeletalMeshComponent)
		{
			FoeSkeletalMeshComponent->SetHiddenInGame(true);
			FoeSkeletalMeshComponent->SetVisibility(false);
		}
		SetActorTickEnabled(false);
		return;
	}

	bClearingFoe = true;
	FoeClearAlpha = 1.f;
	SetActorTickEnabled(true);
}

void ANightCourseStoneActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bClearingFoe)
	{
		return;
	}

	FoeClearAlpha -= DeltaSeconds * 2.5f;
	const float Alpha = FMath::Clamp(FoeClearAlpha, 0.f, 1.f);
	if (FoeCapsule)
	{
		FoeCapsule->SetRelativeScale3D(FoeRuntimeBaseScale * Alpha);
		FoeCapsule->AddLocalRotation(FRotator(0.f, 0.f, 360.f * DeltaSeconds));
	}
	if (FoeSkeletalMeshComponent && FoeSkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		FoeSkeletalMeshComponent->SetRelativeScale3D(
			FoeSkeletalRuntimeBaseScale * Alpha);
		FoeSkeletalMeshComponent->AddLocalRotation(
			FRotator(0.f, 0.f, 360.f * DeltaSeconds));
	}

	if (Alpha <= 0.01f)
	{
		if (FoeCapsule)
		{
			FoeCapsule->SetHiddenInGame(true);
			FoeCapsule->SetVisibility(false);
			FoeCapsule->SetRelativeScale3D(FoeRuntimeBaseScale);
		}
		if (FoeSkeletalMeshComponent)
		{
			FoeSkeletalMeshComponent->SetHiddenInGame(true);
			FoeSkeletalMeshComponent->SetVisibility(false);
			FoeSkeletalMeshComponent->SetRelativeScale3D(
				FoeSkeletalRuntimeBaseScale);
		}
		bClearingFoe = false;
		SetActorTickEnabled(false);
	}
}

void ANightCourseStoneActor::ApplyColors()
{
	TintMesh(PlatformMesh, PadColor);
	if (FoeCapsule && FoeCapsule->IsVisible())
	{
		TintMesh(FoeCapsule, FoeColor);
	}
	if (FoeSkeletalMeshComponent
		&& FoeSkeletalMeshComponent->GetSkeletalMeshAsset()
		&& FoeSkeletalMeshComponent->IsVisible())
	{
		TintMesh(FoeSkeletalMeshComponent, FoeColor);
	}
}

void ANightCourseStoneActor::TintMesh(UMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return;
	}
	UMaterialInterface* Base = Mesh->GetMaterial(0);
	if (!Base)
	{
		return;
	}
	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Base))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}
#pragma endregion K2 moonyfli
