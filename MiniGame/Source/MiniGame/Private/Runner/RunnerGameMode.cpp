#include "Runner/RunnerGameMode.h"
#include "Runner/RunnerCharacter.h"
#include "Runner/RunnerPlayerController.h"
#include "Runner/RunnerFlowComponent.h"
#include "Runner/RunnerTrackData.h"
#include "Kismet/GameplayStatics.h"

#pragma region K2 moonyfli
ARunnerGameMode::ARunnerGameMode()
{
	DefaultPawnClass = ARunnerCharacter::StaticClass();
	PlayerControllerClass = ARunnerPlayerController::StaticClass();
}

void ARunnerGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!DefaultTrackData)
	{
		return;
	}

	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (ARunnerCharacter* Runner = Cast<ARunnerCharacter>(Pawn))
		{
			if (Runner->FlowComponent)
			{
				Runner->FlowComponent->SetTrackData(DefaultTrackData);
			}
		}
	}
}
#pragma endregion K2 moonyfli
