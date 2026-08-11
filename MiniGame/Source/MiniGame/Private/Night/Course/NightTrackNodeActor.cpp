#include "Night/Course/NightTrackNodeActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma region K2 moonyfli
ANightTrackNodeActor::ANightTrackNodeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitMat(TEXT("/Game/Night/Course/Materials/M_NightUnlitColor.M_NightUnlitColor"));
	if (UnlitMat.Succeeded())
	{
		MeshComponent->SetMaterial(0, UnlitMat.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (ShapeMat.Succeeded())
		{
			MeshComponent->SetMaterial(0, ShapeMat.Object);
		}
	}
}

void ANightTrackNodeActor::SetupNode(int32 InIndex, const FNightTrackNodeSpec& InSpec)
{
	NodeIndex = InIndex;
	Spec = InSpec;
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
	SetActorScale3D(GetActorScale3D() * 1.35f);
	ApplyDebugColor(FLinearColor(1.f, 0.85f, 0.15f));
}

void ANightTrackNodeActor::OnResolved_Implementation(ENightJudgeOutcome Outcome)
{
	if (Outcome == ENightJudgeOutcome::Success)
	{
		ApplyDebugColor(FLinearColor(0.2f, 1.f, 0.35f));
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

void ANightTrackNodeActor::ApplyDebugColor(FLinearColor Color)
{
	if (!MeshComponent)
	{
		return;
	}

	UMaterialInterface* BaseMat = MeshComponent->GetMaterial(0);
	if (!BaseMat)
	{
		return;
	}

	UMaterialInstanceDynamic* MID = MeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMat);
	if (!MID)
	{
		return;
	}

	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
}
#pragma endregion K2 moonyfli
