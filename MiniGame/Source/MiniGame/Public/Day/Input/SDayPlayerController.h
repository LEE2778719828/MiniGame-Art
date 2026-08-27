#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SDayPlayerController.generated.h"

class ASDayBoardPresenter;
class UInputAction;
class UInputMappingContext;

#pragma region K2 moonyfli
/**
 * Day merge board pointer: press/hold/release on LMB or Touch1.
 * Screen position is read from the player controller, not from an Axis2D action
 * (Mouse2D is a delta, not a viewport location).
 */
UCLASS()
class MINIGAME_API ASDayPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASDayPlayerController();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S Day|Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S Day|Input")
	TObjectPtr<UInputAction> PointerPressAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S Day|Input", meta = (ClampMin = "0.0"))
	float DragThresholdPixels = 12.0f;

	void RegisterBoardPresenter(ASDayBoardPresenter* Presenter);

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;

private:
	bool ActivateDayPointerContext();
	void BindPointerActions();
	void ResolveBoardPresenter();
	void ReadPointerScreenPosition(FVector2D& OutScreenPosition, bool bPreferTouch) const;
	void HandlePointerStarted();
	void HandlePointerUpdated();
	void HandlePointerCompleted();
	void HandlePointerCanceled();

	TWeakObjectPtr<ASDayBoardPresenter> BoardPresenter;
	bool bDayPointerContextActive = false;
	bool bDrivingBoardPointer = false;
	bool bActivePointerIsTouch = false;
	FVector2D PressScreenPosition = FVector2D::ZeroVector;
	FVector2D LastScreenPosition = FVector2D::ZeroVector;
};
#pragma endregion K2 moonyfli
