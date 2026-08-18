#include "Night/Course/NightCoursePawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
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
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
		HeadMesh->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitMat(TEXT("/Game/Night/Course/Materials/M_NightUnlitColor.M_NightUnlitColor"));
	if (UnlitMat.Succeeded())
	{
		BodyMesh->SetMaterial(0, UnlitMat.Object);
		HeadMesh->SetMaterial(0, UnlitMat.Object);
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

void ANightCoursePawn::OnJumpPressed(const FInputActionValue& Value)
{
	(void)Value;
	if (bTrackAdvancing)
	{
		return;
	}
	if (CourseDirector && CourseDirector->IsForkChoiceActive())
	{
		CourseDirector->ChooseForkLeft();
		return;
	}
	if (FeelStub)
	{
		//add by K2
		FeelStub->TryResolveInput_Implementation(ENightFeelInput::Jump);
	}
}

void ANightCoursePawn::OnAttackPressed(const FInputActionValue& Value)
{
	(void)Value;
	if (bTrackAdvancing)
	{
		return;
	}
	if (CourseDirector && CourseDirector->IsForkChoiceActive())
	{
		CourseDirector->ChooseForkRight();
		return;
	}
	if (FeelStub)
	{
		//add by K2
		FeelStub->TryResolveInput_Implementation(ENightFeelInput::Attack);
	}
}
#pragma endregion K2 moonyfli
