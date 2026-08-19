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
class UStaticMesh;
class USkeletalMeshComponent;
class UAnimSequence;

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

	// add by K2 (R1) —— 骨骼网格主角
	/**
	 * 骨骼主角。加载成功时接管显示，R2 的静态网格随即隐藏（两者互斥，不做混排）。
	 * 想退回静态白模，把 bPreferSkeletalHero 关掉即可。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TObjectPtr<USkeletalMeshComponent> HeroSkelMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	bool bPreferSkeletalHero = true;

	/** 落点之间的朝向修正：美术资产的正面轴与赛道前进方向不一定一致。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	FRotator HeroMeshRotation = FRotator(0.f, -90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	FVector HeroMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Anim")
	TObjectPtr<UAnimSequence> JumpAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Anim")
	TObjectPtr<UAnimSequence> AttackAnim;

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

	/**
	 * add by K2 (R1): 加速未走完的这一段石间移动（负反应缓存命中时用）。
	 * 压缩量上限 MaxCompressSeconds，只影响当前这一段。
	 */
	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void ApplyAdvanceCatchUp(float RateMultiplier, float MaxCompressSeconds);

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void ApplyHeroMesh(UStaticMesh* Mesh);

	/**
	 * add by K2 (R1): 判定成功时播对应动作，bAttack=false 播跳跃。
	 * 表现时钟从这里起算：随后的 ApplyAdvanceCatchUp 会按同样倍率把它一起加速。
	 */
	UFUNCTION(BlueprintCallable, Category = "Night|Anim")
	void PlayHeroAction(bool bAttack);

protected:
	void OnJumpPressed(const FInputActionValue& Value);
	void OnAttackPressed(const FInputActionValue& Value);
	void ApplyAvatarColor(FLinearColor Color);
	void ApplyRearElevatedCamera();

	/** 骨骼主角在场时隐藏静态白模，两套美术只显示一套。 */
	void ResolveHeroArt();

	FVector AdvanceTargetLocation = FVector::ZeroVector;
	FRotator AdvanceTargetRotation = FRotator::ZeroRotator;
	float AdvanceSpeed = 1400.f;
	bool bTrackAdvancing = false;
	bool bUsingHeroArt = false;

	/** 当前动作的播放倍率，追赶加速时在此基础上再乘。 */
	float HeroAnimPlayRate = 1.f;
};
#pragma endregion K2 moonyfli
