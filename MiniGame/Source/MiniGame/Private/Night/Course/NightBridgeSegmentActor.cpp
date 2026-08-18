#include "Night/Course/NightBridgeSegmentActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma region K2 moonyfli
ANightBridgeSegmentActor::ANightBridgeSegmentActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ArtRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArtRoot"));
	SetRootComponent(ArtRoot);

	BridgeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BridgeMesh"));
	BridgeMesh->SetupAttachment(ArtRoot);
	BridgeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BridgeMesh->SetCollisionProfileName(TEXT("BlockAll"));
	BridgeMesh->SetRelativeLocation(FVector(0.f, 0.f, 8.f));
	// The imported bridge's long axis is local Y; rotate it onto course forward.
	BridgeMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	BridgeMesh->SetRelativeScale3D(FVector::OneVector);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BridgeMesh->SetStaticMesh(CubeMesh.Object);
	}

	// Prefer imported muban when present; falls back to cube above.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MubanA(
		TEXT("/Game/Night/Course/Art/Bridge/muban1.muban1"));
	if (MubanA.Succeeded())
	{
		BridgeMesh->SetStaticMesh(MubanA.Object);
		BridgeMesh->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> NightMat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (NightMat.Succeeded())
	{
		BridgeMesh->SetMaterial(0, NightMat.Object);
	}
}

void ANightBridgeSegmentActor::ApplyMesh(UStaticMesh* Mesh)
{
	if (BridgeMesh && Mesh)
	{
		BridgeMesh->SetStaticMesh(Mesh);
	}
}

void ANightBridgeSegmentActor::SetupBridge(
	const FNightBridgeSpec& InSpec,
	UStaticMesh* MeshOverride,
	UMaterialInterface* MaterialOverride)
{
	Spec = InSpec;
	if (MeshOverride)
	{
		ApplyMesh(MeshOverride);
	}
	SetActorLocationAndRotation(Spec.WorldLocation, FRotator(0.f, Spec.YawDeg, 0.f));
	if (BridgeMesh)
	{
		const FVector Base = FVector::OneVector;
		BridgeMesh->SetRelativeScale3D(FVector(
			12.f * Base.X,
			FMath::Max(0.05f, Spec.LengthScale) * Base.Y,
			4.f * Base.Z));
		const FVector MeshCenter = BridgeMesh->GetStaticMesh()
			? BridgeMesh->GetStaticMesh()->GetBounds().Origin
			: FVector::ZeroVector;
		const FVector MeshScale = BridgeMesh->GetRelativeScale3D();
		BridgeMesh->SetRelativeLocation(
			-BridgeMesh->GetRelativeRotation().RotateVector(MeshCenter * MeshScale));
		if (MaterialOverride)
		{
			for (int32 MaterialIndex = 0; MaterialIndex < BridgeMesh->GetNumMaterials(); ++MaterialIndex)
			{
				BridgeMesh->SetMaterial(MaterialIndex, MaterialOverride);
			}
		}
		if (UMaterialInstanceDynamic* MID =
			BridgeMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BridgeMesh->GetMaterial(0)))
		{
			const FLinearColor BridgeColor = Spec.MeshVariant == 0
				? FLinearColor(0.12f, 0.55f, 0.95f)
				: FLinearColor(0.95f, 0.35f, 0.12f);
			MID->SetVectorParameterValue(TEXT("Color"), BridgeColor);
		}
	}
}
#pragma endregion K2 moonyfli
