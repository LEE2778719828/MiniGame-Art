#include "Night/Course/NightFeelStubComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#pragma region K2 moonyfli
UNightFeelStubComponent::UNightFeelStubComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNightFeelStubComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		SetupInput(Cast<APlayerController>(Pawn->GetController()));
	}
}

void UNightFeelStubComponent::SetupInput(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	// Mapping only here. Action binds live on the Pawn to avoid double-fire.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (MappingContext)
		{
			Subsystem->AddMappingContext(MappingContext, 1);
		}
	}
}

void UNightFeelStubComponent::NotifyJudgeRequest_Implementation(const FNightJudgeRequest& Request)
{
	ActiveRequest = Request;
	bHasActiveRequest = true;

	const bool bAttack = (Request.Kind == ENightNodeKind::Enemy);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			9911,
			999.f,
			bAttack ? FColor::Red : FColor::Cyan,
			bAttack ? TEXT("WINDOW: ATTACK (E / LMB)") : TEXT("WINDOW: JUMP (Q)"));
	}
}

void UNightFeelStubComponent::ClearJudgeRequest_Implementation(int32 NodeIndex)
{
	if (bHasActiveRequest && ActiveRequest.NodeIndex == NodeIndex)
	{
		bHasActiveRequest = false;
		ActiveRequest = FNightJudgeRequest();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(9911, 0.05f, FColor::Black, TEXT(""));
		}
	}
}

ENightJudgeOutcome UNightFeelStubComponent::TryResolveInput_Implementation(ENightFeelInput Input)
{
	if (!bHasActiveRequest)
	{
		return ENightJudgeOutcome::None;
	}

	const bool bExpectAttack = (ActiveRequest.Kind == ENightNodeKind::Enemy);
	const bool bExpectJump = (ActiveRequest.Kind == ENightNodeKind::Hazard);
	const bool bCorrect =
		(bExpectAttack && Input == ENightFeelInput::Attack) ||
		(bExpectJump && Input == ENightFeelInput::Jump);

	LastOutcome = bCorrect ? ENightJudgeOutcome::Success : ENightJudgeOutcome::WrongButton;
	const int32 ResolvedIndex = ActiveRequest.NodeIndex;
	OnDebugSoulChanged.Broadcast(Soul, LastOutcome);
	OnInputResolved.Broadcast(ResolvedIndex, LastOutcome);
	return LastOutcome;
}

float UNightFeelStubComponent::GetSoul_Implementation() const
{
	return Soul;
}

void UNightFeelStubComponent::ApplySoulPenalty_Implementation(float Amount, ENightJudgeOutcome Reason)
{
	Soul = FMath::Max(0.f, Soul - Amount);
	LastOutcome = Reason;
	OnDebugSoulChanged.Broadcast(Soul, LastOutcome);
}

void UNightFeelStubComponent::PlaySuccessFeedback_Implementation(ENightNodeKind Kind)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			9912,
			1.2f,
			FColor::Green,
			Kind == ENightNodeKind::Enemy ? TEXT("HIT OK") : TEXT("JUMP OK"));
	}
}

void UNightFeelStubComponent::PlayFailFeedback_Implementation(ENightJudgeOutcome Outcome, ENightNodeKind Kind)
{
	(void)Kind;
	if (GEngine)
	{
		const FString Msg = (Outcome == ENightJudgeOutcome::WrongButton)
			? TEXT("WRONG BUTTON")
			: TEXT("MISS");
		GEngine->AddOnScreenDebugMessage(9912, 1.2f, FColor::Orange, Msg);
	}
}

void UNightFeelStubComponent::HandleJump(const FInputActionValue& Value)
{
	(void)Value;
	// BlueprintNativeEvent interface: never call TryResolveInput() directly.
	TryResolveInput_Implementation(ENightFeelInput::Jump);
}

void UNightFeelStubComponent::HandleAttack(const FInputActionValue& Value)
{
	(void)Value;
	TryResolveInput_Implementation(ENightFeelInput::Attack);
}
#pragma endregion K2 moonyfli
