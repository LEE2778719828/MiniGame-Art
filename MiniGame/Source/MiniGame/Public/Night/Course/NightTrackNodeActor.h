#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightTrackNodeActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;

#pragma region K2 moonyfli
/**
 * 刃心-style whitebox beat:
 * - Jump (Hazard): two platforms with a wide gap
 * - Kill (Enemy): two close platforms + capsule on the far pad
 */
UCLASS(Abstract, Blueprintable)
class MINIGAME_API ANightTrackNodeActor : public AActor
{
	GENERATED_BODY()

public:
	ANightTrackNodeActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<USceneComponent> ArtRoot;

	/** Near pad (player approaches from behind). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> PlatformA;

	/** Far pad. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> PlatformB;

	/** Enemy body; only visible for Enemy nodes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> FoeCapsule;

	/** Kept for BP/compat; aliases PlatformA. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	FNightTrackNodeSpec Spec;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	int32 NodeIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	FLinearColor DebugColor = FLinearColor::White;

	/** Center-to-center gap along track for Jump beats. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Layout")
	float JumpGapCm = 420.f;

	/** Center-to-center gap along track for Kill beats. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Layout")
	float KillGapCm = 140.f;

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

protected:
	void BuildPlatformLayout(ENightNodeKind Kind);
	void TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color);
};
#pragma endregion K2 moonyfli
