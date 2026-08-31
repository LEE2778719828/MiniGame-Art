#include "Night/Course/NightCourseHUD.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTipWidget.h"
#include "Night/Course/NightCourseTypes.h"
#include "EngineUtils.h"
#include "Day/UI/SRestaurantEndDialogueWidget.h"
#include "../../../SStandaloneSandbox.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h" //add by K2
#include "Curves/CurveFloat.h"
#include "Fonts/SlateFontInfo.h"
#include "Components/Widget.h"
#include "Blueprint/SlateBlueprintLibrary.h" //add by K2
#include "CanvasItem.h" //add by K2
#include "Engine/Canvas.h"
#include "Engine/DataTable.h" //add by K2
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CoreGlobals.h"
#include "Engine/Texture2D.h" //add by K2
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h" //add by K2
#include "Kismet/GameplayStatics.h" //add by K2
#include "Sound/SoundBase.h" //add by K2
#include "TimerManager.h" //add by K2
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#pragma region K2 moonyfli
namespace NightHudPads
{
	bool Contains(
		float ScreenX,
		float ScreenY,
		float ViewX,
		float ViewY,
		const FVector4& Norm)
	{
		if (ViewX <= KINDA_SMALL_NUMBER || ViewY <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float X = Norm.X * ViewX;
		const float Y = Norm.Y * ViewY;
		const float W = FMath::Max(0.f, Norm.Z) * ViewX;
		const float H = FMath::Max(0.f, Norm.W) * ViewY;
		return ScreenX >= X && ScreenX <= X + W && ScreenY >= Y && ScreenY <= Y + H;
	}
}

bool ANightCourseHUD::HitTestActionButtons(
	float ScreenX,
	float ScreenY,
	float ViewX,
	float ViewY,
	ENightFeelInput& OutInput) const
{
	using namespace NightHudPads;
	if (Contains(ScreenX, ScreenY, ViewX, ViewY, JumpTouchNorm))
	{
		OutInput = ENightFeelInput::Jump;
		return true;
	}
	if (Contains(ScreenX, ScreenY, ViewX, ViewY, AttackTouchNorm))
	{
		OutInput = ENightFeelInput::Attack;
		return true;
	}
	return false;
}

ANightCourseHUD::ANightCourseHUD()
{
	SuccessIngredientTextWidgetNames.Add(EIngredientId::F01_LingGu, TEXT("1"));
	SuccessIngredientTextWidgetNames.Add(EIngredientId::F02_YinShanJun, TEXT("2"));
	SuccessIngredientTextWidgetNames.Add(EIngredientId::F03_ChiYanJiao, TEXT("3"));
	SuccessIngredientTextWidgetNames.Add(EIngredientId::F04_YueLinYu, TEXT("4"));
	SuccessIngredientTextWidgetNames.Add(EIngredientId::F05_XuanYuQin, TEXT("5"));
	IngredientIconTable = TSoftObjectPtr<UDataTable>(
		FSoftObjectPath(TEXT("/Game/Shared/Data/DT_Ingredients.DT_Ingredients"))); //add by K2
	StartScreenTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/Night/Course/UI/T_NightCourseStartScreen.T_NightCourseStartScreen")));
	CourseTipPortrait = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/Day/UI/Dialogue/Textures/T_Dialogue_KYisi.T_Dialogue_KYisi")));

#pragma region K2 moonyfli
	auto MakeSfx = [](const TCHAR* Path)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(Path));
	};
	SlashSound = MakeSfx(TEXT("/Game/Night/Course/Audio/SW_Slash.SW_Slash"));
	IngredientDropSound = MakeSfx(
		TEXT("/Game/Night/Course/Audio/SW_IngredientDrop.SW_IngredientDrop"));

	FNightFoeHitSfx FishHit;
	FishHit.Voice = MakeSfx(TEXT("/Game/Night/Course/Audio/SW_Hit_Fish_Voice.SW_Hit_Fish_Voice"));
	FishHit.Material = MakeSfx(
		TEXT("/Game/Night/Course/Audio/SW_Hit_Fish_Material.SW_Hit_Fish_Material"));
	FoeHitSounds.Add(EFoeId::M01, FishHit);

	FNightFoeHitSfx BatHit;
	BatHit.Material = MakeSfx(
		TEXT("/Game/Night/Course/Audio/SW_Hit_Bat_Material.SW_Hit_Bat_Material"));
	FoeHitSounds.Add(EFoeId::M02, BatHit);

	FNightFoeHitSfx AquaticHit;
	AquaticHit.Voice = MakeSfx(
		TEXT("/Game/Night/Course/Audio/SW_Hit_Aquatic_Voice.SW_Hit_Aquatic_Voice"));
	FoeHitSounds.Add(EFoeId::M03, AquaticHit);
	FoeHitSounds.Add(EFoeId::M04, AquaticHit);

	FNightFoeHitSfx RiceHit;
	RiceHit.Voice = MakeSfx(TEXT("/Game/Night/Course/Audio/SW_Hit_Rice_Voice.SW_Hit_Rice_Voice"));
	RiceHit.Material = MakeSfx(
		TEXT("/Game/Night/Course/Audio/SW_Hit_Rice_Material.SW_Hit_Rice_Material"));
	FoeHitSounds.Add(EFoeId::M05, RiceHit);
#pragma endregion K2 moonyfli
}
void ANightCourseHUD::BeginPlay()
{
	Super::BeginPlay();

	// Never let stale Blueprint defaults expose development overlays in a packaged build.
	bShowDebugHud = false;
	bShowTouchZones = false;

	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		if (!SceneLoadingTexture.IsNull())
		{
			GameInstance->RegisterSceneLoadingTexture(SceneLoadingTexture);
		}
	}
	if (bShowStartScreenOnBeginPlay && !StartScreenTexture.IsNull())
	{
		// Prime the opening art before the first frame. The asset is a soft reference so a
		// missing unsaved import simply leaves the normal HUD path intact.
		StartScreenTexture.LoadSynchronous();
	}
	EnsureMainHUD();
}


bool ANightCourseHUD::IsStartScreenVisible() const
{
	return bShowStartScreenOnBeginPlay && !bStartScreenDismissed && StartScreenTexture.Get() != nullptr;
}

