#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightBridgeSegmentActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UMaterialInterface;

#pragma region K2 moonyfli
/** Art bridge board spanning FromStone -> ToStone. */
UCLASS(Blueprintable)
class MINIGAME_API ANightBridgeSegmentActor : public AActor
{
	GENERATED_BODY()

public:
	ANightBridgeSegmentActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<USceneComponent> ArtRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> BridgeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge")
	TObjectPtr<UStaticMesh> BridgeMeshOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge")
	TObjectPtr<UMaterialInterface> BridgeMaterialOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge|Transform")
	float BridgeScaleMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge|Transform")
	FVector BridgePivotOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge|Collision")
	bool bBridgeCollisionEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	FNightBridgeSpec Spec;

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetupBridge(
		const FNightBridgeSpec& InSpec,
		UStaticMesh* MeshOverride = nullptr,
		UMaterialInterface* MaterialOverride = nullptr,
		const FVector& PivotOffsetCm = FVector::ZeroVector,
		float GlobalScaleMultiplier = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void ApplyMesh(UStaticMesh* Mesh);
};
#pragma endregion K2 moonyfli
