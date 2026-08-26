#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Night/Course/NightFeelBridge.h"
#include "NightCourseHUD.generated.h"

class UUserWidget;

#pragma region K2 moonyfli
/** G1 parkour HUD host: composite UMG, soul updates, window prompt and key hints. */
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

	/** Uniform scale of the composite Night HUD, 1 = authored size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MainHUDScale = 0.5f;

	/** Left edge of the composite HUD in design units. 0 = game view left, not the window letterbox. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD")
	float MainHUDLeftMargin = 80.f;

	/** Gap between the top of the viewport and the composite HUD, in design units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD", meta = (ClampMin = "0.0"))
	float MainHUDTopMargin = 24.f;

private:
	/** Main parkour widget: /Game/Night/Course/Blueprints/WBP_NightHUD_Multi. */
	UPROPERTY(EditAnywhere, Category = "Night|HUD")
	TSubclassOf<UUserWidget> MainHUDWidgetClass;

	/** Authored size of the bar inside the WBP: SetHealth pins the fill to x = FullBarWidth. */
	UPROPERTY(EditAnywhere, Category = "Night|HUD")
	FVector2D HealthBarDesignSize = FVector2D(800.f, 240.f);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MainHUDWidget;

	/** Nested WBP_HealthBar instance owned by MainHUDWidget. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HealthBarWidget;

	FVector2D MainHUDPlacement = FVector2D(-1.f, -1.f);

	void EnsureMainHUD();
	void UpdateMainHUDPlacement();
	void PushSoulToHealthBar(float Soul);
	void SetHealthBarNumeric(FName PropertyName, double Value);
};
#pragma endregion K2 moonyfli