void ANightCourseHUD::DismissStartScreen()
{
	if (!IsStartScreenVisible())
	{
		return;
	}

	bStartScreenDismissed = true;
	if (MainHUDWidget && !bNightResultVisible)
	{
		MainHUDWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

bool ANightCourseHUD::DismissStartScreenIfVisible()
{
	if (!IsStartScreenVisible())
	{
		return false;
	}

	DismissStartScreen();
	return true;
}

bool ANightCourseHUD::ShowCourseTip(
	const FText& SpeakerName,
	const FText& Body,
	const FText& ContinueHint)
{
	if (Body.IsEmpty())
	{
		return false;
	}

	DismissCourseTip();

	ActiveCourseTipSpeaker = SpeakerName;
	ActiveCourseTipBody = Body;
	ActiveCourseTipContinue = ContinueHint;
	bCourseTipVisible = true;
	bDrawCourseTipOnCanvas = true;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		if (const UWorld* World = GetWorld())
		{
			PC = World->GetFirstPlayerController();
		}
	}

	if (PC && !CourseTipOverlayWidgetClass)
	{
		CourseTipOverlayWidgetClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Night/Course/Blueprints/WBP_NightCourseTip.WBP_NightCourseTip_C"));
	}
	if (PC && CourseTipOverlayWidgetClass)
	{
		CourseTipOverlayWidget = CreateWidget<UUserWidget>(PC, CourseTipOverlayWidgetClass);
		if (CourseTipOverlayWidget)
		{
			CourseTipOverlayWidget->SetVisibility(ESlateVisibility::Visible);
			ApplyCourseTipWidgetTexts(CourseTipOverlayWidget, SpeakerName, Body, ContinueHint);
			BindCourseTipDismissButton(CourseTipOverlayWidget);
			if (!Cast<UTextBlock>(CourseTipOverlayWidget->GetWidgetFromName(TEXT("TXT_Body"))))
			{
				CourseTipOverlayWidget->RemoveFromParent();
				CourseTipOverlayWidget = nullptr;
			}
			else
			{
				CourseTipOverlayWidget->AddToViewport(CourseTipZOrder);
			}
		}
	}

	if (PC && CourseTipDialogueWidgetClass)
	{
		CourseTipDialogueWidget = CreateWidget<USRestaurantEndDialogueWidget>(PC, CourseTipDialogueWidgetClass);
		if (CourseTipDialogueWidget)
		{
			CourseTipDialogueWidget->OnStandaloneTipDismissed.AddDynamic(
				this,
				&ANightCourseHUD::HandleCourseTipDismissed);
			CourseTipDialogueWidget->PresentStandaloneTip(
				ESRestaurantDialogueSpeaker::KYisi,
				SpeakerName,
				Body);
			ApplyCourseTipWidgetTexts(CourseTipDialogueWidget, SpeakerName, Body, ContinueHint);
			BindCourseTipDismissButton(CourseTipDialogueWidget);
			CourseTipDialogueWidget->AddToViewport(CourseTipZOrder);
		}
	}

	if (PC && !CourseTipOverlayWidget && !CourseTipDialogueWidget)
	{
		if (!CourseTipFallbackWidgetClass)
		{
			CourseTipFallbackWidgetClass = UNightCourseTipWidget::StaticClass();
		}
		CourseTipFallbackWidget = CreateWidget<UNightCourseTipWidget>(PC, CourseTipFallbackWidgetClass);
		if (CourseTipFallbackWidget)
		{
			UTexture2D* Portrait = nullptr;
			if (!CourseTipPortrait.IsNull())
			{
				Portrait = CourseTipPortrait.LoadSynchronous();
			}
			CourseTipFallbackWidget->OnTipDismissed.AddDynamic(
				this,
				&ANightCourseHUD::HandleCourseTipDismissed);
			CourseTipFallbackWidget->PresentTip(SpeakerName, Body, ContinueHint, Portrait);
			CourseTipFallbackWidget->AddToViewport(CourseTipZOrder);
		}
	}

	const bool bHasWidget = HasVisibleCourseTipWidget();
	bDrawCourseTipOnCanvas = !bHasWidget;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightHUD][Tips] visible=%d widget=%s canvas=%d text='%s'."),
		bCourseTipVisible ? 1 : 0,
		bHasWidget ? TEXT("yes") : TEXT("no"),
		bDrawCourseTipOnCanvas ? 1 : 0,
		*Body.ToString());
	return bHasWidget || bDrawCourseTipOnCanvas;
}

void ANightCourseHUD::DismissCourseTip()
{
	const bool bWasVisible = bCourseTipVisible;
	bCourseTipVisible = false;

	if (CourseTipDialogueWidget)
	{
		CourseTipDialogueWidget->OnStandaloneTipDismissed.RemoveDynamic(
			this,
			&ANightCourseHUD::HandleCourseTipDismissed);
		CourseTipDialogueWidget->RemoveFromParent();
		CourseTipDialogueWidget = nullptr;
	}
	if (CourseTipOverlayWidget)
	{
		if (UButton* DismissButton = Cast<UButton>(CourseTipOverlayWidget->GetWidgetFromName(TEXT("BTN_Dismiss"))))
		{
			DismissButton->OnClicked.RemoveDynamic(this, &ANightCourseHUD::HandleCourseTipDismissed);
		}
		CourseTipOverlayWidget->RemoveFromParent();
		CourseTipOverlayWidget = nullptr;
	}
	if (CourseTipFallbackWidget)
	{
		CourseTipFallbackWidget->OnTipDismissed.RemoveDynamic(
			this,
			&ANightCourseHUD::HandleCourseTipDismissed);
		CourseTipFallbackWidget->RemoveFromParent();
		CourseTipFallbackWidget = nullptr;
	}

	ActiveCourseTipSpeaker = FText::GetEmpty();
	ActiveCourseTipBody = FText::GetEmpty();
	ActiveCourseTipContinue = FText::GetEmpty();
	bDrawCourseTipOnCanvas = false;

	if (bWasVisible)
	{
		GameplayInputSuppressedFrame = GFrameCounter;
		NotifyDirectorCourseTipDismissed();
	}
}

bool ANightCourseHUD::DismissCourseTipIfVisible()
{
	if (!IsCourseTipVisible())
	{
		return false;
	}
	DismissCourseTip();
	return true;
}

bool ANightCourseHUD::ConsumeBlockingOverlayInput()
{
	if (DismissStartScreenIfVisible())
	{
		return true;
	}
	if (DismissCourseTipIfVisible())
	{
		return true;
	}
	return GameplayInputSuppressedFrame == GFrameCounter;
}

void ANightCourseHUD::HandleCourseTipDismissed()
{
	DismissCourseTip();
}

void ANightCourseHUD::ApplyCourseTipWidgetTexts(
	UUserWidget* Widget,
	const FText& SpeakerName,
	const FText& Body,
	const FText& ContinueHint) const
{
	if (!Widget)
	{
		return;
	}

	auto SetNamedText = [Widget](const TCHAR* WidgetName, const FText& Text)
	{
		if (UTextBlock* Block = Cast<UTextBlock>(Widget->GetWidgetFromName(WidgetName)))
		{
			Block->SetText(Text);
		}
	};
	SetNamedText(TEXT("TXT_Speaker"), SpeakerName);
	SetNamedText(TEXT("TXT_Body"), Body);
	SetNamedText(TEXT("TXT_Dialogue"), Body);
	SetNamedText(TEXT("TXT_Continue"), ContinueHint);

	if (FTextProperty* BodyProp = FindFProperty<FTextProperty>(Widget->GetClass(), TEXT("TipBodyText")))
	{
		BodyProp->SetPropertyValue_InContainer(Widget, Body);
	}
}

void ANightCourseHUD::BindCourseTipDismissButton(UUserWidget* Widget)
{
	if (!Widget)
	{
		return;
	}
	if (UButton* DismissButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("BTN_Dismiss"))))
	{
		DismissButton->OnClicked.RemoveDynamic(this, &ANightCourseHUD::HandleCourseTipDismissed);
		DismissButton->OnClicked.AddDynamic(this, &ANightCourseHUD::HandleCourseTipDismissed);
		DismissButton->SetVisibility(ESlateVisibility::Visible);
	}
}

void ANightCourseHUD::NotifyDirectorCourseTipDismissed() const
{
	if (const ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(GetOwningPawn()))
	{
		if (UNightCourseDirector* Director = CoursePawn->GetCourseDirector())
		{
			Director->NotifyCourseTipDismissed();
			return;
		}
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ANightCourseHost> It(World); It; ++It)
		{
			if (*It && (*It)->Director)
			{
				(*It)->Director->NotifyCourseTipDismissed();
				return;
			}
		}
	}
}

