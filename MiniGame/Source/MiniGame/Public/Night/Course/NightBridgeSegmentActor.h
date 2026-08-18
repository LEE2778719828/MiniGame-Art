#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightBridgeSegmentActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;

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

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	FNightBridgeSpec Spec;

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetupBridge(const FNightBridgeSpec& InSpec, UStaticMesh* MeshOverride = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void ApplyMesh(UStaticMesh* Mesh);
};
#pragma endregion K2 moonyfli
