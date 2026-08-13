#include "Runner/RunnerCharacter.h"
#include "Runner/RunnerFlowComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#pragma region K2 moonyfli
ARunnerCharacter::ARunnerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	FlowComponent = CreateDefaultSubobject<URunnerFlowComponent>(TEXT("FlowComponent"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->BrakingDecelerationWalking = 2048.f;
	}
}

void ARunnerCharacter::BeginPlay()
{
	Super::BeginPlay();

	TrackForward = GetActorForwardVector().GetSafeNormal2D();
	if (TrackForward.IsNearlyZero())
	{
		TrackForward = FVector::ForwardVector;
	}

	if (FlowComponent)
	{
		FlowComponent->OnJudge.AddDynamic(this, &ARunnerCharacter::HandleJudge);
		FlowComponent->ResetRun();
	}
}

void ARunnerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ARunnerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ARunnerCharacter::HandleJumpInput);
		}
		if (AttackAction)
		{
			EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &ARunnerCharacter::HandleAttackInput);
		}
	}
}

void ARunnerCharacter::HandleJumpInput(const FInputActionValue& Value)
{
	RequestJump();
}

void ARunnerCharacter::HandleAttackInput(const FInputActionValue& Value)
{
	RequestAttack();
}

void ARunnerCharacter::RequestJump()
{
	if (!FlowComponent)
	{
		return;
	}

	float ForwardDistance = 0.f;
	float JumpHeight = 0.f;
	ERunnerEventType EventType = ERunnerEventType::Gap;
	if (FlowComponent->TryResolveInput(ERunnerInputAction::Jump, ForwardDistance, JumpHeight, EventType))
	{
		ExecuteResolvedMove(ERunnerInputAction::Jump, ForwardDistance, JumpHeight, EventType);
	}
}

void ARunnerCharacter::RequestAttack()
{
	if (!FlowComponent)
	{
		return;
	}

	float ForwardDistance = 0.f;
	float JumpHeight = 0.f;
	ERunnerEventType EventType = ERunnerEventType::Gap;
	if (FlowComponent->TryResolveInput(ERunnerInputAction::Attack, ForwardDistance, JumpHeight, EventType))
	{
		ExecuteResolvedMove(ERunnerInputAction::Attack, ForwardDistance, JumpHeight, EventType);
	}
}

void ARunnerCharacter::ExecuteResolvedMove(ERunnerInputAction Action, float ForwardDistance, float JumpHeight, ERunnerEventType EventType)
{
	if (Action == ERunnerInputAction::Jump)
	{
		OnRunnerJumpStarted(ForwardDistance, JumpHeight);
		PlayForwardMove(ForwardDistance, JumpHeight, true);
	}
	else
	{
		OnRunnerAttackStarted(ForwardDistance);
		PlayForwardMove(ForwardDistance, 0.f, false);
	}
}

void ARunnerCharacter::HandleJudge(ERunnerJudgeResult Result, ERunnerEventType EventType)
{
	OnRunnerJudgeFeedback(Result, EventType);
}

void ARunnerCharacter::PlayForwardMove(float ForwardDistance, float JumpHeight, bool bIsJump)
{
	const FVector Start = GetActorLocation();
	const FVector End = Start + TrackForward * ForwardDistance;

	if (bIsJump && JumpHeight > 0.f)
	{
		LaunchCharacter(TrackForward * (ForwardDistance * 1.5f) + FVector(0.f, 0.f, JumpHeight * 2.f), true, true);
	}
	else
	{
		SetActorLocation(End, true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveTimerHandle);
		World->GetTimerManager().SetTimer(
			MoveTimerHandle,
			this,
			&ARunnerCharacter::OnMoveTimerFinished,
			bIsJump ? 0.45f : 0.12f,
			false);
	}
	else
	{
		OnMoveTimerFinished();
	}
}

void ARunnerCharacter::OnMoveTimerFinished()
{
	if (FlowComponent)
	{
		FlowComponent->NotifyMoveFinished();
	}
}
#pragma endregion K2 moonyfli
