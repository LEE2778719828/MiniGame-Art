#include "Night/Course/NightCoursePawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HeroMesh(
		TEXT("/Game/Night/Course/Art/Hero/kat.kat"));
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
		HeadMesh->SetStaticMesh(CubeMesh.Object);
	}
	if (HeroMesh.Succeeded())
	{
		bUsingHeroArt = true;
		BodyMesh->SetStaticMesh(HeroMesh.Object);
		BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
		BodyMesh->SetRelativeScale3D(FVector(0.75f));
		HeadMesh->SetVisibility(false);
		HeadMesh->SetHiddenInGame(true);
	}

	// add by K2 (R1): 骨骼主角。单节点播放，不挂 AnimBP —— 这一层只要「按一下动一下」，
	// 状态机等 idle 动画到位后再说。
	HeroSkelMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeroSkelMesh"));
	HeroSkelMesh->SetupAttachment(ArtRoot);
	HeroSkelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeroSkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	HeroSkelMesh->SetRelativeRotation(HeroMeshRotation);
	HeroSkelMesh->SetRelativeLocation(HeroMeshOffset);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> HeroSkeletal(
		TEXT("/Game/Night/Character/SK_Hero.SK_Hero"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpClip(
		TEXT("/Game/Night/Character/Anims/Jump_noknife2.Jump_noknife2"));
	// 常速斩击尚未到位（源文件的动画取不出来，见任务清单第 22 轮），暂用 200ms 的快版顶替
	static ConstructorHelpers::FObjectFinder<UAnimSequence> AttackClip(
		TEXT("/Game/Night/Character/Anims/Slash_fast_Armature_Armature_kan.Slash_fast_Armature_Armature_kan"));

	if (HeroSkeletal.Succeeded())
	{
		HeroSkelMesh->SetSkeletalMeshAsset(HeroSkeletal.Object);
	}
	if (JumpClip.Succeeded())
	{
		JumpAnim = JumpClip.Object;
	}
	if (AttackClip.Succeeded())
	{
		AttackAnim = AttackClip.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitMat(TEXT("/Game/Night/Course/Materials/M_NightUnlitColor.M_NightUnlitColor"));
	if (UnlitMat.Succeeded())
	{
		BodyMesh->SetMaterial(0, UnlitMat.Object);
		if (!bUsingHeroArt)
		{
			HeadMesh->SetMaterial(0, UnlitMat.Object);
		}
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
	ApplyAvatarColor(FLinearColor(0.95f, 0.9f, 0.75f));
	ResolveHeroArt();
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
	bTrackAdvancing = true;
}

void ANightCoursePawn::ApplyHeroMesh(UStaticMesh* Mesh)
{
	if (!BodyMesh || !Mesh)
	{
		return;
	}

	bUsingHeroArt = true;
	BodyMesh->SetStaticMesh(Mesh);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	BodyMesh->SetRelativeScale3D(FVector(0.75f));
	HeadMesh->SetVisibility(false);
	HeadMesh->SetHiddenInGame(true);
	if (UMaterialInterface* NightMat = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Night/Course/Materials/M_NightUnlitColor.M_NightUnlitColor")))
	{
		BodyMesh->SetMaterial(0, NightMat);
		if (UMaterialInstanceDynamic* MID =
			BodyMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, NightMat))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.72f, 0.28f));
		}
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
		HeroSkelMesh->SetRelativeLocation(HeroMeshOffset);
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

	HeroAnimPlayRate = 1.f;
	// 不循环：跳/斩都是一次性动作，播完停在末帧等下一次输入（idle 动画到位前的权宜）
	HeroSkelMesh->PlayAnimation(Clip, false);
	HeroSkelMesh->SetPlayRate(HeroAnimPlayRate);
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
