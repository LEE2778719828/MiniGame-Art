#include "Day/Input/SDayPlayerController.h"

#include "Day/Presentation/SDayBoardPresentation.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

#pragma region K2 moonyfli
ASDayPlayerController::ASDayPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingFinder(
		TEXT("/Game/Day/Input/IMC_DayTouch.IMC_DayTouch"));
	if (MappingFinder.Succeeded())
	{
		MappingContext = MappingFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> PressFinder(
		TEXT("/Game/Day/Input/IA_Tap.IA_Tap"));
	if (PressFinder.Succeeded())
	{
		PointerPressAction = PressFinder.Object;
	}
}

void ASDayPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!PointerPressAction)
	{
		PointerPressAction = NewObject<UInputAction>(this, TEXT("IA_DayPointerPressRuntime"));
		PointerPressAction->ValueType = EInputActionValueType::Boolean;
	}
	BindPointerActions();
}

void ASDayPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ActivateTouchInterface(nullptr);
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	ActivateDayPointerContext();
	ResolveBoardPresenter();
}

void ASDayPlayerController::ActivateDayPointerContext()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem || !MappingContext)
	{
		return;
	}

	Subsystem->AddMappingContext(MappingContext, 0);
}

void ASDayPlayerController::BindPointerActions()
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput || !PointerPressAction)
	{
		return;
	}

	EnhancedInput->BindAction(
		PointerPressAction,
		ETriggerEvent::Started,
		this,
		&ASDayPlayerController::HandlePointerStarted);
	EnhancedInput->BindAction(
		PointerPressAction,
		ETriggerEvent::Triggered,
		this,
		&ASDayPlayerController::HandlePointerUpdated);
	EnhancedInput->BindAction(
		PointerPressAction,
		ETriggerEvent::Completed,
		this,
		&ASDayPlayerController::HandlePointerCompleted);
	EnhancedInput->BindAction(
		PointerPressAction,
		ETriggerEvent::Canceled,
		this,
		&ASDayPlayerController::HandlePointerCanceled);
}

void ASDayPlayerController::RegisterBoardPresenter(ASDayBoardPresenter* Presenter)
{
	if (!Presenter)
	{
		return;
	}
	BoardPresenter = Presenter;
	Presenter->SetUseExternalPointerDriver(true);
	bDrivingBoardPointer = true;
}

void ASDayPlayerController::ResolveBoardPresenter()
{
	if (BoardPresenter.IsValid())
	{
		RegisterBoardPresenter(BoardPresenter.Get());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASDayBoardPresenter> It(World); It; ++It)
	{
		RegisterBoardPresenter(*It);
		break;
	}
}

void ASDayPlayerController::ReadPointerScreenPosition(
	FVector2D& OutScreenPosition,
	const bool bPreferTouch) const
{
	float TouchX = 0.0f;
	float TouchY = 0.0f;
	bool bTouchPressed = false;
	GetInputTouchState(ETouchIndex::Touch1, TouchX, TouchY, bTouchPressed);
	if (bPreferTouch || bTouchPressed)
	{
		OutScreenPosition = FVector2D(TouchX, TouchY);
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	GetMousePosition(MouseX, MouseY);
	OutScreenPosition = FVector2D(MouseX, MouseY);
}

void ASDayPlayerController::HandlePointerStarted()
{
	if (!bDrivingBoardPointer)
	{
		ResolveBoardPresenter();
	}

	float TouchX = 0.0f;
	float TouchY = 0.0f;
	bool bTouchPressed = false;
	GetInputTouchState(ETouchIndex::Touch1, TouchX, TouchY, bTouchPressed);
	bActivePointerIsTouch = bTouchPressed;
	ReadPointerScreenPosition(LastScreenPosition, bActivePointerIsTouch);
	PressScreenPosition = LastScreenPosition;
	UE_LOG(LogTemp, Display, TEXT("Day pointer press at (%.0f, %.0f)"), LastScreenPosition.X, LastScreenPosition.Y);

	if (ASDayBoardPresenter* Presenter = BoardPresenter.Get())
	{
		Presenter->SimulatePointerEvent(LastScreenPosition, true);
	}
}

void ASDayPlayerController::HandlePointerUpdated()
{
	ReadPointerScreenPosition(LastScreenPosition, bActivePointerIsTouch);
}

void ASDayPlayerController::HandlePointerCompleted()
{
	if (!bActivePointerIsTouch)
	{
		ReadPointerScreenPosition(LastScreenPosition, false);
	}

	if (ASDayBoardPresenter* Presenter = BoardPresenter.Get())
	{
		Presenter->SimulatePointerEvent(LastScreenPosition, false);
		if (FVector2D::Distance(PressScreenPosition, LastScreenPosition) >= DragThresholdPixels)
		{
			Presenter->CancelPointerInteraction();
		}
	}
	bActivePointerIsTouch = false;
}

void ASDayPlayerController::HandlePointerCanceled()
{
	if (ASDayBoardPresenter* Presenter = BoardPresenter.Get())
	{
		Presenter->CancelPointerInteraction();
	}
	bActivePointerIsTouch = false;
}
#pragma endregion K2 moonyfli
