#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "NightCoursePreviewCamera.generated.h"

class ANightCourseHost;

#pragma region K2 moonyfli
/**
 * Editor-only camera the user can drag in the Night preview. Moving it writes SpringArm /
 * FOV back onto the HeroClass pawn (BP_NightCoursePawn).
 */
UCLASS(NotPlaceable, Transient)
class MINIGAME_API ANightCoursePreviewCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	ANightCoursePreviewCamera();

	UPROPERTY(VisibleAnywhere, Category = "Night|Camera Bake")
	float BakedArmLength = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Night|Camera Bake")
	FRotator BakedBoomRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, Category = "Night|Camera Bake")
	FVector BakedSocketOffset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Night|Camera Bake")
	float BakedFieldOfView = 70.f;

	void SetPreviewHost(ANightCourseHost* InHost);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
#endif

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ANightCourseHost> PreviewHost;

	void NotifyHostEdited();
};
#pragma endregion K2 moonyfli
