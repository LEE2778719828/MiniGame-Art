#include "Runner/RunnerPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Runner/RunnerCharacter.h"

#pragma region K2 moonyfli
ARunnerPlayerController::ARunnerPlayerController()
{
	bShowMouseCursor = true;
}

void ARunnerPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Portrait lock for packaged Android is in DefaultEngine.ini.
	// Editor/PC preview uses 9:16 viewport; do not touch removed PC aspect members on UE5.8+.
	(void)bForcePortraitConstraint;
}

void ARunnerPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (MappingContext)
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	if (ARunnerCharacter* Runner = Cast<ARunnerCharacter>(InPawn))
	{
		if (!Runner->DefaultMappingContext && MappingContext)
		{
			Runner->DefaultMappingContext = MappingContext;
		}
	}
}
#pragma endregion K2 moonyfli
