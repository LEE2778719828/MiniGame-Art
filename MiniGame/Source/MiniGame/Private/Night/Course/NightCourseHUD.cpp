#include "Night/Course/NightCourseHUD.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTypes.h"
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
#include "Components/Widget.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#pragma region K2 moonyfli
namespace NightHudPads
{
	// Fractions of the viewport. DrawHUD and the pointer hit-test must stay on the same numbers
	// or a tap that looks like it hit a pad would miss (or the reverse).
	constexpr float JumpX = 0.08f;
	constexpr float AttackX = 0.56f;
	constexpr float ButtonY = 0.84f;
	constexpr float ButtonW = 0.36f;
	constexpr float ButtonH = 0.10f;

	bool Contains(float ScreenX, float ScreenY, float ViewX, float ViewY, float NX, float NY, float NW, float NH)
	{
		if (ViewX <= KINDA_SMALL_NUMBER || ViewY <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float X = NX * ViewX;
		const float Y = NY * ViewY;
		return ScreenX >= X && ScreenX <= X + NW * ViewX && ScreenY >= Y && ScreenY <= Y + NH * ViewY;
	}
}

bool ANightCourseHUD::HitTestActionButtons(
	float ScreenX,
	float ScreenY,
	float ViewX,
	float ViewY,
	ENightFeelInput& OutInput)
{
	using namespace NightHudPads;
	if (Contains(ScreenX, ScreenY, ViewX, ViewY, JumpX, ButtonY, ButtonW, ButtonH))
	{
		OutInput = ENightFeelInput::Jump;
		return true;
	}
	if (Contains(ScreenX, ScreenY, ViewX, ViewY, AttackX, ButtonY, ButtonW, ButtonH))
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
}
void ANightCourseHUD::BeginPlay()
{
	Super::BeginPlay();
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		if (!SceneLoadingTexture.IsNull())
		{
			GameInstance->RegisterSceneLoadingTexture(SceneLoadingTexture);
		}
	}
	EnsureMainHUD();
}

void ANightCourseHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HealthBarWidget = nullptr;
	ComboWidget = nullptr;
	ComboCountText = nullptr;
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
		MainHUDWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
		HUDCameraFitScale = FMath::Min(
			GameViewportSize.X / SafeDesignX,
			GameViewportSize.Y / SafeDesignY);
		HUDCameraFrameSize = FVector2D(SafeDesignX, SafeDesignY) * HUDCameraFitScale;
		HUDCameraFrameOffset = GameViewportOffset
			+ (GameViewportSize - HUDCameraFrameSize) * 0.5f;

		MainHUDWidget->SetRenderScale(FVector2D(1.f, 1.f));
		MainHUDWidget->SetPositionInViewport(HUDCameraFrameOffset, false);
		MainHUDWidget->SetDesiredSizeInViewport(HUDCameraFrameSize);
		auto PlaceResultWidget = [this](UUserWidget* ResultWidget)
		{
			if (!ResultWidget)
			{
				return;
			}
			ResultWidget->SetRenderScale(FVector2D(1.f, 1.f));
			ResultWidget->SetPositionInViewport(HUDCameraFrameOffset, false);
			ResultWidget->SetDesiredSizeInViewport(HUDCameraFrameSize);
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
void ANightCourseHUD::PushComboToHUD(int32 Combo)
{
	EnsureMainHUD();
	if (!ComboWidget)
	{
		return;
	}

	const bool bShow = Combo > 0;
	ComboWidget->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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

	if (!Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(GetOwningPawn());
	UNightCourseDirector* Director = CoursePawn
		? CoursePawn->GetCourseDirector()
		: nullptr;
	UNightFeelStubComponent* Feel = CoursePawn ? CoursePawn->FeelStub : nullptr;
	const bool bForkChoice = Director && Director->IsForkChoiceActive();
	const bool bSwappedControls =
		Feel && Feel->ControlScheme == ENightControlScheme::Swapped;

	using namespace NightHudPads;
	// Bottom control bar (portrait-friendly).
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f), 0.f, H * 0.78f, W, H * 0.22f);
	DrawRect(FLinearColor(0.15f, 0.55f, 0.95f, 0.9f), JumpX * W, ButtonY * H, ButtonW * W, ButtonH * H);
	DrawRect(FLinearColor(0.9f, 0.22f, 0.18f, 0.9f), AttackX * W, ButtonY * H, ButtonW * W, ButtonH * H);

	const FString LeftControl = bForkChoice
		? TEXT("Q  LEFT")
		: (bSwappedControls ? TEXT("Q  ATTACK") : TEXT("Q  JUMP"));
	const FString RightControl = bForkChoice
		? TEXT("E  RIGHT")
		: (bSwappedControls ? TEXT("E  JUMP") : TEXT("E  ATTACK"));
	FCanvasTextItem JumpText(FVector2D(W * 0.14f, H * 0.87f), FText::FromString(LeftControl), GEngine->GetLargeFont(), FLinearColor::White);
	JumpText.Scale = FVector2D(1.4f, 1.4f);
	Canvas->DrawItem(JumpText);

	FCanvasTextItem AttackText(FVector2D(W * 0.62f, H * 0.87f), FText::FromString(RightControl), GEngine->GetLargeFont(), FLinearColor::White);
	AttackText.Scale = FVector2D(1.4f, 1.4f);
	Canvas->DrawItem(AttackText);

	float Soul = 100.f;
	FString Prompt;
	FLinearColor PromptColor = FLinearColor(1.f, 0.9f, 0.3f);
	if (CoursePawn)
	{
		if (Feel)
		{
			Soul = Feel->Soul;
			if (Feel->bHasActiveRequest && !bForkChoice)
			{
				const bool bAttack = (Feel->ActiveRequest.Kind == ENightNodeKind::Enemy);
				Prompt = bAttack ? TEXT("NOW: ATTACK") : TEXT("NOW: JUMP");
				PromptColor = bAttack ? FLinearColor(1.f, 0.3f, 0.25f) : FLinearColor(0.3f, 0.75f, 1.f);
			}
		}
	}

	PushSoulToHealthBar(Soul);
	PushComboToHUD(Feel ? Feel->Combo : 0);
	UpdateMainHUDPlacement();
	if (!HealthBarWidget)
	{
		FCanvasTextItem SoulText(FVector2D(W * 0.06f, H * 0.05f), FText::FromString(FString::Printf(TEXT("SOUL  %.0f"), Soul)), GEngine->GetLargeFont(), FLinearColor(1.f, 0.95f, 0.7f));
		SoulText.Scale = FVector2D(1.6f, 1.6f);
		Canvas->DrawItem(SoulText);
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
		const int32 VisibleBlocks = Director->GetVisibleBlockCount();
		FCanvasTextItem StatusText(
			FVector2D(W * 0.06f, H * 0.12f),
			FText::FromString(FString::Printf(TEXT("%s   ROUTE %s"), *PhaseText, *RouteText)),
			GEngine->GetLargeFont(),
			Director->IsCourseFailed() ? FLinearColor(1.f, 0.15f, 0.1f) : FLinearColor::White);
		StatusText.Scale = FVector2D(1.1f, 1.1f);
		Canvas->DrawItem(StatusText);
		if (VisibleBlocks > 0)
		{
			FCanvasTextItem VisibilityText(
				FVector2D(W * 0.06f, H * 0.17f),
				FText::FromString(FString::Printf(TEXT("VISIBLE BLOCKS  %d"), VisibleBlocks)),
				GEngine->GetLargeFont(),
				FLinearColor(0.65f, 0.85f, 1.f));
			VisibilityText.Scale = FVector2D(0.9f, 0.9f);
			Canvas->DrawItem(VisibilityText);
		}

		if (bForkChoice)
		{
			FCanvasTextItem ForkText(
				FVector2D(W * 0.25f, H * 0.2f),
				FText::FromString(FString::Printf(
					TEXT("LEFT %d     RIGHT %d     %.1fs"),
					static_cast<int32>(Director->GetForkLeftRoute()),
					static_cast<int32>(Director->GetForkRightRoute()),
					Director->GetForkSecondsRemaining())),
				GEngine->GetLargeFont(),
				FLinearColor(1.f, 0.85f, 0.25f));
			ForkText.Scale = FVector2D(1.35f, 1.35f);
			Canvas->DrawItem(ForkText);
			const FString ForkHint = Director->GetForkHintText();
			if (!ForkHint.IsEmpty())
			{
				FCanvasTextItem HintText(
					FVector2D(W * 0.16f, H * 0.26f),
					FText::FromString(ForkHint),
					GEngine->GetLargeFont(),
					FLinearColor(0.65f, 0.9f, 1.f));
				HintText.Scale = FVector2D(0.85f, 0.85f);
				Canvas->DrawItem(HintText);
			}
		}
		else if (Director->IsKeySwapWarningActive())
		{
			FCanvasTextItem SwapText(
				FVector2D(W * 0.24f, H * 0.2f),
				FText::FromString(FString::Printf(
					TEXT("CONTROL SWAP  %.1fs"),
					Director->GetKeySwapSecondsRemaining())),
				GEngine->GetLargeFont(),
				FLinearColor(0.9f, 0.55f, 1.f));
			SwapText.Scale = FVector2D(1.35f, 1.35f);
			Canvas->DrawItem(SwapText);
		}
	}

	if (!Prompt.IsEmpty())
	{
		FCanvasTextItem PromptText(FVector2D(W * 0.28f, H * 0.12f), FText::FromString(Prompt), GEngine->GetLargeFont(), PromptColor);
		PromptText.Scale = FVector2D(2.0f, 2.0f);
		Canvas->DrawItem(PromptText);
	}
}
#pragma endregion K2 moonyfli
