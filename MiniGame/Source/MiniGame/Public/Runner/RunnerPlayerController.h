#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RunnerPlayerController.generated.h"

class UInputMappingContext;

#pragma region K2 moonyfli
/** Portrait dual-button controller. UI buttons should call Character RequestJump/RequestAttack. */
UCLASS()
class MINIGAME_API ARunnerPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARunnerPlayerController();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Soft lock for portrait preview; true mobile lock is in project Android settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Mobile")
	bool bForcePortraitConstraint = true;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
};
#pragma endregion K2 moonyfli
