#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightCourseHUD.generated.h"

class UUserWidget;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnNightHUDResultReady,
	const FNightResult&,
	Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNightHUDResultContinueRequested);

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

	/** Legacy placement scale. Only used when bUseLegacyHUDPlacement is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MainHUDScale = 0.5f;

	/** Left edge of the composite HUD in design units. 0 = game view left, not the window letterbox. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD")
	float MainHUDLeftMargin = 80.f;

	/** Gap between the top of the viewport and the composite HUD, in design units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD", meta = (ClampMin = "0.0"))
	float MainHUDTopMargin = 24.f;

	/** Keep the old fixed-position HUD behavior for maps that have not moved to the 9:20 frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Layout")
	bool bUseLegacyHUDPlacement = false;

	/** 20:9 Loading image used for both directions of scene travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Loading")
	TSoftObjectPtr<UTexture2D> SceneLoadingTexture;

	/** Portrait authoring frame. 900 x 2000 is exactly 9:20 and is shared by PC and Android. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Layout", meta = (ClampMin = "1.0"))
	FVector2D HUDDesignSize = FVector2D(900.f, 2000.f);

	/** 9:20 camera frame size in UMG/Slate units after DPI and letterbox handling. */
	UPROPERTY(BlueprintReadOnly, Category = "Night|HUD|Layout")
	FVector2D HUDCameraFrameSize = FVector2D::ZeroVector;

	/** Top-left of the fitted 9:20 camera frame in full-viewport UMG/Slate units. */
	UPROPERTY(BlueprintReadOnly, Category = "Night|HUD|Layout")
	FVector2D HUDCameraFrameOffset = FVector2D::ZeroVector;

	/** Uniform scale from HUDDesignSize to HUDCameraFrameSize. */
	UPROPERTY(BlueprintReadOnly, Category = "Night|HUD|Layout")
	float HUDCameraFitScale = 1.f;

	/** Standalone result pages. They are created by C++ and do not need to be nested in the main HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result")
	TSubclassOf<UUserWidget> SuccessResultWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result")
	TSubclassOf<UUserWidget> FailureResultWidgetClass;

	/** C++ binds these buttons directly; no Blueprint OnClicked wiring is required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result")
	FName SuccessContinueButtonName = TEXT("button");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result")
	FName FailureContinueButtonName = TEXT("button");

	/** Used only when a result WBP has no button; C++ creates a transparent runtime click target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result")
	FVector2D RuntimeContinueButtonPosition = FVector2D(320.f, 1568.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result")
	FVector2D RuntimeContinueButtonSize = FVector2D(238.f, 233.f);

	/** Ingredient -> quantity widget in WBP_Success. Supports TextBlock, RichTextBlock and EditableTextBox. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result")
	TMap<EIngredientId, FName> SuccessIngredientTextWidgetNames;

	/** Ordered format used for each integer quantity. {0} is the final Night -> Day count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result")
	FString SuccessIngredientCountFormat = TEXT("{0}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result", meta = (ClampMin = "0"))
	int32 ResultWidgetZOrder = 100;

	UPROPERTY(BlueprintReadOnly, Category = "Night|HUD|Result")
	FNightResult CurrentNightResult;

	UPROPERTY(BlueprintReadOnly, Category = "Night|HUD|Result")
	bool bNightResultVisible = false;

	/** Fires after the matching result WBP has been made visible and populated. */
	UPROPERTY(BlueprintAssignable, Category = "Night|HUD|Result")
	FOnNightHUDResultReady OnNightResultReady;

	/** The Host listens to this; result-page buttons should call RequestContinueAfterNightResult. */
	UPROPERTY(BlueprintAssignable, Category = "Night|HUD|Result")
	FOnNightHUDResultContinueRequested OnNightResultContinueRequested;

	/** Displays the standalone success/failure WBP. False means its class could not be loaded/created. */
	UFUNCTION(BlueprintCallable, Category = "Night|HUD|Result")
	bool PresentNightResult(const FNightResult& Result);

	UFUNCTION(BlueprintCallable, Category = "Night|HUD|Result")
	void HideNightResult();

	/** Call this from the final frame of a Blueprint animation or from the result button. */
	UFUNCTION(BlueprintCallable, Category = "Night|HUD|Result")
	void RequestContinueAfterNightResult();

	UFUNCTION(BlueprintPure, Category = "Night|HUD")
	UUserWidget* GetMainHUDWidget() const { return MainHUDWidget; }

	/** Blueprint presentation hook: bind result text, play animation, focus the result button, etc. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Night|HUD|Result", meta = (DisplayName = "On Night Result Ready"))
	void BP_OnNightResultReady(const FNightResult& Result);

	/** Blueprint layout hook. Use Offset/Size on the inner Canvas/SizeBox authored at HUDDesignSize. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Night|HUD|Layout", meta = (DisplayName = "On Night HUD Camera Frame Changed"))
	void BP_OnHUDCameraFrameChanged(
		FVector2D CameraFrameOffset,
		FVector2D CameraFrameSize,
		float CameraFitScale);

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

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SuccessResultWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> FailureResultWidget;

	FVector2D MainHUDPlacement = FVector2D(-1.f, -1.f);
	FVector2D LastNotifiedCameraFrameSize = FVector2D(-1.f, -1.f);
	FVector2D LastNotifiedCameraFrameOffset = FVector2D(-1.f, -1.f);

	void EnsureMainHUD();
	void EnsureResultWidgets();
	void ConfigureResultWidget(UUserWidget* ResultWidget, FName ContinueButtonName);
	void ApplySuccessIngredientCounts(const FNightResult& Result);
	void SetResultInputMode(UUserWidget* ActiveResultWidget);
	void UpdateMainHUDPlacement();
	void PushSoulToHealthBar(float Soul);
	void SetHealthBarNumeric(FName PropertyName, double Value);
};
#pragma endregion K2 moonyfli
