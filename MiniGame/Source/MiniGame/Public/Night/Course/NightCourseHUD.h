#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightCourseHUD.generated.h"

class UUserWidget;
class UTextBlock;
class UWidget;
class UDataTable; //add by K2
class UTexture2D; //add by K2
class UNightCourseDirector; //add by K2
class USoundBase; //add by K2

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnNightHUDResultReady,
	const FNightResult&,
	Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNightHUDResultContinueRequested);

#pragma region K2 moonyfli
/** One food icon in flight from a killed foe to the bag mouth. */
USTRUCT()
struct FNightDropFlyIcon
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Kill point. The screen start is sampled from it on the first drawn frame. */
	FVector WorldStart = FVector::ZeroVector;

	FVector2D CanvasStart = FVector2D::ZeroVector;

	float Elapsed = 0.f;

	float Delay = 0.f;

	bool bCanvasStartValid = false;
};

/** Per-foe hit layers: a vocal bark and/or a material crunch. Either slot may be empty. */
USTRUCT(BlueprintType)
struct FNightFoeHitSfx
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX")
	TSoftObjectPtr<USoundBase> Voice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX")
	TSoftObjectPtr<USoundBase> Material;
};
#pragma endregion K2 moonyfli

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
	 * add by K2 (R1): maps a screen point onto the Jump / Attack touch zones.
	 * Zones are authored in viewport fractions so they stay aligned after letterbox.
	 */
	bool HitTestActionButtons(
		float ScreenX,
		float ScreenY,
		float ViewX,
		float ViewY,
		ENightFeelInput& OutInput) const;

