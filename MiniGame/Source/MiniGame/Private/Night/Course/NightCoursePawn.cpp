#include "Night/Course/NightCoursePawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h" //add by K2
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Course/NightCourseDirector.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma region K2 moonyfli
ANightCoursePawn::ANightCoursePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	ArtRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArtRoot"));
	SetRootComponent(ArtRoot);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(ArtRoot);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	BodyMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.1f));

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(BodyMesh);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	HeadMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.35f));

	// add by K2 (R1): 骨骼主角。单节点播放，不挂 AnimBP —— 这一层只要「按一下动一下」，
	// 状态机等 idle 动画到位后再说。
	HeroSkelMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeroSkelMesh"));
	HeroSkelMesh->SetupAttachment(ArtRoot);
	HeroSkelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeroSkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	HeroSkelMesh->SetRelativeRotation(HeroMeshRotation);
	HeroSkelMesh->SetRelativeLocation(HeroMeshOffset);
	HeroSkelMesh->SetVisibility(false);
	HeroSkelMesh->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpClip(
		TEXT("/Game/Night/Character/Anims/Jump_noknife2.Jump_noknife2"));
	// 常速斩击（美术 0819 重导）。200ms 的 Slash_fast_Armature_Armature_kan 留作追赶加速的备选：
	// 姿势与这条相同，但出刀提前到 36%，天生适合赶拍。
	static ConstructorHelpers::FObjectFinder<UAnimSequence> AttackClip(
		TEXT("/Game/Night/Character/Anims/Slash.Slash"));

	if (JumpClip.Succeeded())
	{
		JumpAnim = JumpClip.Object;
	}
	if (AttackClip.Succeeded())
	{
		AttackAnim = AttackClip.Object;
	}

	// 刃心-style: behind + above the runner, looking forward down the lane.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(ArtRoot);
	SpringArm->TargetArmLength = CameraArmLength;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = false;
	SpringArm->SocketOffset = CameraBoomSocketOffset;
	SpringArm->SetRelativeRotation(CameraBoomRotation);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(70.f);
	Camera->bConstrainAspectRatio = false;
	Camera->PostProcessBlendWeight = 1.f;
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = 1.0f;
	Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Camera->PostProcessSettings.AutoExposureMinBrightness = 1.f;
	Camera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	Camera->PostProcessSettings.AutoExposureMaxBrightness = 1.f;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;

	FeelStub = CreateDefaultSubobject<UNightFeelStubComponent>(TEXT("FeelStub"));
}

void ANightCoursePawn::ApplyRearElevatedCamera()
{
	if (SpringArm)
	{
		SpringArm->TargetArmLength = CameraArmLength;
		SpringArm->bDoCollisionTest = false;
		SpringArm->bUsePawnControlRotation = false;
		SpringArm->SocketOffset = CameraBoomSocketOffset;
		SpringArm->SetRelativeLocation(FVector::ZeroVector);
		SpringArm->SetRelativeRotation(CameraBoomRotation);
	}
	if (Camera)
	{
		Camera->SetFieldOfView(70.f);
		Camera->bConstrainAspectRatio = false;
		Camera->PostProcessBlendWeight = 1.f;
		Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Camera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
		Camera->PostProcessSettings.AutoExposureBias = 1.0f;
		Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
		Camera->PostProcessSettings.AutoExposureMinBrightness = 1.f;
		Camera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
		Camera->PostProcessSettings.AutoExposureMaxBrightness = 1.f;
		Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	}
}

void ANightCoursePawn::BeginPlay()
{
	Super::BeginPlay();
	ApplyRearElevatedCamera();
	ApplyConfiguredHeroVisual();
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->ConsoleCommand(TEXT("r.EyeAdaptationQuality 0"));
			PC->ConsoleCommand(TEXT("r.EyeAdaptation.PreExposureOverride 1"));
		}
	}
}