bool ANightCourseHUD::HasVisibleCourseTipWidget() const
{
	auto IsUp = [](const UUserWidget* Widget)
	{
		return Widget && Widget->IsInViewport();
	};
	return IsUp(CourseTipOverlayWidget) || IsUp(CourseTipDialogueWidget) || IsUp(CourseTipFallbackWidget);
}

void ANightCourseHUD::SyncCourseTipWidgetLifetime()
{
	if (!bCourseTipVisible || bDrawCourseTipOnCanvas)
	{
		return;
	}
	if (!HasVisibleCourseTipWidget())
	{
		DismissCourseTip();
	}
}

void ANightCourseHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HealthBarWidget = nullptr;
	ComboWidget = nullptr;
	ComboCountText = nullptr;
#pragma region K2 moonyfli
	BagPackWidget = nullptr;
	DropFlyIcons.Reset();
	ResolvedIngredientIcons.Reset();
	FailSideFlashRemaining = 0.f;
	if (UNightCourseDirector* Director = BoundDropDirector.Get())
	{
		Director->OnIngredientDropped.RemoveDynamic(
			this,
			&ANightCourseHUD::HandleIngredientDropped);
	}
	BoundDropDirector = nullptr;
#pragma endregion K2 moonyfli
	if (SuccessResultWidget)
	{
		SuccessResultWidget->RemoveFromParent();
		SuccessResultWidget = nullptr;
	}
	if (FailureResultWidget)
	{
		FailureResultWidget->RemoveFromParent();
		FailureResultWidget = nullptr;
	}
	if (MainHUDWidget)
	{
		MainHUDWidget->RemoveFromParent();
		MainHUDWidget = nullptr;
	}
	DismissCourseTip();
	Super::EndPlay(EndPlayReason);
}
void ANightCourseHUD::EnsureMainHUD()
{
	// Resolve the widget only after the HUD CDO has finished loading. Resolving it in the
	// constructor can recursively load this HUD's CDO through the widget blueprint and deadlock
	// the function-local static FClassFinder guard.
	if (!MainHUDWidgetClass)
	{
		MainHUDWidgetClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Night/Course/Blueprints/WBP_NightHUD_Multi.WBP_NightHUD_Multi_C"));
	}
	if (MainHUDWidget || !MainHUDWidgetClass)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}
	MainHUDWidget = CreateWidget<UUserWidget>(PC, MainHUDWidgetClass);
	if (!MainHUDWidget)
	{
		return;
	}
	MainHUDWidget->AddToViewport(20);
	MainHUDPlacement = FVector2D(-1.f, -1.f);
	MainHUDWidget->SetRenderTransformPivot(FVector2D(0.f, 0.f));
	MainHUDWidget->SetRenderScale(
		bUseLegacyHUDPlacement
			? FVector2D(MainHUDScale, MainHUDScale)
			: FVector2D(1.f, 1.f));

	EnsureResultWidgets();
	HideNightResult();

	HealthBarWidget = Cast<UUserWidget>(
		MainHUDWidget->GetWidgetFromName(TEXT("WBP_HealthBar")));
	if (HealthBarWidget)
	{
		SetHealthBarNumeric(TEXT("FullBarWidth"), HealthBarDesignSize.X);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightHUD] WBP_NightHUD_Multi is missing its nested WBP_HealthBar widget; Soul falls back to Canvas text."));
	}

#pragma region K2 moonyfli
	ComboWidget = Cast<UUserWidget>(MainHUDWidget->GetWidgetFromName(TEXT("WBP_Combo")));
	if (ComboWidget)
	{
		ComboCountText = Cast<UTextBlock>(ComboWidget->GetWidgetFromName(TEXT("Txt_ComboCount")));
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[NightHUD] WBP_NightHUD_Multi is missing nested WBP_Combo; combo count will stay hidden."));
	}

	BagPackWidget = Cast<UUserWidget>(
		MainHUDWidget->GetWidgetFromName(BagPackWidgetName));
	if (!BagPackWidget)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[NightHUD] WBP_NightHUD_Multi is missing nested '%s'; drop icons fly to BagFlyTargetFallback."),
			*BagPackWidgetName.ToString());
	}
#pragma endregion K2 moonyfli
}

void ANightCourseHUD::EnsureResultWidgets()
{
	if (!SuccessResultWidgetClass)
	{
		SuccessResultWidgetClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Night/Course/Blueprints/WBP_Success.WBP_Success_C"));
	}
	if (!FailureResultWidgetClass)
	{
		FailureResultWidgetClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Night/Course/Blueprints/WBP_Failed.WBP_Failed_C"));
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	if (!SuccessResultWidget && SuccessResultWidgetClass)
	{
		SuccessResultWidget = CreateWidget<UUserWidget>(PC, SuccessResultWidgetClass);
		if (SuccessResultWidget)
		{
			SuccessResultWidget->AddToViewport(ResultWidgetZOrder);
			SuccessResultWidget->SetVisibility(ESlateVisibility::Collapsed);
			ConfigureResultWidget(SuccessResultWidget, SuccessContinueButtonName);
		}
	}
	if (!FailureResultWidget && FailureResultWidgetClass)
	{
		FailureResultWidget = CreateWidget<UUserWidget>(PC, FailureResultWidgetClass);
		if (FailureResultWidget)
		{
			FailureResultWidget->AddToViewport(ResultWidgetZOrder);
			FailureResultWidget->SetVisibility(ESlateVisibility::Collapsed);
			ConfigureResultWidget(FailureResultWidget, FailureContinueButtonName);
		}
	}
}

void ANightCourseHUD::ConfigureResultWidget(UUserWidget* ResultWidget, const FName ContinueButtonName)
{
	if (!ResultWidget)
	{
		return;
	}

	UButton* ContinueButton = Cast<UButton>(ResultWidget->GetWidgetFromName(ContinueButtonName));
	if (!ContinueButton && ResultWidget->WidgetTree)
	{
		if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(ResultWidget->GetRootWidget()))
		{
			ContinueButton = ResultWidget->WidgetTree->ConstructWidget<UButton>(
				UButton::StaticClass(),
				ContinueButtonName);
			if (UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(ContinueButton))
			{
				ButtonSlot->SetPosition(RuntimeContinueButtonPosition);
				ButtonSlot->SetSize(RuntimeContinueButtonSize);
				ButtonSlot->SetZOrder(100);
				ContinueButton->SetRenderOpacity(0.f);
				UE_LOG(
					LogTemp,
					Display,
					TEXT("[NightHUD] Created runtime continue button '%s' for '%s'."),
					*ContinueButtonName.ToString(),
					*ResultWidget->GetName());
			}
		}
	}

	if (!ContinueButton)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightHUD] Result widget '%s' is missing button '%s' and has no Canvas root for a runtime fallback."),
			*ResultWidget->GetName(),
			*ContinueButtonName.ToString());
		return;
	}

	ContinueButton->OnClicked.RemoveDynamic(this, &ANightCourseHUD::RequestContinueAfterNightResult);
	ContinueButton->OnClicked.AddDynamic(this, &ANightCourseHUD::RequestContinueAfterNightResult);
}
void ANightCourseHUD::ApplySuccessIngredientCounts(const FNightResult& Result)
{
	if (!SuccessResultWidget)
	{
		return;
	}

	TMap<EIngredientId, int32> Counts;
	for (const FIngredientStack& Stack : Result.Ingredients)
	{
		if (Stack.Id != EIngredientId::None)
		{
			Counts.FindOrAdd(Stack.Id) += FMath::Max(0, Stack.Count);
		}
	}

	for (const TPair<EIngredientId, FName>& Binding : SuccessIngredientTextWidgetNames)
	{
		UWidget* Target = SuccessResultWidget->GetWidgetFromName(Binding.Value);
		if (!Target)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightHUD] WBP_Success is missing ingredient text widget '%s' for Ingredient=%d."),
				*Binding.Value.ToString(),
				static_cast<int32>(Binding.Key));
			continue;
		}

		FFormatOrderedArguments Arguments;
		Arguments.Add(FText::AsNumber(Counts.FindRef(Binding.Key)));
		const FText CountText = FText::Format(
			FText::FromString(SuccessIngredientCountFormat),
			Arguments);

		if (UTextBlock* TextBlock = Cast<UTextBlock>(Target))
		{
			TextBlock->SetText(CountText);
		}
		else if (URichTextBlock* RichTextBlock = Cast<URichTextBlock>(Target))
		{
			RichTextBlock->SetText(CountText);
		}
		else if (UEditableTextBox* EditableTextBox = Cast<UEditableTextBox>(Target))
		{
			EditableTextBox->SetIsReadOnly(true);
			EditableTextBox->SetText(CountText);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightHUD] Ingredient widget '%s' has unsupported class '%s'."),
				*Binding.Value.ToString(),
				*Target->GetClass()->GetName());
		}
	}
}

