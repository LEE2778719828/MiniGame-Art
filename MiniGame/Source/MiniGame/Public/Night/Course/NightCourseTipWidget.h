#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "NightCourseTipWidget.generated.h"

class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNightCourseTipDismissed);

/**
 * Fallback KYisi tip overlay when WBP_RestaurantEndDialogue is not cooked into this map.
 * Click / touch dismisses; Night HUD swallows that input so it cannot jump or slash.
 */
UCLASS()
class MINIGAME_API UNightCourseTipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UNightCourseTipWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable, Category = "Night|Tips")
	FOnNightCourseTipDismissed OnTipDismissed;

	UFUNCTION(BlueprintCallable, Category = "Night|Tips")
	void PresentTip(
		const FText& InSpeakerName,
		const FText& InBody,
		const FText& InContinueHint,
		UTexture2D* Portrait);

	UFUNCTION(BlueprintCallable, Category = "Night|Tips")
	void DismissTip();

protected:
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	void EnsureFallbackTree();
	void RefreshBoundWidgets();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Portrait = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Speaker = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Body = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Continue = nullptr;

	FText SpeakerName;
	FText BodyText;
	FText ContinueHint;
	TObjectPtr<UTexture2D> PortraitTexture = nullptr;
	bool bDismissed = false;
};
