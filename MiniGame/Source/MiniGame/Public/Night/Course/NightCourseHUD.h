#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Night/Course/NightFeelBridge.h"
#include "NightCourseHUD.generated.h"

class UUserWidget;

#pragma region K2 moonyfli
/** Simple readable G1 HUD: soul, window prompt, key hints. */
UCLASS()
class MINIGAME_API ANightCourseHUD : public AHUD
{
	GENERATED_BODY()

public:
	ANightCourseHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;

	/**
	 * add by K2 (R1): maps a screen point onto the two on-screen Jump / Attack pads.
	 * The pads are the same rectangles DrawHUD paints; false means the pointer missed both.
	 */
	static bool HitTestActionButtons(
		float ScreenX,
		float ScreenY,
		float ViewX,
		float ViewY,
		ENightFeelInput& OutInput);

	/** Uniform shrink of the bar, 1 = authored size. Scale from the top-left so Left Margin stays put. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float HealthBarScale = 0.5f;

	/** Left edge of the bar in design units. 0 = game view left, not the window letterbox. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD")
	float HealthBarLeftMargin = 80.f;

	/** Gap between the top of the viewport and the bar, in design units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD", meta = (ClampMin = "0.0"))
	float HealthBarTopMargin = 24.f;

private:
	/** Art widget: /Game/Night/Course/Blueprints/WBP_HealthBar, driven by Feel->Soul. */
	UPROPERTY(EditAnywhere, Category = "Night|HUD")
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

	/** Authored size of the bar inside the WBP: SetHealth pins the fill to x = FullBarWidth. */
	UPROPERTY(EditAnywhere, Category = "Night|HUD")
	FVector2D HealthBarDesignSize = FVector2D(800.f, 240.f);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HealthBarWidget;

	FVector2D HealthBarPlacement = FVector2D(-1.f, -1.f);

	void EnsureHealthBar();
	void UpdateHealthBarPlacement();
	void PushSoulToHealthBar(float Soul);
	void SetHealthBarNumeric(FName PropertyName, double Value);
};
#pragma endregion K2 moonyfli
