#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "NightCoursePawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UNightFeelStubComponent;
class UInputMappingContext;
class UInputAction;
class UStaticMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;
class USkeletalMeshComponent;
class UAnimSequence;
class UNightCourseDirector;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Visual")
	TObjectPtr<USkeletalMesh> HeroSkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Visual")
	TObjectPtr<UStaticMesh> HeroStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Visual")
	TObjectPtr<UMaterialInterface> HeroMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Visual")
	float HeroScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art|Visual")
	FVector HeroPivotOffsetCm = FVector::ZeroVector;

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

	/**
	 * 骨骼主角相对 ArtRoot 的落脚偏移。
	 *
	 * Z 默认 79.37 = 桥面顶高：muban1 局部顶面 5.29 × 桥的缩放 15。SK_Hero 与 kat 的原点
	 * 都在脚底，所以这个值就是「脚踩在桥面上」所需的抬升，Z=0 会让角色陷进桥里一半。
	 *
	 * 隐患：这是从 R2 当前的桥资产量出来的常数，他换网格或改缩放就会失准。
	 * 正解是让石头/桥自报落脚面高度，由 Director 转给主角——待与 R2 收口。
	 * 在那之前用 Night.Anim.Mesh 现调。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	FVector HeroMeshOffset = FVector(0.f, 0.f, 79.37f);

	/** Additional correction for this Blueprint's model root. Positive values move the visible Hero up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	float HeroZCompensationCm = 0.f;

	/**
	 * Controls only editor previews. Runtime-spawned Heroes always apply HeroZCompensationCm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Art")
	bool bApplyHeroZCompensationInPreview = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Anim")
	TObjectPtr<UAnimSequence> JumpAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Anim")
	TObjectPtr<UAnimSequence> AttackAnim;

	/**
	 * 动作播放倍率（<1 变慢、>1 变快）。常速斩击到位前，用它把快版拨到能接受的节奏。
	 * 追赶加速在此基础上再乘，所以这里改的是「基准速度」，不影响双时钟的压缩逻辑。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Anim", meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float JumpAnimRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Anim", meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float AttackAnimRate = 1.f;

	// add by K2 (R1) —— 动画驱动位移
	/**
	 * 开启后石间移动的时长由动画锚点决定，而不是 R2 配的 AdvanceSpeed。
	 * 好处是倍率一改，动作与位移一起缩放、永远同步；代价是移动速度随石距变化，
	 * 且石距不再影响节奏。默认关，先用 Night.Anim.Drive 1 对比过再决定。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Anim")
	bool bAnimDrivenAdvance = false;

	/**
	 * 动作与位移的对齐锚点（ms，倍率 1.0 下的值）。位移在锚点这一刻结束，锚点之后是收尾。
	 * 跳跃取落地帧、斩击取接触帧，由 Tools/MeasureAnimAnchors.py 量出。
	 * 等 TA 在接触帧打上 AnimNotify 后，这两个值应改为运行时读取。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Anim", meta = (ClampMin = "10.0"))
	float JumpAnchorMs = 266.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Anim", meta = (ClampMin = "10.0"))
	float AttackAnchorMs = 179.f;

	/**
	 * Camera framing lives on these two components and nowhere else: edit arm length, boom
	 * rotation, socket offset and FOV on the components in BP_NightCoursePawn and what you set
	 * is what runs. BeginPlay used to copy mirror UPROPERTYs over the boom, which silently
	 * discarded anything set here; it now only re-asserts the invariants listed on
	 * EnforceCameraInvariants.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Camera")
	TObjectPtr<UCameraComponent> Camera;

	// add by K2 (R1) —— 动作镜头反馈
	/**
	 * 起跳瞬间加到 FOV 上的度数，正值张开。触发时刻取自 Jump 动画上的 Takeoff 通知，
	 * 所以动画重新配时相机自动跟着走，这里不存时间、只存幅度。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Camera|Feel", meta = (ClampMin = "-30.0", ClampMax = "30.0"))
	float TakeoffFovKickDeg = 5.f;

	/** 落地瞬间镜头下沉的厘米数，负值向下。触发时刻取自 Land 通知。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Camera|Feel", meta = (ClampMin = "-200.0", ClampMax = "200.0"))
	float LandBoomDipCm = -14.f;

	/** 斩击接触瞬间加到 FOV 上的度数，负值收紧成"推近打击感"。触发时刻取自 Contact 通知。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Camera|Feel", meta = (ClampMin = "-30.0", ClampMax = "30.0"))
	float AttackFovKickDeg = -6.f;

	/** 上面三种冲击回到零的速度，越低余韵越长。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Camera|Feel", meta = (ClampMin = "0.5", ClampMax = "60.0"))
	float CameraKickRecoverySpeed = 9.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Feel")
	TObjectPtr<UNightFeelStubComponent> FeelStub;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputAction> AttackAction;

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

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void BindCourseDirector(UNightCourseDirector* InDirector);

	UFUNCTION(BlueprintPure, Category = "Night|Course")
	UNightCourseDirector* GetCourseDirector() const;

	/**
	 * add by K2 (R1): 加速未走完的这一段石间移动（负反应缓存命中时用）。
	 * 压缩量上限 MaxCompressSeconds，只影响当前这一段。
	 */
	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void ApplyAdvanceCatchUp(float RateMultiplier, float MaxCompressSeconds);

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void ApplyHeroMesh(
		UStaticMesh* Mesh,
		UMaterialInterface* MaterialOverride = nullptr,
		const FVector& PivotOffsetCm = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void ApplyConfiguredHeroVisual();

	UFUNCTION(BlueprintCallable, Category = "Night|Art")
	void ApplyHeroZCompensation(bool bPreview);

	/**
	 * add by K2 (R1): 判定成功时播对应动作，bAttack=false 播跳跃。
	 * 表现时钟从这里起算：随后的 ApplyAdvanceCatchUp 会按同样倍率把它一起加速。
	 */
	UFUNCTION(BlueprintCallable, Category = "Night|Anim")
	void PlayHeroAction(bool bAttack);

	/** 正在生效的播放倍率（基准 × 追赶加速）。 */
	UFUNCTION(BlueprintPure, Category = "Night|Anim")
	float GetHeroAnimPlayRate() const { return HeroAnimPlayRate; }

	/**
	 * add by K2 (R1): 当前动作还要播多久（秒），已计入播放倍率；没在播返回 0。
	 * 裁定 R-007 用它把「挂机扣血」的起算点推到动画播完之后。
	 */
	UFUNCTION(BlueprintPure, Category = "Night|Anim")
	float GetHeroActionRemainingSeconds() const;

	/** 骨骼主角在场时隐藏静态白模，两套美术只显示一套；改完 HeroMeshOffset 等外观参数后调它生效。 */
	void ResolveHeroArt(); //add by K2

protected:
	void OnJumpPressed(const FInputActionValue& Value);
	void OnAttackPressed(const FInputActionValue& Value);
	void OnHudPointerPressed();
	void OnHudTouchPressed(const ETouchIndex::Type FingerIndex, const FVector Location);
	void TryResolveHudPointer(float ScreenX, float ScreenY);
	void ApplyAvatarColor(FLinearColor Color);

	/**
	 * Re-asserts only what would break the lane camera if a Blueprint changed it: no controller
	 * rotation (nothing ever sets one, so the boom would point somewhere arbitrary), no boom
	 * collision test (the bridge deck would shove the camera around), and manual exposure (the
	 * night scene must not auto-adapt). Framing values are deliberately left to the components.
	 */
	void EnforceCameraInvariants();

	/**
	 * One scheduled camera impulse, queued when an action starts and fired once DelaySeconds runs
	 * out. Delays are stored already divided by the play rate in force at schedule time, so
	 * ApplyAdvanceCatchUp has to rescale them when it speeds the action up.
	 */
	struct FNightCameraKick
	{
		float DelaySeconds = 0.f;
		float FovDeg = 0.f;
		float DipCm = 0.f;
	};

	/** Reads Takeoff / Land / Contact off the clip and queues the matching impulses. */
	void ScheduleCameraKicksForClip(const UAnimSequenceBase* Clip);

	/** Fires due impulses, decays the live ones, and writes FOV / socket offset. */
	void UpdateCameraKicks(float DeltaSeconds);

	TArray<FNightCameraKick> PendingCameraKicks;

	/**
	 * Authored framing, cached at BeginPlay so impulses can be added on top without ever
	 * overwriting what the Blueprint components carry.
	 */
	float BaseFieldOfView = 70.f;
	FVector BaseSocketOffset = FVector(0.f, 55.f, 160.f);

	float LiveFovKickDeg = 0.f;
	float LiveBoomDipCm = 0.f;

	/** True while the components hold impulse-modified values, so they get restored exactly once. */
	bool bCameraKickApplied = false;

	FVector AdvanceTargetLocation = FVector::ZeroVector;
	FRotator AdvanceTargetRotation = FRotator::ZeroRotator;
	float AdvanceSpeed = 1400.f;
	bool bTrackAdvancing = false;
	bool bUsingHeroArt = false;
	FVector HeroRuntimeBaseLocation = FVector::ZeroVector;
	FVector HeroStaticRuntimeBaseLocation = FVector::ZeroVector;
	bool bHeroRuntimeBaseLocationCached = false;
	bool bHeroStaticRuntimeBaseLocationCached = false;

	/** 当前动作的播放倍率，追赶加速时在此基础上再乘。 */
	float HeroAnimPlayRate = 1.f;

	UPROPERTY(Transient)
	TObjectPtr<UNightCourseDirector> CourseDirector;

	/**
	 * 最近一次动作是不是斩击。PlayHeroAction 早于 Director 的 BeginTrackAdvance，
	 * 所以动画驱动模式靠它选锚点——Director 传进来的只有位置和速度，认不出节点类型。
	 */
	bool bLastActionWasAttack = false;
};
#pragma endregion K2 moonyfli
