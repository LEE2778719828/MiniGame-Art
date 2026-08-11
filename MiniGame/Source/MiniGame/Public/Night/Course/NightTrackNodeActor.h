#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightTrackNodeActor.generated.h"

class UStaticMeshComponent;

#pragma region K2 moonyfli
/**
 * Base whitebox node. Artists subclass BP and override Presentation events;
 * Course only calls Activate/Resolve/Despawn.
 */
UCLASS(Abstract, Blueprintable)
class MINIGAME_API ANightTrackNodeActor : public AActor
{
	GENERATED_BODY()

public:
	ANightTrackNodeActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	FNightTrackNodeSpec Spec;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	int32 NodeIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	FLinearColor DebugColor = FLinearColor::White;

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetupNode(int32 InIndex, const FNightTrackNodeSpec& InSpec);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetTrackPose(const FVector& WorldLocation, const FRotator& WorldRotation);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Art")
	void OnNodeActivated();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Art")
	void OnJudgeWindowOpened();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Art")
	void OnResolved(ENightJudgeOutcome Outcome);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Art")
	void OnDespawnRequested();

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void ApplyDebugColor(FLinearColor Color);
};
#pragma endregion K2 moonyfli
