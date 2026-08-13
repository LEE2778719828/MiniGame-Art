#include "Night/Course/NightTrackNodeActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma region K2 moonyfli
ANightTrackNodeActor::ANightTrackNodeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ArtRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArtRoot"));
	SetRootComponent(ArtRoot);

	PlatformA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformA"));
	PlatformA->SetupAttachment(ArtRoot);
	PlatformA->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlatformB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformB"));
	PlatformB->SetupAttachment(ArtRoot);
	PlatformB->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FoeCapsule = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FoeCapsule"));
	FoeCapsule->SetupAttachment(PlatformB);
	FoeCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FoeCapsule->SetHiddenInGame(true);
	FoeCapsule->SetVisibility(false);

	MeshComponent = PlatformA;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CubeMesh.Succeeded())
	{
		PlatformA->SetStaticMesh(CubeMesh.Object);
		PlatformB->SetStaticMesh(CubeMesh.Object);
	}
	if (CapsuleMesh.Succeeded())
	{
		FoeCapsule->SetStaticMesh(CapsuleMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitMat(TEXT("/Game/Night/Course/Materials/M_NightUnlitColor.M_NightUnlitColor"));
	if (UnlitMat.Succeeded())
	{
		PlatformA->SetMaterial(0, UnlitMat.Object);
		PlatformB->SetMaterial(0, UnlitMat.Object);
		FoeCapsule->SetMaterial(0, UnlitMat.Object);
	}

	// Default flat pads; layout finalized in BuildPlatformLayout.
	PlatformA->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.35f));
	PlatformB->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.35f));
	FoeCapsule->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.35f));
	FoeCapsule->SetRelativeLocation(FVector(0.f, 0.f, 95.f));
}

void ANightTrackNodeActor::BuildPlatformLayout(ENightNodeKind Kind)
{
	const bool bKill = (Kind == ENightNodeKind::Enemy);
	const float Gap = bKill ? KillGapCm : JumpGapCm;
	const float Half = Gap * 0.5f;

	PlatformA->SetRelativeLocation(FVector(-Half, 0.f, 18.f));
	PlatformB->SetRelativeLocation(FVector(Half, 0.f, 18.f));

	if (bKill)
	{
		FoeCapsule->SetHiddenInGame(false);
		FoeCapsule->SetVisibility(true);
		FoeCapsule->SetRelativeLocation(FVector(0.f, 0.f, 95.f));
	}
	else
	{
		FoeCapsule->SetHiddenInGame(true);
		FoeCapsule->SetVisibility(false);
	}
}

void ANightTrackNodeActor::SetupNode(int32 InIndex, const FNightTrackNodeSpec& InSpec)
{
	NodeIndex = InIndex;
	Spec = InSpec;
	BuildPlatformLayout(Spec.Kind);
	OnNodeActivated();
}

void ANightTrackNodeActor::SetTrackPose(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	SetActorLocationAndRotation(WorldLocation, WorldRotation);
}

void ANightTrackNodeActor::OnNodeActivated_Implementation()
{
	ApplyDebugColor(DebugColor);
}

void ANightTrackNodeActor::OnJudgeWindowOpened_Implementation()
{
	ApplyDebugColor(FLinearColor(1.f, 0.85f, 0.15f));
}

void ANightTrackNodeActor::OnResolved_Implementation(ENightJudgeOutcome Outcome)
{
	if (Outcome == ENightJudgeOutcome::Success)
	{
		ApplyDebugColor(FLinearColor(0.2f, 1.f, 0.35f));
		if (FoeCapsule)
		{
			FoeCapsule->SetHiddenInGame(true);
			FoeCapsule->SetVisibility(false);
		}
	}
	else
	{
		ApplyDebugColor(FLinearColor(1.f, 0.4f, 0.1f));
	}
}

void ANightTrackNodeActor::OnDespawnRequested_Implementation()
{
	Destroy();
}

void ANightTrackNodeActor::TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return;
	}
	UMaterialInterface* BaseMat = Mesh->GetMaterial(0);
	if (!BaseMat)
	{
		return;
	}
	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMat))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ANightTrackNodeActor::ApplyDebugColor(FLinearColor Color)
{
	TintMesh(PlatformA, Color);
	TintMesh(PlatformB, Color);
	if (FoeCapsule && FoeCapsule->IsVisible())
	{
		// Enemy body stays readable red while pads flash yellow/green.
		const FLinearColor CapsuleColor = (Spec.Kind == ENightNodeKind::Enemy)
			? FLinearColor(0.95f, 0.2f, 0.18f)
			: Color;
		TintMesh(FoeCapsule, CapsuleColor);
	}
}
#pragma endregion K2 moonyfli