void ANightCoursePawn::SnapToTrack(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	bTrackAdvancing = false;
	SetActorLocationAndRotation(WorldLocation, WorldRotation);
}

void ANightCoursePawn::SetTrackTarget(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	AdvanceTargetLocation = WorldLocation;
	AdvanceTargetRotation = WorldRotation;
}

void ANightCoursePawn::BeginTrackAdvance(const FVector& WorldLocation, const FRotator& WorldRotation, float SpeedCmPerSec)
{
	AdvanceTargetLocation = WorldLocation;
	AdvanceTargetRotation = WorldRotation;
	AdvanceSpeed = FMath::Max(100.f, SpeedCmPerSec);

	// add by K2 (R1): 动画驱动模式下丢掉 Director 传来的速度，改由锚点反推——
	// 这样位移在动作的关键帧（落地 / 接触）那一刻结束，倍率一改两者一起缩放。
	if (bAnimDrivenAdvance)
	{
		const float AnchorSeconds =
			FMath::Max(10.f, bLastActionWasAttack ? AttackAnchorMs : JumpAnchorMs)
			* 0.001f / FMath::Max(0.05f, HeroAnimPlayRate);
		const float Distance = FVector::Dist(GetActorLocation(), AdvanceTargetLocation);
		if (Distance > KINDA_SMALL_NUMBER)
		{
			AdvanceSpeed = FMath::Max(100.f, Distance / AnchorSeconds);
		}
	}

	bTrackAdvancing = true;
}

#pragma region K2 moonyfli
/**
 * 把主角材质铺到网格的每一个槽上。静态模 kat 只有一个槽，骨骼 SK_Hero 有两个，
 * 所以「只设槽 0」这种写法在骨骼上会漏掉一半。MID 也逐槽建：只给槽 0 调色会让
 * 两半颜色不一致，比不调色更难看。
 */
static void ApplyHeroMaterial(UMeshComponent* Component, UMaterialInterface* Material)
{
	if (!Component || !Material)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < Component->GetNumMaterials(); ++SlotIndex)
	{
		Component->SetMaterial(SlotIndex, Material);
		if (UMaterialInstanceDynamic* MID =
			Component->CreateAndSetMaterialInstanceDynamicFromMaterial(SlotIndex, Material))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.72f, 0.28f));
		}
	}
}
#pragma endregion K2 moonyfli

void ANightCoursePawn::ApplyHeroMesh(
	UStaticMesh* Mesh,
	UMaterialInterface* MaterialOverride,
	const FVector& PivotOffsetCm)
{
	if (!BodyMesh || !Mesh)
	{
		return;
	}

	bUsingHeroArt = true;
	BodyMesh->SetStaticMesh(Mesh);
	BodyMesh->SetVisibility(true);
	BodyMesh->SetHiddenInGame(false);
	const FVector MeshCenter = Mesh->GetBounds().Origin;
	const float AppliedHeroScale = FMath::Max(0.01f, HeroScale);
	BodyMesh->SetRelativeLocation(
		FVector(0.f, 0.f, 90.f) + (PivotOffsetCm - MeshCenter) * AppliedHeroScale);
	BodyMesh->SetRelativeScale3D(FVector(AppliedHeroScale));
	HeadMesh->SetVisibility(false);
	HeadMesh->SetHiddenInGame(true);
	ApplyHeroMaterial(BodyMesh, MaterialOverride); //add by K2
}

