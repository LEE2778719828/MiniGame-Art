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
#include "Blueprint/SlateBlueprintLibrary.h" //add by K2
#include "CanvasItem.h" //add by K2
#include "Engine/Canvas.h"
#include "Engine/DataTable.h" //add by K2
#include "Engine/Engine.h"
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
	IngredientIconTable = TSoftObjectPtr<UDataTable>(
		FSoftObjectPath(TEXT("/Game/Shared/Data/DT_Ingredients.DT_Ingredients"))); //add by K2

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
#pragma region K2 moonyfli
	BagPackWidget = nullptr;
	DropFlyIcons.Reset();
	ResolvedIngredientIcons.Reset();
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

	EnsureDropDirectorBinding(Director);
	DrawDropFlyIcons(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f);
}

void ANightCourseHUD::EnsureDropDirectorBinding(UNightCourseDirector* Director)
{
	if (BoundDropDirector.Get() == Director)
	{
		return;
	}
	if (UNightCourseDirector* Previous = BoundDropDirector.Get())
	{
		Previous->OnIngredientDropped.RemoveDynamic(
			this,
			&ANightCourseHUD::HandleIngredientDropped);
	}
	BoundDropDirector = Director;
	if (Director)
	{
		Director->OnIngredientDropped.AddUniqueDynamic(
			this,
			&ANightCourseHUD::HandleIngredientDropped);
	}
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
}

UTexture2D* ANightCourseHUD::ResolveIngredientIcon(EIngredientId DropId)
{
	if (const TObjectPtr<UTexture2D>* Cached = ResolvedIngredientIcons.Find(DropId))
	{
		return Cached->Get();
	}

	UDataTable* Table = IngredientIconTable.LoadSynchronous();
	if (!Table)
	{
		return nullptr;
	}

	// DT_Ingredients row names match the enum DisplayName metadata (F01_LingGu -> LingGu).
	const UEnum* IdEnum = StaticEnum<EIngredientId>();
	if (!IdEnum)
	{
		return nullptr;
	}
	const FName RowName(*IdEnum->GetDisplayNameTextByValue(
		static_cast<int64>(DropId)).ToString());
	const FSIngredientDefRow* Row = Table->FindRow<FSIngredientDefRow>(
		RowName,
		TEXT("NightCourseHUD drop icon"),
		false);
	if (!Row)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[NightHUD] DT_Ingredients has no row '%s'; that drop will not fly to the bag."),
			*RowName.ToString());
		ResolvedIngredientIcons.Add(DropId, nullptr);
		return nullptr;
	}

	UTexture2D* Icon = Row->Icon.LoadSynchronous();
	ResolvedIngredientIcons.Add(DropId, Icon);
	return Icon;
}

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
