#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightCourseStoneActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

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

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	FNightStoneSpec Spec;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	int32 StoneIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	FLinearColor PadColor = FLinearColor(0.55f, 0.55f, 0.62f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	FLinearColor FoeColor = FLinearColor(0.95f, 0.2f, 0.18f);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetupStone(int32 InIndex, const FNightStoneSpec& InSpec);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetTrackPose(const FVector& WorldLocation, const FRotator& WorldRotation);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetHighlight(bool bHighlight);

	/** Hide/despawn foe after a successful Attack into this stone. */
	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void ClearFoe(bool bAnimate);

	/** Swap to translucent fade material (Color/Opacity/FadeAlpha) when provided. */
	UFUNCTION(BlueprintCallable, Category = "Night|Fade")
	void ConfigureDistanceFadeMaterial(UMaterialInterface* FadeMaterial, const FNightDistanceFadeSettings& Settings);

	/** Apply computed opacity (0..1). Updates MIDs and optional hide. */
	UFUNCTION(BlueprintCallable, Category = "Night|Fade")
	void ApplyDistanceFade(float Opacity01, const FNightDistanceFadeSettings& Settings);

	UFUNCTION(BlueprintPure, Category = "Night|Fade")
	float GetCurrentFadeOpacity() const { return CurrentFadeOpacity; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlayFoeClearedVFX();

	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlaySlashVFX();

	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlayDropBurst(EIngredientId DropId, int32 Count);

	virtual void Tick(float DeltaSeconds) override;

protected:
	void ApplyColors();
	void TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color);
	void EnsureMeshMids();
	void PushFadeToMid(UMaterialInstanceDynamic* Mid, float Opacity01, const FNightDistanceFadeSettings& Settings);

	float FoeClearAlpha = 1.f;
	bool bClearingFoe = false;
	float CurrentFadeOpacity = 1.f;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FadeMaterialParent;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PlatformMid;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FoeMid;

	FNightDistanceFadeSettings CachedFadeSettings;
	bool bFadeMaterialConfigured = false;
};
#pragma endregion K2 moonyfli
