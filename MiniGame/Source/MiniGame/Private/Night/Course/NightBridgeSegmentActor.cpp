#include "Night/Course/NightBridgeSegmentActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
	BridgeMesh->SetRelativeLocation(FVector::ZeroVector);
	BridgeMesh->SetRelativeRotation(FRotator::ZeroRotator);
	BridgeMesh->SetRelativeScale3D(FVector::OneVector);

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
	UMaterialInterface* MaterialOverride,
	const FVector& PivotOffsetCm,
	float GlobalScaleMultiplier)
{
	Spec = InSpec;
	const FTransform BpComponentTransform =
		BridgeMesh ? BridgeMesh->GetRelativeTransform() : FTransform::Identity;
	const bool bHadBpMesh = BridgeMesh && BridgeMesh->GetStaticMesh();
	if (BridgeMeshOverride)
	{
		ApplyMesh(BridgeMeshOverride);
	}
	else if (MeshOverride)
	{
		ApplyMesh(MeshOverride);
	}
	SetActorLocationAndRotation(Spec.WorldLocation, FRotator(0.f, Spec.YawDeg, 0.f));
	if (BridgeMesh)
	{
		const bool bExplicitTransform =
			!FMath::IsNearlyEqual(GlobalScaleMultiplier, 1.f) ||
			!FMath::IsNearlyEqual(BridgeScaleMultiplier, 1.f) ||
			!BridgePivotOffsetCm.IsNearlyZero() ||
			!PivotOffsetCm.IsNearlyZero();
		const bool bHasConfiguredMesh =
			bHadBpMesh || BridgeMeshOverride || MeshOverride;
		if (bExplicitTransform || !bHasConfiguredMesh)
		{
			const float GlobalScale = FMath::Max(
				0.01f,
				FMath::Max(0.01f, GlobalScaleMultiplier)
				* FMath::Max(0.01f, BridgeScaleMultiplier));
			const FVector MeshScale(GlobalScale);
			BridgeMesh->SetRelativeScale3D(MeshScale);
			const FVector MeshCenter = BridgeMesh->GetStaticMesh()
				? BridgeMesh->GetStaticMesh()->GetBounds().Origin
				: FVector::ZeroVector;
			BridgeMesh->SetRelativeLocation(
				BridgeMesh->GetRelativeRotation().RotateVector(
					(BridgePivotOffsetCm + PivotOffsetCm - MeshCenter) * MeshScale));
		}
		else
		{
			// The BP component transform is the source of truth.
			BridgeMesh->SetRelativeTransform(BpComponentTransform);
		}
		BridgeMesh->SetCollisionEnabled(
			bBridgeCollisionEnabled
				? ECollisionEnabled::QueryAndPhysics
				: ECollisionEnabled::NoCollision);
		UMaterialInterface* EffectiveMaterial =
			BridgeMaterialOverride.Get() ? BridgeMaterialOverride.Get() : MaterialOverride;
		if (BridgeMesh->GetStaticMesh() && EffectiveMaterial)
		{
			for (int32 MaterialIndex = 0; MaterialIndex < BridgeMesh->GetNumMaterials(); ++MaterialIndex)
			{
				BridgeMesh->SetMaterial(MaterialIndex, EffectiveMaterial);
			}
		}
		if (BridgeMesh->GetStaticMesh() && BridgeMesh->GetMaterial(0))
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