void ANightCoursePawn::ApplyConfiguredHeroVisual()
{
	if (HeroSkeletalMesh && HeroSkelMesh)
	{
		HeroSkelMesh->SetSkeletalMeshAsset(HeroSkeletalMesh);
		HeroSkelMesh->SetRelativeLocation(
			HeroMeshOffset + HeroPivotOffsetCm * HeroScale);
		HeroSkelMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, HeroScale)));
		// add by K2 (R1): SK_Hero 有两个槽（blinn3_002 / pasted__blinn3_002，源模型在 Maya 里
		// 粘贴几何体留下的重复材质），只喂槽 0 会让另一半留着导入时的灰色默认材质。
		// 逐槽覆盖并按静态模那条路同样建 MID，两条路径的着色保持一致。
		ApplyHeroMaterial(HeroSkelMesh, HeroMaterial);
		bPreferSkeletalHero = true;
		ResolveHeroArt();
		return;
	}

	if (HeroStaticMesh)
	{
		bPreferSkeletalHero = false;
		ApplyHeroMesh(HeroStaticMesh, HeroMaterial, HeroPivotOffsetCm);
		if (HeroSkelMesh)
		{
			HeroSkelMesh->SetVisibility(false);
			HeroSkelMesh->SetHiddenInGame(true);
		}
		return;
	}

	// Missing BP visual configuration intentionally produces an empty pawn.
	bUsingHeroArt = false;
	if (BodyMesh)
	{
		BodyMesh->SetStaticMesh(nullptr);
		BodyMesh->SetVisibility(false);
		BodyMesh->SetHiddenInGame(true);
	}
	if (HeadMesh)
	{
		HeadMesh->SetStaticMesh(nullptr);
		HeadMesh->SetVisibility(false);
		HeadMesh->SetHiddenInGame(true);
	}
	if (HeroSkelMesh)
	{
		HeroSkelMesh->SetSkeletalMeshAsset(nullptr);
		HeroSkelMesh->SetVisibility(false);
		HeroSkelMesh->SetHiddenInGame(true);
	}
}

// add by K2 (R1)
void ANightCoursePawn::ResolveHeroArt()
{
	const bool bSkeletalReady =
		bPreferSkeletalHero && HeroSkelMesh && HeroSkelMesh->GetSkeletalMeshAsset() != nullptr;

	if (HeroSkelMesh)
	{
		HeroSkelMesh->SetRelativeRotation(HeroMeshRotation);
		// 位置要带上 R2 的枢轴补偿，否则 ApplyConfiguredHeroVisual 刚算好的偏移会被这里抹掉
		HeroSkelMesh->SetRelativeLocation(
			HeroMeshOffset + HeroPivotOffsetCm * FMath::Max(0.01f, HeroScale));
		HeroSkelMesh->SetVisibility(bSkeletalReady);
		HeroSkelMesh->SetHiddenInGame(!bSkeletalReady);
	}

	if (!bSkeletalReady)
	{
		return;
	}

	for (UStaticMeshComponent* Mesh : { BodyMesh.Get(), HeadMesh.Get() })
	{
		if (Mesh)
		{
			Mesh->SetVisibility(false);
			Mesh->SetHiddenInGame(true);
		}
	}
}

// add by K2 (R1)
void ANightCoursePawn::PlayHeroAction(bool bAttack)
{
	UAnimSequence* Clip = bAttack ? AttackAnim : JumpAnim;
	if (!HeroSkelMesh || !Clip || HeroSkelMesh->GetSkeletalMeshAsset() == nullptr)
	{
		return;
	}

	bLastActionWasAttack = bAttack;
	HeroAnimPlayRate = FMath::Max(0.05f, bAttack ? AttackAnimRate : JumpAnimRate);
	// 不循环：跳/斩都是一次性动作，播完停在末帧等下一次输入（idle 动画到位前的权宜）
	HeroSkelMesh->PlayAnimation(Clip, false);
	HeroSkelMesh->SetPlayRate(HeroAnimPlayRate);
}

// add by K2 (R1)
float ANightCoursePawn::GetHeroActionRemainingSeconds() const
{
	if (!HeroSkelMesh || !HeroSkelMesh->GetSkeletalMeshAsset())
	{
		return 0.f;
	}

	UAnimSingleNodeInstance* Single = HeroSkelMesh->GetSingleNodeInstance();
	if (!Single || !Single->GetAnimationAsset())
	{
		return 0.f;
	}

	const float Rate = FMath::Max(0.05f, FMath::Abs(Single->GetPlayRate()));
	const float Left = Single->GetLength() - Single->GetCurrentTime();
	return FMath::Max(0.f, Left) / Rate;
}

void ANightCoursePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bTrackAdvancing)
	{
		return;
	}

	const FVector Current = GetActorLocation();
	const FVector Next = FMath::VInterpConstantTo(Current, AdvanceTargetLocation, DeltaSeconds, AdvanceSpeed);
	SetActorLocationAndRotation(Next, AdvanceTargetRotation);

	if (FVector::DistSquared(Next, AdvanceTargetLocation) <= 4.f)
	{
		SetActorLocationAndRotation(AdvanceTargetLocation, AdvanceTargetRotation);
		bTrackAdvancing = false;
	}
}

void ANightCoursePawn::ApplyAvatarColor(FLinearColor Color)
{
	auto Tint = [Color](UStaticMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}
		UMaterialInterface* Base = Mesh->GetMaterial(0);
		if (!Base)
		{
			return;
		}
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Base))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	};
	Tint(BodyMesh);
	Tint(HeadMesh);
}

void ANightCoursePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (FeelStub)
	{
		FeelStub->MappingContext = MappingContext;
		FeelStub->JumpAction = JumpAction;
		FeelStub->AttackAction = AttackAction;
	}

	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (MappingContext)
			{
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}
}

void ANightCoursePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (FeelStub)
	{
		FeelStub->MappingContext = MappingContext;
		FeelStub->JumpAction = JumpAction;
		FeelStub->AttackAction = AttackAction;
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ANightCoursePawn::OnJumpPressed);
		}
		if (AttackAction)
		{
			EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &ANightCoursePawn::OnAttackPressed);
		}
	}
}

// add by K2 (R1): 加速未走完的这一段石间移动，供负反应缓存命中时追赶时间轴
void ANightCoursePawn::ApplyAdvanceCatchUp(float RateMultiplier, float MaxCompressSeconds)
{
	if (!bTrackAdvancing || RateMultiplier <= 1.f || AdvanceSpeed <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float Remaining = FVector::Dist(GetActorLocation(), AdvanceTargetLocation);
	if (Remaining <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float BaseTime = Remaining / AdvanceSpeed;
	float TargetTime = BaseTime / RateMultiplier;
	if (MaxCompressSeconds > 0.f)
	{
		TargetTime = FMath::Max(TargetTime, BaseTime - MaxCompressSeconds);
	}
	if (TargetTime <= KINDA_SMALL_NUMBER || TargetTime >= BaseTime)
	{
		return;
	}

	// 只影响剩余这一段；下次 BeginTrackAdvance 会用配置速度重置
	AdvanceSpeed = Remaining / TargetTime;

	// add by K2 (R1): 表现时钟跟着判定走 —— 位移压缩多少倍，正在播的动作就加速多少倍
	if (HeroSkelMesh && HeroSkelMesh->GetSkeletalMeshAsset())
	{
		HeroAnimPlayRate *= BaseTime / TargetTime;
		HeroSkelMesh->SetPlayRate(HeroAnimPlayRate);
	}
}

void ANightCoursePawn::OnJumpPressed(const FInputActionValue& Value)
{
	(void)Value;
	if (FeelStub)
	{
		// add by K2 (R1): 移动中的输入不再丢弃，由 Feel 决定缓存 / 忽略 / 判定
		FeelStub->TryResolveInput_Implementation(ENightFeelInput::Jump);
	}
}

void ANightCoursePawn::OnAttackPressed(const FInputActionValue& Value)
{
	(void)Value;
	// 裁定 R-006：不再拦 bTrackAdvancing，移动中的输入要交给 Feel 缓存并加速衔接
	if (FeelStub)
	{
		FeelStub->TryResolveInput_Implementation(ENightFeelInput::Attack);
	}
}
#pragma endregion K2 moonyfli