#pragma region K2 moonyfli
void ANightCourseHUD::ApplyResultMaxCombo(UUserWidget* ResultWidget, const int32 MaxCombo)
{
	if (!ResultWidget || ResultComboTextWidgetName.IsNone())
	{
		return;
	}

	UWidget* Target = ResultWidget->GetWidgetFromName(ResultComboTextWidgetName);
	if (!Target)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[NightHUD] Result widget '%s' is missing combo text '%s'."),
			*ResultWidget->GetName(),
			*ResultComboTextWidgetName.ToString());
		return;
	}

	const FText ComboText = FText::AsNumber(FMath::Max(0, MaxCombo));
	if (UTextBlock* TextBlock = Cast<UTextBlock>(Target))
	{
		TextBlock->SetText(ComboText);
	}
	else if (URichTextBlock* RichTextBlock = Cast<URichTextBlock>(Target))
	{
		RichTextBlock->SetText(ComboText);
	}
	else if (UEditableTextBox* EditableTextBox = Cast<UEditableTextBox>(Target))
	{
		EditableTextBox->SetIsReadOnly(true);
		EditableTextBox->SetText(ComboText);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightHUD] Combo widget '%s' has unsupported class '%s'."),
			*ResultComboTextWidgetName.ToString(),
			*Target->GetClass()->GetName());
	}
}
#pragma endregion K2 moonyfli

