#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightCourseStoneActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UMaterialInterface;
class UMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

#pragma region K2 moonyfli
/** Single stepping stone; optional foe capsule on top (刃心 whitebox). */
UCLASS(Blueprintable)
class MINIGAME_API ANightCourseStoneActor : public AActor
{
	GENERATED_BODY()

public:
	ANightCourseStoneActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<USceneComponent> ArtRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> PlatformMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> FoeCapsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<USkeletalMeshComponent> FoeSkeletalMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Visual")
	TObjectPtr<USkeletalMesh> FoeSkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Visual")
	TObjectPtr<UStaticMesh> FoeStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Visual")
	TObjectPtr<UMaterialInterface> FoeMaterial;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	FNightStoneSpec Spec;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	int32 StoneIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	FLinearColor PadColor = FLinearColor(0.55f, 0.55f, 0.62f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	FLinearColor FoeColor = FLinearColor(0.95f, 0.2f, 0.18f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Foe Transform")
	float FoeYawOffsetDeg = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Foe Transform")
	float FoeScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Foe Transform")
	float FoeHeightOffsetCm = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Foe Transform")
	FVector FoePivotOffsetCm = FVector::ZeroVector;

	/** Additional correction for this Blueprint's model root. Positive values move the visible foe up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Foe Transform")
	float FoeZCompensationCm = 0.f;

	/**
	 * Controls only editor previews. Runtime-spawned foes always apply FoeZCompensationCm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Foe Transform")
	bool bApplyFoeZCompensationInPreview = true;

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetupStone(int32 InIndex, const FNightStoneSpec& InSpec);

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void ApplyFoeMesh(UStaticMesh* Mesh, UMaterialInterface* MaterialOverride = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void ApplyConfiguredFoeVisual();

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void ShowFoe();

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void SetFoeArtTransform(
		float YawOffsetDeg,
		float Scale,
		float HeightOffsetCm,
		const FVector& PivotOffsetCm = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void ApplyFoeZCompensation(bool bPreview);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetTrackPose(const FVector& WorldLocation, const FRotator& WorldRotation);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetHighlight(bool bHighlight);

	/** Hide/despawn foe after a successful Attack into this stone. */
	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void ClearFoe(bool bAnimate);

	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlayFoeClearedVFX();

	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlaySlashVFX();

	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlayDropBurst(EIngredientId DropId, int32 Count);

	virtual void Tick(float DeltaSeconds) override;

protected:
	void ApplyColors();
	void TintMesh(UMeshComponent* Mesh, const FLinearColor& Color);

	float FoeClearAlpha = 1.f;
	bool bClearingFoe = false;
	FVector FoeRuntimeBaseScale = FVector::OneVector;
	FVector FoeSkeletalRuntimeBaseScale = FVector::OneVector;
	FVector FoeRuntimeBaseLocation = FVector::ZeroVector;
	FVector FoeSkeletalRuntimeBaseLocation = FVector::ZeroVector;
	bool bFoeVisualBaseTransformCached = false;
};
#pragma endregion K2 moonyfli