#pragma region K2 moonyfli
	/** Left-bottom Jump zone: X, Y, Width, Height as 0–1 of the game view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Touch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector4 JumpTouchNorm = FVector4(0.f, 0.5f, 0.5f, 0.5f);

	/** Right-bottom Attack zone: X, Y, Width, Height as 0–1 of the game view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Touch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector4 AttackTouchNorm = FVector4(0.5f, 0.5f, 0.5f, 0.5f);

	/** Phase / NOW / route text. Off for shipping; leave on only while tuning feel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Debug")
	bool bShowDebugHud = false;

	/** Translucent Jump / Attack rectangles so the authored touch zones can be lined up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Debug")
	bool bShowTouchZones = false;
#pragma endregion K2 moonyfli

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

	/** Whether the optional parkour start-screen overlay is currently shown. */
	UFUNCTION(BlueprintPure, Category = "Night|HUD|Start Screen")
	bool IsStartScreenVisible() const;

	/** Hide the parkour start-screen overlay. Safe to call from Blueprint or C++. */
	UFUNCTION(BlueprintCallable, Category = "Night|HUD|Start Screen")
	void DismissStartScreen();

	/** Called by gameplay input so the first key/touch dismisses the overlay. */
	bool DismissStartScreenIfVisible();

	/** Blueprint presentation hook: bind result text, play animation, focus the result button, etc. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Night|HUD|Result", meta = (DisplayName = "On Night Result Ready"))
	void BP_OnNightResultReady(const FNightResult& Result);

#pragma region K2 moonyfli
	/** Master switch for the food-into-bag flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop")
	bool bEnableDropFlyIcons = true;

	/** DT_Ingredients: the Icon column supplies the flown texture per ingredient. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop")
	TSoftObjectPtr<UDataTable> IngredientIconTable;

	/** Nested bag widget inside WBP_NightHUD_Multi. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop")
	FName BagPackWidgetName = TEXT("WBP_BagPack");

	/** Empty box inside WBP_BagPack marking the fish mouth; the flight ends at its center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop")
	FName BagFlyTargetWidgetName = TEXT("SB_FlyTarget");

	/** Used only when the bag widget cannot be resolved. Design units inside HUDDesignSize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop")
	FVector2D BagFlyTargetFallback = FVector2D(210.f, 150.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop", meta = (ClampMin = "0.05"))
	float DropFlySeconds = 0.7f;

	/** Extra delay per icon when one kill awards several units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop", meta = (ClampMin = "0.0"))
	float DropFlyStaggerSeconds = 0.09f;

	/** Icon edge length in design units at the start of the flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop", meta = (ClampMin = "1.0"))
	float DropFlyIconSize = 200.f;

	/** Icon shrinks to this fraction as it reaches the mouth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float DropFlyEndScale = 0.35f;

	/** Sideways bulge of the arc in design units. Positive bows the path away from the straight line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop")
	float DropFlyArcHeight = 300.f;

	/** Lifts the kill point so the icon starts around the foe's chest instead of its pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop")
	float DropFlyWorldZOffset = 60.f;

	/** Hard cap so a long combo cannot flood the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Drop", meta = (ClampMin = "1"))
	int32 MaxDropFlyIcons = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX")
	bool bEnableNightSfx = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX")
	TSoftObjectPtr<USoundBase> SlashSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX")
	TSoftObjectPtr<USoundBase> IngredientDropSound;

	/**
	 * Default mapping follows current DA_Course art:
	 * M01 fish, M02 bat, M03 aquatic, M04 aquatic fallback, M05 rice/cantingguai.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX")
	TMap<EFoeId, FNightFoeHitSfx> FoeHitSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX", meta = (ClampMin = "0.0"))
	float SlashVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX", meta = (ClampMin = "0.0"))
	float FoeHitVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX", meta = (ClampMin = "0.0"))
	float IngredientDropVolume = 1.f;

	/** Seconds of the drop wave to keep before fade. 0 plays the full 1.87s file. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX", meta = (ClampMin = "0.0"))
	float IngredientDropPlaySeconds = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|SFX", meta = (ClampMin = "0.01"))
	float IngredientDropFadeSeconds = 0.12f;

	UFUNCTION(BlueprintCallable, Category = "Night|HUD|SFX")
	void NotifyFoeKilled(EFoeId FoeId, bool bPlayDrop);
#pragma endregion K2 moonyfli

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

	/** 20:9 opening artwork shown over the parkour HUD until the first input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Start Screen", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> StartScreenTexture;

	/** Show the opening artwork when this HUD begins play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Start Screen", meta = (AllowPrivateAccess = "true"))
	bool bShowStartScreenOnBeginPlay = true;

	/** Hide the opening artwork after the first key/touch. */
	UPROPERTY(Transient)
	bool bStartScreenDismissed = false;

	/** Authored size of the bar inside the WBP: SetHealth pins the fill to x = FullBarWidth. */
	UPROPERTY(EditAnywhere, Category = "Night|HUD")
	FVector2D HealthBarDesignSize = FVector2D(800.f, 240.f);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MainHUDWidget;

	/** Nested WBP_HealthBar instance owned by MainHUDWidget. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HealthBarWidget;

#pragma region K2 moonyfli
	/** Nested WBP_Combo instance owned by MainHUDWidget. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ComboWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ComboCountText;

	/** Nested WBP_BagPack instance owned by MainHUDWidget. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BagPackWidget;

	UPROPERTY(Transient)
	TArray<FNightDropFlyIcon> DropFlyIcons;

	UPROPERTY(Transient)
	TMap<EIngredientId, TObjectPtr<UTexture2D>> ResolvedIngredientIcons;

	TWeakObjectPtr<UNightCourseDirector> BoundDropDirector;
#pragma endregion K2 moonyfli

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
	void PushComboToHUD(int32 Combo); //add by K2

#pragma region K2 moonyfli
	UFUNCTION()
	void HandleIngredientDropped(EIngredientId DropId, int32 Count, FVector WorldLocation);

	void EnsureDropDirectorBinding(UNightCourseDirector* Director);
	UTexture2D* ResolveIngredientIcon(EIngredientId DropId);
	/** Canvas-space pixel offset of the game view inside the window letterbox. */
	FVector2D GetCanvasLetterboxPixelOffset() const;
	bool GetBagFlyTargetCanvasPosition(FVector2D& OutCanvasPosition) const;
	void DrawDropFlyIcons(float DeltaSeconds);
	void PlaySfx(
		const TSoftObjectPtr<USoundBase>& SoftSound,
		float Volume,
		float StopAfterSeconds = 0.f,
		float FadeOutSeconds = 0.08f);
#pragma endregion K2 moonyfli
};
#pragma endregion K2 moonyfli