void ANightCourseHUD::SetResultInputMode(UUserWidget* ActiveResultWidget)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	if (ActiveResultWidget)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ActiveResultWidget->TakeWidget());
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}
bool ANightCourseHUD::PresentNightResult(const FNightResult& Result)
{
	EnsureMainHUD();
	EnsureResultWidgets();
	CurrentNightResult = Result;

	const bool bSucceeded = Result.bSuccess && !Result.bFailedMidway;
	if (bSucceeded)
	{
		ApplySuccessIngredientCounts(Result);
	}
	ApplyResultMaxCombo(bSucceeded ? SuccessResultWidget.Get() : FailureResultWidget.Get(), Result.MaxCombo);
	UpdateMainHUDPlacement();

	if (SuccessResultWidget)
	{
		SuccessResultWidget->SetVisibility(
			bSucceeded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (FailureResultWidget)
	{
		FailureResultWidget->SetVisibility(
			bSucceeded ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	UUserWidget* ActiveResultWidget = bSucceeded
		? SuccessResultWidget.Get()
		: FailureResultWidget.Get();
	bNightResultVisible = ActiveResultWidget != nullptr;
	if (bNightResultVisible)
	{
		if (MainHUDWidget)
		{
			MainHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		SetResultInputMode(ActiveResultWidget);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightHUD] Standalone result widget class for '%s' could not be loaded or created; flow will not wait for presentation."),
			bSucceeded ? TEXT("Success") : TEXT("Failure"));
	}

	OnNightResultReady.Broadcast(Result);
	BP_OnNightResultReady(Result);
	return bNightResultVisible;
}
void ANightCourseHUD::HideNightResult()
{
	if (SuccessResultWidget)
	{
		SuccessResultWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (FailureResultWidget)
	{
		FailureResultWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MainHUDWidget)
	{
		if (UWidget* EmbeddedSuccess = MainHUDWidget->GetWidgetFromName(TEXT("WBP_Success")))
		{
			EmbeddedSuccess->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (UWidget* EmbeddedFailure = MainHUDWidget->GetWidgetFromName(TEXT("WBP_Failed")))
		{
			EmbeddedFailure->SetVisibility(ESlateVisibility::Collapsed);
		}
		MainHUDWidget->SetVisibility(
			IsStartScreenVisible()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
	SetResultInputMode(nullptr);
	bNightResultVisible = false;
}
void ANightCourseHUD::RequestContinueAfterNightResult()
{
	if (!bNightResultVisible)
	{
		return;
	}
	HideNightResult();
	OnNightResultContinueRequested.Broadcast();
}

void ANightCourseHUD::UpdateMainHUDPlacement()
{
	if (!MainHUDWidget)
	{
		return;
	}

	// UMG AddToViewport is the full window, including the black letterbox. Canvas is the
	// constrained game view that DrawHUD / Q-E pads already use. Offset by that gap so
	// Left / Top Margin are measured from the game view, not the window.
	const float Dpi = UWidgetLayoutLibrary::GetViewportScale(this);
	if (Dpi <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	int32 FullX = 0;
	int32 FullY = 0;
	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->GetViewportSize(FullX, FullY);
	}

	float GameW = static_cast<float>(FullX);
	float GameH = static_cast<float>(FullY);
	if (Canvas)
	{
		GameW = Canvas->SizeX;
		GameH = Canvas->SizeY;
	}

	if (!bUseLegacyHUDPlacement)
	{
		const FVector2D GameViewportSize(GameW / Dpi, GameH / Dpi);
		const FVector2D GameViewportOffset(
			FMath::Max(0.f, (static_cast<float>(FullX) - GameW) * 0.5f) / Dpi,
			FMath::Max(0.f, (static_cast<float>(FullY) - GameH) * 0.5f) / Dpi);

		const float SafeDesignX = FMath::Max(1.f, HUDDesignSize.X);
		const float SafeDesignY = FMath::Max(1.f, HUDDesignSize.Y);

		// WBP_NightHUD_Multi is authored as a fixed 900x2000 canvas. Fill the actual
		// game viewport in both axes so phones with a different aspect ratio do not
		// leave black bands or let the slot and its fixed-size children disagree.
		HUDCameraFitScale = FMath::Min(
			GameViewportSize.X / SafeDesignX,
			GameViewportSize.Y / SafeDesignY);
		HUDCameraFrameSize = GameViewportSize;
		HUDCameraFrameOffset = GameViewportOffset;
		auto PlaceDesignedWidget = [this](UUserWidget* Widget)
		{
			if (!Widget)
			{
				return;
			}
			// Let UMG apply DPI and Canvas layout once. Manual X/Y scaling here
			// desynchronizes Android's layout geometry from its rendered pixels.
			Widget->SetAlignmentInViewport(FVector2D::ZeroVector);
			Widget->SetDesiredSizeInViewport(HUDCameraFrameSize);
			Widget->SetRenderTransformPivot(FVector2D::ZeroVector);
			Widget->SetRenderScale(FVector2D(1.f, 1.f));
			Widget->SetPositionInViewport(HUDCameraFrameOffset, false);
		};
		PlaceDesignedWidget(MainHUDWidget);
		auto PlaceResultWidget = [&PlaceDesignedWidget](UUserWidget* ResultWidget)
		{
			PlaceDesignedWidget(ResultWidget);
		};
		PlaceResultWidget(SuccessResultWidget);
		PlaceResultWidget(FailureResultWidget);

		if (!HUDCameraFrameSize.Equals(LastNotifiedCameraFrameSize)
			|| !HUDCameraFrameOffset.Equals(LastNotifiedCameraFrameOffset))
		{
			LastNotifiedCameraFrameSize = HUDCameraFrameSize;
			LastNotifiedCameraFrameOffset = HUDCameraFrameOffset;
			BP_OnHUDCameraFrameChanged(
				HUDCameraFrameOffset,
				HUDCameraFrameSize,
				HUDCameraFitScale);
		}
		return;
	}

	const FVector2D Placement(
		FMath::Max(0.f, (static_cast<float>(FullX) - GameW) * 0.5f) / Dpi + MainHUDLeftMargin,
		FMath::Max(0.f, (static_cast<float>(FullY) - GameH) * 0.5f) / Dpi + MainHUDTopMargin);

	// Legacy path for old maps which still use a fixed top-left placement.
	const float EffectiveHUDScale = MainHUDScale / Dpi;
	if (Placement.Equals(MainHUDPlacement)
		&& FMath::IsNearlyEqual(MainHUDWidget->GetRenderTransform().Scale.X, EffectiveHUDScale))
	{
		return;
	}

	MainHUDWidget->SetRenderScale(FVector2D(EffectiveHUDScale, EffectiveHUDScale));
	// Placement has already been converted to Slate/design units, so keep the
	// precomputed DPI conversion instead of applying it a second time.
	MainHUDWidget->SetPositionInViewport(Placement, false);
	MainHUDPlacement = Placement;
}
void ANightCourseHUD::SetHealthBarNumeric(FName PropertyName, double Value)
{
	if (!HealthBarWidget)
	{
		return;
	}
	FNumericProperty* Num =
		CastField<FNumericProperty>(HealthBarWidget->GetClass()->FindPropertyByName(PropertyName));
	if (Num && Num->IsFloatingPoint())
	{
		Num->SetFloatingPointPropertyValue(
			Num->ContainerPtrToValuePtr<void>(HealthBarWidget), Value);
	}
}

#pragma region K2 moonyfli
float ANightCourseHUD::EvaluateComboProgress(int32 Combo, int32 FullAt, float Exponent) const
{
	if (Combo <= 1 || FullAt <= 1)
	{
		return 0.f;
	}
	const float LinearT = FMath::Clamp(
		static_cast<float>(Combo - 1) / static_cast<float>(FullAt - 1),
		0.f,
		1.f);
	return FMath::Pow(LinearT, FMath::Max(0.01f, Exponent));
}

void ANightCourseHUD::ApplyComboNumberStyle(int32 Combo, float DeltaSeconds)
{
	if (Combo <= 0)
	{
		LastDisplayedCombo = 0;
		ComboPopElapsed = ComboPopSeconds;
		if (ComboWidget)
		{
			ComboWidget->SetRenderScale(FVector2D(1.f, 1.f));
		}
		return;
	}

	if (Combo > LastDisplayedCombo)
	{
		ComboPopElapsed = 0.f;
	}
	LastDisplayedCombo = Combo;
	ComboPopElapsed += FMath::Max(0.f, DeltaSeconds);

	float ScaleAlpha = EvaluateComboProgress(Combo, ComboScaleFullAt, ComboScaleExponent);
	if (ComboScaleCurve)
	{
		const float LinearT = EvaluateComboProgress(Combo, ComboScaleFullAt, 1.f);
		ScaleAlpha = FMath::Clamp(ComboScaleCurve->GetFloatValue(LinearT), 0.f, 1.f);
	}
	const float PopT = ComboPopSeconds > KINDA_SMALL_NUMBER
		? FMath::Clamp(ComboPopElapsed / ComboPopSeconds, 0.f, 1.f)
		: 1.f;
	const float Scale = FMath::Lerp(ComboScaleMin, ComboScaleMax, ScaleAlpha)
		+ ComboPopExtraScale * (1.f - PopT);
	if (ComboWidget)
	{
		ComboWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		ComboWidget->SetRenderScale(FVector2D(Scale, Scale));
	}

	if (!ComboCountText)
	{
		return;
	}

	const float ColorAlpha = EvaluateComboProgress(Combo, ComboColorFullAt, 1.f);
	FSlateFontInfo Font = ComboCountText->GetFont();
	Font.OutlineSettings.OutlineSize = FMath::Max(0, ComboOutlineSize);
	Font.OutlineSettings.OutlineColor = FLinearColor::LerpUsingHSV(
		ComboOutlineColor,
		ComboOutlineColorAtMax,
		ColorAlpha);
	ComboCountText->SetFont(Font);
	ComboCountText->SetColorAndOpacity(FSlateColor(FLinearColor::LerpUsingHSV(
		ComboTextColor,
		ComboTextColorAtMax,
		ColorAlpha)));
}

void ANightCourseHUD::PushComboToHUD(int32 Combo)
{
	EnsureMainHUD();
	if (!ComboWidget)
	{
		return;
	}

	const bool bShow = Combo > 0;
	ComboWidget->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	ApplyComboNumberStyle(Combo, DeltaSeconds);
	if (bShow && ComboCountText)
	{
		ComboCountText->SetText(FText::AsNumber(Combo));
	}
}
#pragma endregion K2 moonyfli

void ANightCourseHUD::PushSoulToHealthBar(float Soul)
{
	EnsureMainHUD();
	if (!HealthBarWidget)
	{
		return;
	}

	UFunction* SetHealth = HealthBarWidget->FindFunction(FName(TEXT("SetHealth")));
	if (!SetHealth)
	{
		return;
	}

	// WBP_HealthBar.SetHealth uses Blueprint real pins (FDoubleProperty in UE5), not C++ float.
	TArray<FNumericProperty*> NumericParms;
	for (TFieldIterator<FProperty> It(SetHealth); It && It->HasAnyPropertyFlags(CPF_Parm)
		 && !It->HasAnyPropertyFlags(CPF_ReturnParm); ++It)
	{
		if (FNumericProperty* Num = CastField<FNumericProperty>(*It))
		{
			if (Num->IsFloatingPoint())
			{
				NumericParms.Add(Num);
			}
		}
	}
	if (NumericParms.Num() == 0)
	{
		return;
	}

	const int32 Bytes = SetHealth->ParmsSize;
	uint8* Buffer = static_cast<uint8*>(FMemory_Alloca(Bytes));
	FMemory::Memzero(Buffer, Bytes);
	NumericParms[0]->SetFloatingPointPropertyValue(NumericParms[0]->ContainerPtrToValuePtr<void>(Buffer), Soul);
	if (NumericParms.Num() >= 2)
	{
		NumericParms[1]->SetFloatingPointPropertyValue(NumericParms[1]->ContainerPtrToValuePtr<void>(Buffer), 100.0);
	}
	HealthBarWidget->ProcessEvent(SetHealth, Buffer);
}

void ANightCourseHUD::DrawHUD()
{
	Super::DrawHUD();
	SyncCourseTipWidgetLifetime();

	if (!Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	if (IsCourseTipVisible() && bDrawCourseTipOnCanvas)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, 0.f, W, H);

		const float FitScale = H / FMath::Max(1.f, HUDDesignSize.Y);
		const float Left = (W - HUDDesignSize.X * FitScale) * 0.5f;
		float CursorY = H * 0.62f;
		if (UTexture2D* Portrait = CourseTipPortrait.Get() ? CourseTipPortrait.Get() : CourseTipPortrait.LoadSynchronous())
		{
			const float PortraitSize = 220.f * FitScale;
			Canvas->K2_DrawTexture(
				Portrait,
				FVector2D(Left + 40.f * FitScale, CursorY),
				FVector2D(PortraitSize, PortraitSize),
				FVector2D::ZeroVector,
				FVector2D::UnitVector,
				FLinearColor::White,
				BLEND_Translucent);
		}

		UFont* TipFont = GEngine ? GEngine->GetLargeFont() : nullptr;
		auto DrawTipLine = [this, TipFont](const FVector2D& Pos, const FText& Text, const FLinearColor& Color, const FVector2D& Scale)
		{
			if (!TipFont || Text.IsEmpty())
			{
				return;
			}
			FCanvasTextItem Item(Pos, Text, TipFont, Color);
			Item.EnableShadow(FLinearColor::Black);
			Item.Scale = Scale;
			Canvas->DrawItem(Item);
		};
		const float TextX = Left + 280.f * FitScale;
		DrawTipLine(FVector2D(TextX, CursorY), ActiveCourseTipSpeaker, FLinearColor(1.f, 0.86f, 0.45f), FVector2D(1.6f, 1.6f));
		DrawTipLine(FVector2D(TextX, CursorY + 48.f * FitScale), ActiveCourseTipBody, FLinearColor::White, FVector2D(1.35f, 1.35f));
		DrawTipLine(
			FVector2D(TextX, CursorY + 160.f * FitScale),
			ActiveCourseTipContinue,
			FLinearColor(0.8f, 0.8f, 0.8f),
			FVector2D(1.1f, 1.1f));
	}

	if (IsStartScreenVisible())
	{
		if (UTexture2D* StartTexture = StartScreenTexture.Get())
		{
			// The supplied art is authored at 9:20. Fit its height and center it in wider
			// PC windows, preserving the same portrait frame used on Android.
			const float FitScale = H / FMath::Max(1.f, HUDDesignSize.Y);
			const float DrawWidth = HUDDesignSize.X * FitScale;
			const float DrawX = (W - DrawWidth) * 0.5f;
			Canvas->K2_DrawTexture(
				StartTexture,
				FVector2D(DrawX, 0.f),
				FVector2D(DrawWidth, H),
				FVector2D::ZeroVector,
				FVector2D::UnitVector,
				FLinearColor::White,
				BLEND_Translucent);
		}
	}

	ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(GetOwningPawn());
	UNightCourseDirector* Director = CoursePawn
		? CoursePawn->GetCourseDirector()
		: nullptr;
	UNightFeelStubComponent* Feel = CoursePawn ? CoursePawn->FeelStub : nullptr;
	const float Soul = Feel ? Feel->Soul : 100.f;
	PushSoulToHealthBar(Soul);
	PushComboToHUD(Feel ? Feel->Combo : 0);
	UpdateMainHUDPlacement();

	if (bShowTouchZones)
	{
		DrawRect(
			FLinearColor(0.15f, 0.55f, 0.95f, 0.18f),
			JumpTouchNorm.X * W,
			JumpTouchNorm.Y * H,
			JumpTouchNorm.Z * W,
			JumpTouchNorm.W * H);
		DrawRect(
			FLinearColor(0.9f, 0.22f, 0.18f, 0.18f),
			AttackTouchNorm.X * W,
			AttackTouchNorm.Y * H,
			AttackTouchNorm.Z * W,
			AttackTouchNorm.W * H);
	}

	if (bShowDebugHud)
	{
		const bool bForkChoice = Director && Director->IsForkChoiceActive();
		if (!HealthBarWidget)
		{
			FCanvasTextItem SoulText(
				FVector2D(W * 0.06f, H * 0.05f),
				FText::FromString(FString::Printf(TEXT("SOUL  %.0f"), Soul)),
				GEngine->GetLargeFont(),
				FLinearColor(1.f, 0.95f, 0.7f));
			SoulText.Scale = FVector2D(1.6f, 1.6f);
			Canvas->DrawItem(SoulText);
		}

		if (Feel && Feel->bHasActiveRequest && !bForkChoice)
		{
			const bool bAttack = (Feel->ActiveRequest.Kind == ENightNodeKind::Enemy);
			FCanvasTextItem PromptText(
				FVector2D(W * 0.28f, H * 0.12f),
				FText::FromString(bAttack ? TEXT("NOW: ATTACK") : TEXT("NOW: JUMP")),
				GEngine->GetLargeFont(),
				bAttack ? FLinearColor(1.f, 0.3f, 0.25f) : FLinearColor(0.3f, 0.75f, 1.f));
			PromptText.Scale = FVector2D(2.0f, 2.0f);
			Canvas->DrawItem(PromptText);
		}

		if (Director)
		{
			const FString PhaseText = [&Director]()
			{
				switch (Director->GetPhase())
				{
				case ENightCoursePhase::ForkChoice: return FString(TEXT("FORK CHOICE"));
				case ENightCoursePhase::BranchEnterBuffer: return FString(TEXT("BRANCH BUFFER"));
				case ENightCoursePhase::BranchSegment: return FString(TEXT("BRANCH"));
				case ENightCoursePhase::KeySwapWarning: return FString(TEXT("KEY SWAP WARNING"));
				case ENightCoursePhase::KeySwapSafetyHold: return FString(TEXT("KEY SWAP HOLD"));
				case ENightCoursePhase::Failed: return FString(TEXT("NIGHT FAILED"));
				case ENightCoursePhase::Finished: return FString(TEXT("NIGHT CLEAR"));
				default: return FString(TEXT("BASE"));
				}
			}();
			const FString RouteText = Director->GetCurrentRoute() == ENightRouteId::None
				? TEXT("-")
				: FString::Printf(TEXT("%d"), static_cast<int32>(Director->GetCurrentRoute()));
			FCanvasTextItem StatusText(
				FVector2D(W * 0.06f, H * 0.12f),
				FText::FromString(FString::Printf(TEXT("%s   ROUTE %s"), *PhaseText, *RouteText)),
				GEngine->GetLargeFont(),
				Director->IsCourseFailed() ? FLinearColor(1.f, 0.15f, 0.1f) : FLinearColor::White);
			StatusText.Scale = FVector2D(1.1f, 1.1f);
			Canvas->DrawItem(StatusText);
		}
	}

	EnsureDropDirectorBinding(Director);
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	DrawDropFlyIcons(DeltaSeconds);
	DrawFailSideFlash(DeltaSeconds);
}

void ANightCourseHUD::EnsureDropDirectorBinding(UNightCourseDirector* Director)
{
	if (UNightCourseDirector* Previous = BoundDropDirector.Get())
	{
		if (Previous != Director)
		{
			Previous->OnIngredientDropped.RemoveDynamic(
				this,
				&ANightCourseHUD::HandleIngredientDropped);
		}
	}

	BoundDropDirector = Director;
	if (!Director)
	{
		return;
	}

	// Always remove+add. Live Coding / tip HUD edits can leave a stale dynamic
	// binding for the same Director pointer; early-return would skip fly icons.
	Director->OnIngredientDropped.RemoveDynamic(
		this,
		&ANightCourseHUD::HandleIngredientDropped);
	Director->OnIngredientDropped.AddUniqueDynamic(
		this,
		&ANightCourseHUD::HandleIngredientDropped);
}

void ANightCourseHUD::HandleIngredientDropped(
	EIngredientId DropId,
	int32 Count,
	FVector WorldLocation)
{
	if (!bEnableDropFlyIcons || DropId == EIngredientId::None || Count <= 0)
	{
		return;
	}
	UTexture2D* Icon = ResolveIngredientIcon(DropId);
	if (!Icon)
	{
		return;
	}

	// One icon per awarded unit, staggered, but never more than the on-screen budget.
	const int32 Budget = FMath::Max(0, MaxDropFlyIcons - DropFlyIcons.Num());
	const int32 SpawnCount = FMath::Min(Count, Budget);
	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		FNightDropFlyIcon& Entry = DropFlyIcons.AddDefaulted_GetRef();
		Entry.Icon = Icon;
		Entry.WorldStart = WorldLocation + FVector(0.f, 0.f, DropFlyWorldZOffset);
		Entry.Delay = DropFlyStaggerSeconds * static_cast<float>(Index);
	}
	if (SpawnCount > 0)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightHUD] Drop fly spawned id=%d count=%d (active=%d)."),
			static_cast<int32>(DropId),
			SpawnCount,
			DropFlyIcons.Num());
	}
}

#pragma region K2 moonyfli
namespace NightHudIngredientIcons
{
	UTexture2D* KeepNightFoodTextureReady(UTexture2D* Icon)
	{
#if PLATFORM_ANDROID
		if (Icon)
		{
			// Same as Day: Android can return a sync-loaded texture before the first
			// mobile mip is ready for Canvas. Hold it resident for the fly-in.
			Icon->SetForceMipLevelsToBeResident(2.0f);
			Icon->WaitForStreaming();
		}
#endif
		return Icon;
	}

	const TCHAR* DayFoodFallbackPath(EIngredientId DropId)
	{
		switch (DropId)
		{
		case EIngredientId::F01_LingGu:
			return TEXT("/Game/Day/Art/food/food_rice_V0.food_rice_V0");
		case EIngredientId::F02_YinShanJun:
			return TEXT("/Game/Day/Art/food/food_egg_V0.food_egg_V0");
		case EIngredientId::F03_ChiYanJiao:
			return TEXT("/Game/Day/Art/food/food_hand_V0.food_hand_V0");
		case EIngredientId::F04_YueLinYu:
			return TEXT("/Game/Day/Art/food/food_fish_V0.food_fish_V0");
		case EIngredientId::F05_XuanYuQin:
			return TEXT("/Game/Day/Art/food/food_leg_V0.food_leg_V0");
		default:
			return nullptr;
		}
	}
}

UTexture2D* ANightCourseHUD::ResolveIngredientIcon(EIngredientId DropId)
{
	if (const TObjectPtr<UTexture2D>* Cached = ResolvedIngredientIcons.Find(DropId))
	{
		if (UTexture2D* CachedIcon = Cached->Get())
		{
			return NightHudIngredientIcons::KeepNightFoodTextureReady(CachedIcon);
		}
		// Do not keep a permanent nullptr: Android may miss the first soft load.
		ResolvedIngredientIcons.Remove(DropId);
	}

	UTexture2D* Icon = nullptr;
	FName RowName = NAME_None;
	if (UDataTable* Table = IngredientIconTable.LoadSynchronous())
	{
		if (const UEnum* IdEnum = StaticEnum<EIngredientId>())
		{
			// DT_Ingredients row names match enum DisplayName (F01_LingGu -> LingGu).
			RowName = FName(*IdEnum->GetDisplayNameTextByValue(
				static_cast<int64>(DropId)).ToString());
			if (const FSIngredientDefRow* Row = Table->FindRow<FSIngredientDefRow>(
				RowName,
				TEXT("NightCourseHUD drop icon"),
				false))
			{
				Icon = Row->Icon.LoadSynchronous();
			}
		}
	}

	if (!Icon)
	{
		if (const TCHAR* Fallback = NightHudIngredientIcons::DayFoodFallbackPath(DropId))
		{
			Icon = LoadObject<UTexture2D>(nullptr, Fallback);
		}
	}

	Icon = NightHudIngredientIcons::KeepNightFoodTextureReady(Icon);
	if (Icon)
	{
		ResolvedIngredientIcons.Add(DropId, Icon);
		return Icon;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[NightHUD] Drop icon missing for %s (table row '%s' and Day food fallback); fly-in skipped."),
		*UEnum::GetValueAsString(DropId),
		*RowName.ToString());
	return nullptr;
}
#pragma endregion K2 moonyfli

FVector2D ANightCourseHUD::GetCanvasLetterboxPixelOffset() const
{
	if (!Canvas)
	{
		return FVector2D::ZeroVector;
	}
	int32 FullX = 0;
	int32 FullY = 0;
	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->GetViewportSize(FullX, FullY);
	}
	return FVector2D(
		FMath::Max(0.f, (static_cast<float>(FullX) - Canvas->SizeX) * 0.5f),
		FMath::Max(0.f, (static_cast<float>(FullY) - Canvas->SizeY) * 0.5f));
}

bool ANightCourseHUD::GetBagFlyTargetCanvasPosition(FVector2D& OutCanvasPosition) const
{
	UWidget* Target = nullptr;
	if (BagPackWidget)
	{
		Target = BagPackWidget->GetWidgetFromName(BagFlyTargetWidgetName);
		if (!Target)
		{
			Target = BagPackWidget;
		}
	}

	if (Target)
	{
		const FGeometry& Geometry = Target->GetCachedGeometry();
		const FVector2D LocalSize = Geometry.GetLocalSize();
		if (LocalSize.X > 0.f || LocalSize.Y > 0.f)
		{
			// Widget geometry is window-absolute; Canvas excludes the letterbox bars.
			FVector2D PixelPosition = FVector2D::ZeroVector;
			FVector2D ViewportPosition = FVector2D::ZeroVector;
			USlateBlueprintLibrary::LocalToViewport(
				this,
				Geometry,
				LocalSize * 0.5f,
				PixelPosition,
				ViewportPosition);
			OutCanvasPosition = PixelPosition - GetCanvasLetterboxPixelOffset();
			return true;
		}
	}

	if (HUDCameraFitScale <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	// Fallback: design-space point inside the 9:20 frame, converted to Canvas pixels.
	const float Dpi = UWidgetLayoutLibrary::GetViewportScale(this);
	OutCanvasPosition =
		(HUDCameraFrameOffset + BagFlyTargetFallback * HUDCameraFitScale) * Dpi
		- GetCanvasLetterboxPixelOffset();
	return true;
}

void ANightCourseHUD::DrawDropFlyIcons(float DeltaSeconds)
{
	if (DropFlyIcons.Num() == 0)
	{
		return;
	}
	if (!bEnableDropFlyIcons || !Canvas)
	{
		DropFlyIcons.Reset();
		return;
	}

	FVector2D TargetCanvas = FVector2D::ZeroVector;
	if (!GetBagFlyTargetCanvasPosition(TargetCanvas))
	{
		return;
	}

	const float FlySeconds = FMath::Max(0.05f, DropFlySeconds);
	const float Scale = FMath::Max(KINDA_SMALL_NUMBER, HUDCameraFitScale);
	const float BaseSize = DropFlyIconSize * Scale;

	for (int32 Index = DropFlyIcons.Num() - 1; Index >= 0; --Index)
	{
		FNightDropFlyIcon& Entry = DropFlyIcons[Index];
		if (Entry.Delay > 0.f)
		{
			Entry.Delay -= DeltaSeconds;
			continue;
		}
		if (!Entry.bCanvasStartValid)
		{
			// Sampled once: the foe is already gone, so the start point must stay fixed
			// on screen instead of tracking a camera that keeps moving.
			const FVector Projected = Project(Entry.WorldStart);
			Entry.CanvasStart = FVector2D(Projected.X, Projected.Y);
			Entry.bCanvasStartValid = true;
		}

		Entry.Elapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(Entry.Elapsed / FlySeconds, 0.f, 1.f);
		if (Alpha >= 1.f || !Entry.Icon)
		{
			DropFlyIcons.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		// Ease out so the icon leaves the kill fast and settles into the mouth.
		const float Eased = 1.f - FMath::Pow(1.f - Alpha, 2.f);
		FVector2D Position = FMath::Lerp(Entry.CanvasStart, TargetCanvas, Eased);

		// Bow the path perpendicular to the straight line; peaks at the midpoint.
		const FVector2D Travel = TargetCanvas - Entry.CanvasStart;
		if (!Travel.IsNearlyZero())
		{
			const FVector2D Normal =
				FVector2D(-Travel.Y, Travel.X).GetSafeNormal();
			Position += Normal * (DropFlyArcHeight * Scale
				* FMath::Sin(Eased * PI));
		}

		const float IconScale = FMath::Lerp(1.f, DropFlyEndScale, Eased);
		const float Size = BaseSize * IconScale;
		const float FadeAlpha = Alpha < 0.75f
			? 1.f
			: FMath::GetMappedRangeValueClamped(
				FVector2f(0.75f, 1.f),
				FVector2f(1.f, 0.f),
				Alpha);

		FCanvasTileItem Tile(
			Position - FVector2D(Size * 0.5f, Size * 0.5f),
			Entry.Icon->GetResource(),
			FVector2D(Size, Size),
			FLinearColor(1.f, 1.f, 1.f, FadeAlpha));
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
}

void ANightCourseHUD::NotifyFoeKilled(EFoeId FoeId, bool bPlayDrop)
{
	int32 Combo = 1;
	if (const ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(GetOwningPawn()))
	{
		Combo = FMath::Max(1, CoursePawn->GetSlashCombo());
	}
	PlayKillHaptic(Combo);

	if (!bEnableNightSfx)
	{
		return;
	}

	PlaySfx(SlashSound, SlashVolume);

	if (const FNightFoeHitSfx* Hit = FoeHitSounds.Find(FoeId))
	{
		PlaySfx(Hit->Voice, FoeHitVolume);
		PlaySfx(Hit->Material, FoeHitVolume);
	}

	if (bPlayDrop)
	{
		PlaySfx(
			IngredientDropSound,
			IngredientDropVolume,
			IngredientDropPlaySeconds,
			IngredientDropFadeSeconds);
	}
}

void ANightCourseHUD::PlayKillHaptic(const int32 Combo)
{
	if (!bEnableKillHaptic)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	// Linear ramp combo 1 → FullCombo, matching common mobile/console hit pulses:
	// light ~0.28/55ms, heavy ~0.72/110ms (not a continuous buzz).
	const int32 FullAt = FMath::Max(1, KillHapticFullCombo);
	const float T = FMath::Clamp(
		static_cast<float>(FMath::Max(1, Combo) - 1) / static_cast<float>(FullAt - 1 > 0 ? FullAt - 1 : 1),
		0.f,
		1.f);
	const float Intensity = FMath::Clamp(
		FMath::Lerp(KillHapticIntensityMin, KillHapticIntensityMax, T),
		0.f,
		1.f);
	const float Duration = FMath::Max(
		0.01f,
		FMath::Lerp(KillHapticDurationMin, KillHapticDurationMax, T));
	if (Intensity <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	PC->PlayDynamicForceFeedback(
		Intensity,
		Duration,
		true,
		true,
		true,
		true,
		EDynamicForceFeedbackAction::Start);
}

void ANightCourseHUD::NotifyFailSideFlash()
{
	if (!bEnableFailSideFlash)
	{
		return;
	}
	FailSideFlashRemaining = FMath::Max(0.05f, FailSideFlashSeconds);
}

void ANightCourseHUD::DrawFailSideFlash(const float DeltaSeconds)
{
	if (FailSideFlashRemaining <= 0.f || !Canvas)
	{
		return;
	}

	FailSideFlashRemaining = FMath::Max(0.f, FailSideFlashRemaining - FMath::Max(0.f, DeltaSeconds));
	const float Duration = FMath::Max(0.05f, FailSideFlashSeconds);
	const float LifeAlpha = FailSideFlashRemaining / Duration;
	// Ease-out so the flash pops then softens.
	const float PeakAlpha = FailSideFlashMaxAlpha * LifeAlpha * LifeAlpha;
	if (PeakAlpha <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float EdgeW = ScreenW * FMath::Clamp(FailSideFlashEdgeWidthNorm, 0.05f, 0.5f);
	constexpr int32 BandCount = 10;
	const float BandW = EdgeW / static_cast<float>(BandCount);
	for (int32 Band = 0; Band < BandCount; ++Band)
	{
		const float BandCenter = (static_cast<float>(Band) + 0.5f) / static_cast<float>(BandCount);
		// Opaque at the outer edge, transparent toward screen center.
		const float BandAlpha = PeakAlpha * (1.f - BandCenter);
		if (BandAlpha <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		FLinearColor Color = FailSideFlashColor;
		Color.A = BandAlpha;
		const float LeftX = BandW * static_cast<float>(Band);
		const float RightX = ScreenW - EdgeW + LeftX;
		DrawRect(Color, LeftX, 0.f, BandW + 1.f, ScreenH);
		DrawRect(Color, RightX, 0.f, BandW + 1.f, ScreenH);
	}
}

void ANightCourseHUD::PlaySfx(
	const TSoftObjectPtr<USoundBase>& SoftSound,
	float Volume,
	float StopAfterSeconds,
	float FadeOutSeconds)
{
	if (SoftSound.IsNull() || Volume <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	USoundBase* Sound = SoftSound.LoadSynchronous();
	if (!Sound)
	{
		return;
	}

	UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(this, Sound, Volume);
	if (!Comp || StopAfterSeconds <= KINDA_SMALL_NUMBER || !GetWorld())
	{
		return;
	}

	TWeakObjectPtr<UAudioComponent> WeakComp(Comp);
	const float Fade = FMath::Max(0.01f, FadeOutSeconds);
	FTimerHandle FadeHandle;
	GetWorld()->GetTimerManager().SetTimer(
		FadeHandle,
		[WeakComp, Fade]()
		{
			if (UAudioComponent* Alive = WeakComp.Get())
			{
				Alive->FadeOut(Fade, 0.f);
			}
		},
		StopAfterSeconds,
		false);
}
#pragma endregion K2 moonyfli
