#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Night/Course/NightFeelBridge.h"
#include "NightFeelStubComponent.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNightFeelDebug, float, Soul, ENightJudgeOutcome, LastOutcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNightFeelInputResolved, int32, NodeIndex, ENightJudgeOutcome, Outcome);

#pragma region K2 moonyfli
/**
 * Built-in R1 Feel stub for G1 PIE.
 * Correct: Attack on Enemy, Jump on Hazard.
 */
UCLASS(ClassGroup = (Night), meta = (BlueprintSpawnableComponent))
class MINIGAME_API UNightFeelStubComponent : public UActorComponent, public INightFeelBridge
{
	GENERATED_BODY()

public:
	UNightFeelStubComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel")
	float Soul = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel")
	FNightJudgeRequest ActiveRequest;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel")
	bool bHasActiveRequest = false;

	UPROPERTY(BlueprintAssignable, Category = "Night|Feel|Debug")
	FOnNightFeelDebug OnDebugSoulChanged;

	UPROPERTY(BlueprintAssignable, Category = "Night|Feel")
	FOnNightFeelInputResolved OnInputResolved;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel|Debug")
	ENightJudgeOutcome LastOutcome = ENightJudgeOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel")
	ENightControlScheme ControlScheme = ENightControlScheme::Normal;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Night|Feel")
	void SetupInput(APlayerController* PC);

	virtual void NotifyJudgeRequest_Implementation(const FNightJudgeRequest& Request) override;
	virtual void ClearJudgeRequest_Implementation(int32 NodeIndex) override;
	virtual ENightJudgeOutcome TryResolveInput_Implementation(ENightFeelInput Input) override;
	virtual float GetSoul_Implementation() const override;
	virtual void ApplySoulPenalty_Implementation(float Amount, ENightJudgeOutcome Reason) override;
	virtual void PlaySuccessFeedback_Implementation(ENightNodeKind Kind) override;
	virtual void PlayFailFeedback_Implementation(ENightJudgeOutcome Outcome, ENightNodeKind Kind) override;
	virtual void SetControlScheme_Implementation(ENightControlScheme Scheme) override;
	virtual ENightControlScheme GetControlScheme_Implementation() const override;

	void HandleJump(const FInputActionValue& Value);
	void HandleAttack(const FInputActionValue& Value);

protected:
	ENightFeelInput RemapInput(ENightFeelInput Input) const;
};
#pragma endregion K2 moonyfli
