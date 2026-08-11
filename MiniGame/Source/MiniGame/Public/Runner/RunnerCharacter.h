#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Runner/RunnerTypes.h"
#include "RunnerCharacter.generated.h"

class URunnerFlowComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

#pragma region K2 moonyfli
/**
 * Dual-button runner pawn. No stick locomotion.
 * Jump = large forward arc, Attack = kill + small forward step.
 */
UCLASS()
class MINIGAME_API ARunnerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARunnerCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<URunnerFlowComponent> FlowComponent;

	/** World forward used for whitebox track (usually actor forward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner")
	FVector TrackForward = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	UFUNCTION(BlueprintCallable, Category = "Runner")
	void RequestJump();

	UFUNCTION(BlueprintCallable, Category = "Runner")
	void RequestAttack();

	UFUNCTION(BlueprintImplementableEvent, Category = "Runner")
	void OnRunnerJumpStarted(float ForwardDistance, float JumpHeight);

	UFUNCTION(BlueprintImplementableEvent, Category = "Runner")
	void OnRunnerAttackStarted(float ForwardDistance);

	UFUNCTION(BlueprintImplementableEvent, Category = "Runner")
	void OnRunnerJudgeFeedback(ERunnerJudgeResult Result, ERunnerEventType EventType);

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	void HandleJumpInput(const FInputActionValue& Value);
	void HandleAttackInput(const FInputActionValue& Value);

	void ExecuteResolvedMove(ERunnerInputAction Action, float ForwardDistance, float JumpHeight, ERunnerEventType EventType);

	UFUNCTION()
	void HandleJudge(ERunnerJudgeResult Result, ERunnerEventType EventType);

	/** Simple whitebox move along TrackForward; replace with spline later. */
	void PlayForwardMove(float ForwardDistance, float JumpHeight, bool bIsJump);

	UFUNCTION()
	void OnMoveTimerFinished();

	FTimerHandle MoveTimerHandle;
};
#pragma endregion K2 moonyfli
