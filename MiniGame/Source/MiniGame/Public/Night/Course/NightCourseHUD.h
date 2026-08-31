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
class UCurveFloat;
class USRestaurantEndDialogueWidget;
class UNightCourseTipWidget;

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

#pragma region K2 moonyfli
	/** WBP_Success / WBP_Failed widget that shows peak slash combo. Art still has a placeholder 60. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Result")
	FName ResultComboTextWidgetName = TEXT("EditableTextBox_1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "连击最小缩放", ToolTip = "连击为 1 时 WBP_Combo 的 RenderScale。", ClampMin = "0.1"))
	float ComboScaleMin = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "连击最大缩放", ToolTip = "连击涨满后的封顶 RenderScale。", ClampMin = "0.1"))
	float ComboScaleMax = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "缩放到顶连击数", ToolTip = "连击达到该值时缩放到 ComboScaleMax。", ClampMin = "1"))
	int32 ComboScaleFullAt = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "缩放曲线指数", ToolTip = "大于 1 时后期涨得更快；有独立缩放曲线时忽略此项。", ClampMin = "0.01"))
	float ComboScaleExponent = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "缩放曲线", ToolTip = "可选。横轴 0-1 对应连击 1 到缩放到顶连击数，纵轴是 0-1 插值。"))
	TObjectPtr<UCurveFloat> ComboScaleCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "连击弹出额外缩放", ToolTip = "每次连击增加时额外放大，随后在弹出时间内回落。", ClampMin = "0.0"))
	float ComboPopExtraScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "连击弹出时间", ToolTip = "弹出额外缩放回落到 0 所需秒数。", ClampMin = "0.0"))
	float ComboPopSeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "数字描边像素", ToolTip = "Txt_ComboCount 的 Slate 描边宽度。", ClampMin = "0"))
	int32 ComboOutlineSize = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "低连击描边颜色", ToolTip = "连击刚开始时的数字描边颜色。偏冷白，不要红/黄。"))
	FLinearColor ComboOutlineColor = FLinearColor(0.16f, 0.17f, 0.20f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "高连击描边颜色", ToolTip = "连击涨满后的数字描边颜色。略亮一点的冷白。"))
	FLinearColor ComboOutlineColorAtMax = FLinearColor(0.78f, 0.81f, 0.88f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "低连击数字颜色", ToolTip = "连击刚开始时的数字本体颜色。偏白。"))
	FLinearColor ComboTextColor = FLinearColor(0.93f, 0.94f, 0.97f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "高连击数字颜色", ToolTip = "连击涨满后的数字本体颜色。纯白。"))
	FLinearColor ComboTextColorAtMax = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Combo", meta = (DisplayName = "颜色涨满连击数", ToolTip = "描边和数字颜色插值到高连击色所需的连击数。", ClampMin = "1"))
	int32 ComboColorFullAt = 30;
#pragma endregion K2 moonyfli

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

	UFUNCTION(BlueprintPure, Category = "Night|HUD|Tips")
	bool IsCourseTipVisible() const { return bCourseTipVisible; }

	/** Show one KYisi tip. Returns false when no widget/text could be presented. */
	UFUNCTION(BlueprintCallable, Category = "Night|HUD|Tips")
	bool ShowCourseTip(const FText& SpeakerName, const FText& Body, const FText& ContinueHint);

	UFUNCTION(BlueprintCallable, Category = "Night|HUD|Tips")
	void DismissCourseTip();

	/** Called by gameplay input so the dismiss click is not also a jump/slash. */
	bool DismissCourseTipIfVisible();

	/** Start screen first, then course tip. True means this input must not jump/slash/choose a fork. */
	bool ConsumeBlockingOverlayInput();

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

	/** Prefers Day's KYisi WBP so Night reuses the same tip art. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Tips", meta = (AllowPrivateAccess = "true", DisplayName = "K易斯Tips控件"))
	TSubclassOf<USRestaurantEndDialogueWidget> CourseTipDialogueWidgetClass;

	/** Night-authored KYisi overlay. Preferred over the Day restaurant WBP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Tips", meta = (AllowPrivateAccess = "true", DisplayName = "Night Tips控件"))
	TSubclassOf<UUserWidget> CourseTipOverlayWidgetClass;

	/** Used only when the Day dialogue WBP is missing from this cook. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Tips", meta = (AllowPrivateAccess = "true", DisplayName = "Tips回退控件"))
	TSubclassOf<UNightCourseTipWidget> CourseTipFallbackWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Tips", meta = (AllowPrivateAccess = "true", DisplayName = "K易斯头像"))
	TSoftObjectPtr<UTexture2D> CourseTipPortrait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD|Tips", meta = (AllowPrivateAccess = "true", DisplayName = "Tips层级"))
	int32 CourseTipZOrder = 220;

	UPROPERTY(Transient)
	TObjectPtr<USRestaurantEndDialogueWidget> CourseTipDialogueWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CourseTipOverlayWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNightCourseTipWidget> CourseTipFallbackWidget;

	UPROPERTY(Transient)
	bool bCourseTipVisible = false;

	UPROPERTY(Transient)
	FText ActiveCourseTipSpeaker;

	UPROPERTY(Transient)
	FText ActiveCourseTipBody;

	UPROPERTY(Transient)
	FText ActiveCourseTipContinue;

	UPROPERTY(Transient)
	bool bDrawCourseTipOnCanvas = false;

	/** Same-frame latch: dismissing a tip must not also jump, slash, or pick a fork. */
	uint64 GameplayInputSuppressedFrame = 0; // GFrameCounter when a tip was just dismissed

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

	int32 LastDisplayedCombo = 0;
	float ComboPopElapsed = 0.f;
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
	void ApplyResultMaxCombo(UUserWidget* ResultWidget, int32 MaxCombo); //add by K2
	void SetResultInputMode(UUserWidget* ActiveResultWidget);
	void UpdateMainHUDPlacement();
	void PushSoulToHealthBar(float Soul);
	void SetHealthBarNumeric(FName PropertyName, double Value);
	void PushComboToHUD(int32 Combo); //add by K2
	float EvaluateComboProgress(int32 Combo, int32 FullAt, float Exponent) const;
	void ApplyComboNumberStyle(int32 Combo, float DeltaSeconds);

#pragma region K2 moonyfli
	UFUNCTION()
	void HandleCourseTipDismissed();

	void ApplyCourseTipWidgetTexts(
		UUserWidget* Widget,
		const FText& SpeakerName,
		const FText& Body,
		const FText& ContinueHint) const;
	void BindCourseTipDismissButton(UUserWidget* Widget);
	void NotifyDirectorCourseTipDismissed() const;
	bool HasVisibleCourseTipWidget() const;
	void SyncCourseTipWidgetLifetime();

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
