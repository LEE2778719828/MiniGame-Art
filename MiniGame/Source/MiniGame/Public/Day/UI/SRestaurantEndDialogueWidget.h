#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "SRestaurantEndDialogueWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRestaurantStandaloneTipDismissed);

UENUM(BlueprintType)
enum class ESRestaurantDialogueSpeaker : uint8
{
	System UMETA(DisplayName = "系统"),
	Customer UMETA(DisplayName = "牛马哥"),
	XiaoYao UMETA(DisplayName = "小妖"),
	KYisi UMETA(DisplayName = "K易斯"),
	ShanDaWang UMETA(DisplayName = "山大王")
};

UENUM(BlueprintType)
enum class ESRestaurantDialoguePresentation : uint8
{
	Reward UMETA(DisplayName = "结算奖励"),
	Dialogue UMETA(DisplayName = "普通对话"),
	Transition UMETA(DisplayName = "转场字幕"),
	RouteHintSmall UMETA(DisplayName = "岔路小图"),
	RouteHintLarge UMETA(DisplayName = "岔路大图")
};

/** One authored line in the first restaurant -> Night Course bridge. */
USTRUCT(BlueprintType)
struct MINIGAME_API FSRestaurantDialogueLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Restaurant Dialogue")
	int32 SceneNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Restaurant Dialogue")
	ESRestaurantDialogueSpeaker Speaker = ESRestaurantDialogueSpeaker::System;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Restaurant Dialogue")
	ESRestaurantDialoguePresentation Presentation = ESRestaurantDialoguePresentation::Dialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Restaurant Dialogue")
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Restaurant Dialogue", meta = (MultiLine = "true"))
	FText Text;
};

/**
 * Blueprint-facing, mobile-safe dialogue controller.
 * C++ owns ordering, double-tap protection, reward hand-off and stage continuation.
 * Blueprint owns textures, text placement, animations and sounds.
 */
UCLASS(Abstract, Blueprintable)
class MINIGAME_API USRestaurantEndDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USRestaurantEndDialogueWidget(const FObjectInitializer& ObjectInitializer);

	/** Night / other systems can reuse one KYisi line without advancing the Day restaurant flow. */
	UPROPERTY(BlueprintAssignable, Category = "Restaurant Dialogue")
	FOnRestaurantStandaloneTipDismissed OnStandaloneTipDismissed;

	/** Reset to scene 0 and present the first line. NativeConstruct calls this automatically. */
	UFUNCTION(BlueprintCallable, Category = "Restaurant Dialogue")
	void StartDialogue();

	/** Show a single KYisi-style line. Click/Advance dismisses without completing the Day dialogue. */
	UFUNCTION(BlueprintCallable, Category = "Restaurant Dialogue")
	void PresentStandaloneTip(
		ESRestaurantDialogueSpeaker Speaker,
		const FText& InSpeakerName,
		const FText& InText);

	/** Mobile Continue button entry point. Advances one line; the last press completes the day flow. */
	UFUNCTION(BlueprintCallable, Category = "Restaurant Dialogue")
	void AdvanceDialogue();

	/** Finish immediately. Useful for a future Skip button and safe against repeated taps. */
	UFUNCTION(BlueprintCallable, Category = "Restaurant Dialogue")
	bool FinishDialogue();

	UFUNCTION(BlueprintPure, Category = "Restaurant Dialogue")
	FSRestaurantDialogueLine GetCurrentLine() const;

	UFUNCTION(BlueprintPure, Category = "Restaurant Dialogue")
	int32 GetCurrentLineIndex() const { return CurrentLineIndex; }

	UFUNCTION(BlueprintPure, Category = "Restaurant Dialogue")
	int32 GetDialogueLineCount() const { return DialogueLines.Num(); }

	UFUNCTION(BlueprintPure, Category = "Restaurant Dialogue")
	bool IsDialogueFinished() const { return bFinished; }

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	/** Editable in WBP_RestaurantEndDialogue defaults; C++ supplies the 15-line reference script. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Restaurant Dialogue", meta = (TitleProperty = "Text"))
	TArray<FSRestaurantDialogueLine> DialogueLines;

	/** Update the Blueprint art from Line. Index is zero based; SceneNumber follows the design sheet. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Restaurant Dialogue", meta = (DisplayName = "Dialogue Line Changed"))
	void BP_OnDialogueLineChanged(const FSRestaurantDialogueLine& Line, int32 Index, int32 LineCount);

	UFUNCTION(BlueprintImplementableEvent, Category = "Restaurant Dialogue", meta = (DisplayName = "Dialogue Finished"))
	void BP_OnDialogueFinished();

private:
	void BuildDefaultDialogueLines();
	void PresentCurrentLine();

	UPROPERTY(Transient)
	int32 CurrentLineIndex = INDEX_NONE;

	bool bFinished = false;
	bool bStandaloneTipMode = false;
};
