#include "Night/Course/NightCourseStoneActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	PlatformMesh->SetRelativeLocation(FVector(0.f, 0.f, 18.f));
	PlatformMesh->SetRelativeScale3D(FVector(1.8f, 1.8f, 0.35f));
	// Course footholds are rendered by bridge segments; this component remains
	// only as a compatibility hook for old stone-chain layouts.
	PlatformMesh->SetHiddenInGame(true);
	PlatformMesh->SetVisibility(false);

	FoeCapsule = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FoeCapsule"));
	FoeCapsule->SetupAttachment(PlatformMesh);
	FoeCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FoeCapsule->SetRelativeLocation(FVector(0.f, 0.f, 95.f));
	FoeCapsule->SetRelativeScale3D(FVector(0.35f));
	FoeCapsule->SetHiddenInGame(true);
	FoeCapsule->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FishMesh(
		TEXT("/Game/Night/Course/Art/Foe/fish.fish"));
	if (CubeMesh.Succeeded())
	{
		PlatformMesh->SetStaticMesh(CubeMesh.Object);
	}
	if (CapsuleMesh.Succeeded())
	{
		FoeCapsule->SetStaticMesh(CapsuleMesh.Object);
	}
	if (FishMesh.Succeeded())
	{
		FoeCapsule->SetStaticMesh(FishMesh.Object);
		FoeCapsule->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
		FoeCapsule->SetRelativeScale3D(FVector(0.35f));
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitMat(TEXT("/Game/Night/Course/Materials/M_NightUnlitColor.M_NightUnlitColor"));
	if (UnlitMat.Succeeded())
	{
		PlatformMesh->SetMaterial(0, UnlitMat.Object);
		FoeCapsule->SetMaterial(0, UnlitMat.Object);
	}
}

void ANightCourseStoneActor::SetupStone(int32 InIndex, const FNightStoneSpec& InSpec)
{
	StoneIndex = InIndex;
	Spec = InSpec;
	bClearingFoe = false;
	FoeClearAlpha = 1.f;

	const bool bShowFoe = Spec.bHasFoe;
	FoeCapsule->SetHiddenInGame(!bShowFoe);
	FoeCapsule->SetVisibility(bShowFoe);
	FoeCapsule->SetRelativeScale3D(FVector(0.35f));
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
	if (!Spec.bHasFoe && (!FoeCapsule || FoeCapsule->bHiddenInGame))
	{
		return;
	}

	Spec.bHasFoe = false;
	PlayFoeClearedVFX();
	PlaySlashVFX();

	if (!bAnimate)
	{
		FoeCapsule->SetHiddenInGame(true);
		FoeCapsule->SetVisibility(false);
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
	if (!bClearingFoe || !FoeCapsule)
	{
		return;
	}

	FoeClearAlpha -= DeltaSeconds * 2.5f;
	const float Alpha = FMath::Clamp(FoeClearAlpha, 0.f, 1.f);
		FoeCapsule->SetRelativeScale3D(FVector(0.35f) * Alpha);
	FoeCapsule->AddLocalRotation(FRotator(0.f, 0.f, 360.f * DeltaSeconds));

	if (Alpha <= 0.01f)
	{
		FoeCapsule->SetHiddenInGame(true);
		FoeCapsule->SetVisibility(false);
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
}

void ANightCourseStoneActor::TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
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
