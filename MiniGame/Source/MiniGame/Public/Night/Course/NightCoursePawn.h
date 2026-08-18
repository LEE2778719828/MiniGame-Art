#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "NightCoursePawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UNightFeelStubComponent;
class UInputMappingContext;
class UInputAction;
class UStaticMeshComponent;

#pragma region K2 moonyfli
/** G1 pawn: rear-elevated TPP camera; advances only when Course tells it to. */
UCLASS(Blueprintable)
class MINIGAME_API ANightCoursePawn : public APawn
{
	GENERATED_BODY()

public:
	ANightCoursePawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<USceneComponent> ArtRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Feel")
	TObjectPtr<UNightFeelStubComponent> FeelStub;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Camera")
	float CameraArmLength = 620.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Camera")
	FRotator CameraBoomRotation = FRotator(-28.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Camera")
	FVector CameraBoomSocketOffset = FVector(0.f, 55.f, 160.f);

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SnapToTrack(const FVector& WorldLocation, const FRotator& WorldRotation);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void SetTrackTarget(const FVector& WorldLocation, const FRotator& WorldRotation);

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void BeginTrackAdvance(const FVector& WorldLocation, const FRotator& WorldRotation, float SpeedCmPerSec);

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	bool IsTrackAdvancing() const { return bTrackAdvancing; }

protected:
	void OnJumpPressed(const FInputActionValue& Value);
	void OnAttackPressed(const FInputActionValue& Value);
	void ApplyAvatarColor(FLinearColor Color);
	void ApplyRearElevatedCamera();

	FVector AdvanceTargetLocation = FVector::ZeroVector;
	FRotator AdvanceTargetRotation = FRotator::ZeroRotator;
	float AdvanceSpeed = 1400.f;
	bool bTrackAdvancing = false;
};
#pragma endregion K2 moonyfli
