#include "Night/Course/NightCoursePawn.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h" //add by K2
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimInstance.h" //add by K2
#include "Animation/AnimMontage.h" //add by K2
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Course/NightCourseHUD.h"
#include "Night/Course/NightCourseDirector.h"
#include "InputCoreTypes.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h" //add by K2
#include "NiagaraSystem.h" //add by K2

#pragma region K2 moonyfli
namespace
{
	/**
	 * Returns the anim instance only when an animation blueprint is actually driving the mesh.
	 * The fallback single-node path has no slot node, so nothing can be played into it, and the
	 * caller has to fall back to component-level PlayAnimation.
	 *
	 * Which mode is live is decided by the AnimClass set on BP_NightCoursePawn's HeroSkelMesh,
	 * not by code, so the two paths have to coexist.
	 */
	UAnimInstance* GetSlotDrivenInstance(USkeletalMeshComponent* Mesh)
	{
		if (!Mesh || Mesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
		{
			return nullptr;
		}
		return Mesh->GetAnimInstance();
	}

	bool IsEditorPreviewWorld(const UObject* Object)
	{
		const UWorld* World = Object ? Object->GetWorld() : nullptr;
		return World && !World->IsGameWorld();
	}
}

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

#pragma region K2 moonyfli
	// Child BPs cannot assign Parent Socket on a component parented to this native mesh.
	KnifeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Knife"));
	KnifeMesh->SetupAttachment(HeroSkelMesh, TEXT("KnifeSocket"));
	KnifeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> KnifeDao(
		TEXT("/Game/Night/Character/Knife_dao.Knife_dao"));
	if (KnifeDao.Succeeded())
	{
		KnifeMesh->SetStaticMesh(KnifeDao.Object);
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SlashTrailFinder1(
		TEXT("/Game/Night/Course/VFX/ns/DG1.DG1"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SlashTrailFinder2(
		TEXT("/Game/Night/Course/VFX/ns/DG2.DG2"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SlashTrailFinder3(
		TEXT("/Game/Night/Course/VFX/ns/DG3.DG3"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SlashTrailFinder4(
		TEXT("/Game/Night/Course/VFX/ns/DG4.DG4"));
	SlashTrailByTier = {
		SlashTrailFinder1.Object,
		SlashTrailFinder2.Object,
		SlashTrailFinder3.Object,
		SlashTrailFinder4.Object};
	if (SlashTrailFinder1.Succeeded())
	{
		SlashTrailFX = SlashTrailFinder1.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HitImpactFinder1(
		TEXT("/Game/Night/Course/VFX/ns/phase1-shouji1.phase1-shouji1"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HitImpactFinder2(
		TEXT("/Game/Night/Course/VFX/ns/phase2-shouji2.phase2-shouji2"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HitImpactFinder3(
		TEXT("/Game/Night/Course/VFX/ns/phase3-shouji3.phase3-shouji3"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HitImpactFinder4(
		TEXT("/Game/Night/Course/VFX/ns/phase4-shouji4.phase4-shouji4"));
	HitImpactByTier = {
		HitImpactFinder1.Object,
		HitImpactFinder2.Object,
		HitImpactFinder3.Object,
		HitImpactFinder4.Object};
	if (HitImpactFinder1.Succeeded())
	{
		HitImpactFX = HitImpactFinder1.Object;
	}
#pragma endregion K2 moonyfli

	// 美术 0824 交付的跳跃（492ms，Takeoff 45ms / Land 331ms）。它替掉了 Jump_noknife2，
	// 后者留在资产里作为通知量法的参照。
	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpClip(
		TEXT("/Game/Night/Character/Anims/Jump.Jump"));
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

	// Do not synchronously load the camera-shake Blueprint while constructing the CDO.
	// The asset references a Level Sequence and can request Typed Elements before the
	// registry exists during commandlet cooking. It is resolved on the first real use below.
	// 刃心-style: behind + above the runner, looking forward down the lane.
	// These are only the native defaults now. BP_NightCoursePawn may override any of them on the
	// component and the override survives to runtime.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(ArtRoot);
	SpringArm->TargetArmLength = 620.f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	// Yaw only: the boom swings through turns, but hero pitch/roll must not tilt the frame.
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = false;
	SpringArm->SocketOffset = FVector(0.f, 55.f, 160.f);
	SpringArm->SetRelativeRotation(FRotator(-28.f, 0.f, 0.f));

	// add by K2 (R1) —— follow lag
	// Tick drives the hop with VInterpConstantTo, so the hero leaves and arrives at full speed,
	// and its yaw is assigned outright rather than interpolated. Lag on the boom is what turns
	// both of those into an eased follow. Speed is the knob: lower trails further and feels
	// heavier, higher tightens. Under a constant advance the boom settles roughly
	// AdvanceSpeed / CameraLagSpeed behind the hero, which at the default 1400 cm/s is ~117cm of
	// extra depth — read as the camera straining to keep up.
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 10.f;
	// A guard rather than a look. UpdateDesiredArmLocation clamps the lagged target to within this
	// distance of the real one, which also absorbs the course-start teleport in StartNight; without
	// it the boom would sweep across the level on the first frame of the night.
	SpringArm->CameraLagMaxDistance = 200.f;

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

void ANightCoursePawn::EnforceCameraInvariants()
{
	if (SpringArm)
	{
		SpringArm->bDoCollisionTest = false;
		SpringArm->bUsePawnControlRotation = false;
	}
	if (Camera)
	{
		Camera->bUsePawnControlRotation = false;
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

#pragma region K2 moonyfli
namespace NightCameraNotify
{
	// The names the notifies were authored under, measured by Tools/ProbeAnimNotifyPoints.py.
	// Renaming one on the animation without renaming it here silently drops that impulse.
	static const FName Takeoff(TEXT("Takeoff"));
	static const FName Land(TEXT("Land"));
	static const FName Contact(TEXT("Contact"));
}

void ANightCoursePawn::ScheduleCameraKicksForClip(const UAnimSequenceBase* Clip)
{
	PendingCameraKicks.Reset();
	if (!Clip)
	{
		return;
	}

	// Times come off the clip rather than a UPROPERTY so that retiming an animation moves the
	// camera with it. Storing them here as constants would desync the moment art re-exports.
	const float Rate = FMath::Max(0.05f, HeroAnimPlayRate);
	for (const FAnimNotifyEvent& Event : Clip->Notifies)
	{
		FNightCameraKick Kick;
		if (Event.NotifyName == NightCameraNotify::Takeoff)
		{
			Kick.FovDeg = TakeoffFovKickDeg;
		}
		else if (Event.NotifyName == NightCameraNotify::Land)
		{
			Kick.DipCm = LandBoomDipCm;
		}
		else if (Event.NotifyName == NightCameraNotify::Contact)
		{
			Kick.FovDeg = AttackFovKickDeg;
		}
		else
		{
			continue;
		}

		Kick.DelaySeconds = Event.GetTriggerTime() / Rate;
		PendingCameraKicks.Add(Kick);
	}
}

void ANightCoursePawn::UpdateCameraKicks(float DeltaSeconds)
{
	for (int32 Index = PendingCameraKicks.Num() - 1; Index >= 0; --Index)
	{
		FNightCameraKick& Kick = PendingCameraKicks[Index];
		Kick.DelaySeconds -= DeltaSeconds;
		if (Kick.DelaySeconds > 0.f)
		{
			continue;
		}

		LiveFovKickDeg += Kick.FovDeg;
		LiveBoomDipCm += Kick.DipCm;
		PendingCameraKicks.RemoveAtSwap(Index);
	}

	const float Recovery = FMath::Max(0.5f, CameraKickRecoverySpeed);
	LiveFovKickDeg = FMath::FInterpTo(LiveFovKickDeg, 0.f, DeltaSeconds, Recovery);
	LiveBoomDipCm = FMath::FInterpTo(LiveBoomDipCm, 0.f, DeltaSeconds, Recovery);

	// Idle frames deliberately leave the components alone: writing every tick would stomp any
	// runtime Set on FOV or socket offset from Blueprint. One final write settles them back.
	const bool bActive = !PendingCameraKicks.IsEmpty()
		|| FMath::Abs(LiveFovKickDeg) > 0.01f
		|| FMath::Abs(LiveBoomDipCm) > 0.01f;
	if (!bActive)
	{
		if (bCameraKickApplied)
		{
			if (Camera)
			{
				Camera->SetFieldOfView(BaseFieldOfView);
			}
			if (SpringArm)
			{
				SpringArm->SocketOffset = BaseSocketOffset;
			}
			bCameraKickApplied = false;
		}
		return;
	}

	if (Camera)
	{
		Camera->SetFieldOfView(BaseFieldOfView + LiveFovKickDeg);
	}
	if (SpringArm)
	{
		// Socket offset is applied after the lag maths, so the dip lands crisply instead of being
		// damped away like a TargetOffset change would be.
		SpringArm->SocketOffset = BaseSocketOffset + FVector(0.f, 0.f, LiveBoomDipCm);
	}
	bCameraKickApplied = true;
}
#pragma endregion K2 moonyfli

void ANightCoursePawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AttachKnifeToHand();
}

void ANightCoursePawn::AttachKnifeToHand()
{
	if (!KnifeMesh || !HeroSkelMesh)
	{
		return;
	}

	static const FName SocketName(TEXT("KnifeSocket"));
	const FName Socket = HeroSkelMesh->DoesSocketExist(SocketName) ? SocketName : NAME_None;
	KnifeMesh->AttachToComponent(
		HeroSkelMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		Socket);
}

#pragma region K2 moonyfli
int32 ANightCoursePawn::ResolveAttackVFXTier() const
{
	const int32 Combo = FeelStub ? FeelStub->Combo : 0;
	if (Combo >= SimulatedVFXTier4Combo)
	{
		return 3;
	}
	if (Combo >= SimulatedVFXTier3Combo)
	{
		return 2;
	}
	if (Combo >= SimulatedVFXTier2Combo)
	{
		return 1;
	}
	return 0;
}

UNiagaraSystem* ANightCoursePawn::ResolveSlashTrailFX() const
{
	const int32 Tier = ResolveAttackVFXTier();
	if (SlashTrailByTier.IsValidIndex(Tier) && SlashTrailByTier[Tier])
	{
		return SlashTrailByTier[Tier];
	}
	return SlashTrailFX;
}

UNiagaraSystem* ANightCoursePawn::ResolveHitImpactFX() const
{
	const int32 Tier = ResolveAttackVFXTier();
	if (HitImpactByTier.IsValidIndex(Tier) && HitImpactByTier[Tier])
	{
		return HitImpactByTier[Tier];
	}
	return HitImpactFX;
}

void ANightCoursePawn::PlayAttackVFX(const FVector& HitWorldLocation)
{
	if (UNiagaraSystem* TrailFX = ResolveSlashTrailFX())
	{
		if (KnifeMesh)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				TrailFX,
				KnifeMesh,
				NAME_None,
				FVector::ZeroVector,
				SlashTrailRotation,
				EAttachLocation::SnapToTarget,
				true);
		}
		else if (UWorld* World = GetWorld())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				TrailFX,
				GetActorLocation(),
				SlashTrailRotation);
		}
	}

	if (UNiagaraSystem* ImpactFX = ResolveHitImpactFX())
	{
		if (UWorld* World = GetWorld())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				ImpactFX,
				HitWorldLocation);
		}
	}
}
#pragma endregion K2 moonyfli

void ANightCoursePawn::BeginPlay()
{
	Super::BeginPlay();
	EnforceCameraInvariants();

	// add by K2 (R1): the components own the framing, so the impulse baseline is read from them
	// rather than from a constant here.
	if (Camera)
	{
		BaseFieldOfView = Camera->FieldOfView;
	}
	if (SpringArm)
	{
		BaseSocketOffset = SpringArm->SocketOffset;
	}

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

void ANightCoursePawn::BindCourseDirector(UNightCourseDirector* InDirector)
{
	CourseDirector = InDirector;
}

UNightCourseDirector* ANightCoursePawn::GetCourseDirector() const
{
	return CourseDirector;
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

void ANightCoursePawn::BeginTrackAdvance(const FVector& WorldLocation, const FRotator& WorldRotation, float SpeedCmPerSec, bool bUseRawSpeed)
{
	AdvanceTargetLocation = WorldLocation;
	AdvanceTargetRotation = WorldRotation;
	AdvanceSpeed = FMath::Max(100.f, SpeedCmPerSec);

	// add by K2 (R1): 动画驱动模式下丢掉 Director 传来的速度，改由锚点反推——
	// 这样位移在动作的关键帧（落地 / 接触）那一刻结束，倍率一改两者一起缩放。
	// 岔口过渡传 bUseRawSpeed=true：保留 Director 给的 ForkTransitionAdvanceSpeed，
	// 避免大间距被 AnchorSeconds 压缩成瞬移。
	if (!bUseRawSpeed && bAnimDrivenAdvance)
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
 * Spreads the configured hero material across every slot of the mesh. Setting slot 0
 * alone would leave any remaining slot on the import default, and tinting slot 0 alone
 * would split the hero into two colours, so the MID is built per slot as well.
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
	HeroStaticRuntimeBaseLocation = BodyMesh->GetRelativeLocation();
	bHeroStaticRuntimeBaseLocationCached = true;
	HeadMesh->SetVisibility(false);
	HeadMesh->SetHiddenInGame(true);
	ApplyHeroMaterial(BodyMesh, MaterialOverride); //add by K2
	ApplyHeroZCompensation(IsEditorPreviewWorld(this));
}

void ANightCoursePawn::ApplyHeroZCompensation(bool bPreview)
{
	const bool bApplyCompensation =
		!bPreview || bApplyHeroZCompensationInPreview;
	const float AppliedOffset = bApplyCompensation
		? HeroZCompensationCm
		: 0.f;

	if (bPreferSkeletalHero
		&& HeroSkelMesh
		&& HeroSkelMesh->GetSkeletalMeshAsset())
	{
		if (!bHeroRuntimeBaseLocationCached)
		{
			HeroRuntimeBaseLocation = HeroSkelMesh->GetRelativeLocation();
			bHeroRuntimeBaseLocationCached = true;
		}
		FVector Location = HeroRuntimeBaseLocation;
		Location.Z += AppliedOffset;
		HeroSkelMesh->SetRelativeLocation(Location);
		return;
	}

	if (!bPreferSkeletalHero
		&& BodyMesh
		&& BodyMesh->GetStaticMesh())
	{
		if (!bHeroStaticRuntimeBaseLocationCached)
		{
			HeroStaticRuntimeBaseLocation = BodyMesh->GetRelativeLocation();
			bHeroStaticRuntimeBaseLocationCached = true;
		}
		FVector Location = HeroStaticRuntimeBaseLocation;
		Location.Z += AppliedOffset;
		BodyMesh->SetRelativeLocation(Location);
	}
}

void ANightCoursePawn::ApplyConfiguredHeroVisual()
{
	if (HeroSkeletalMesh && HeroSkelMesh)
	{
		HeroSkelMesh->SetSkeletalMeshAsset(HeroSkeletalMesh);
		AttachKnifeToHand();
		HeroSkelMesh->SetRelativeLocation(
			HeroMeshOffset + HeroPivotOffsetCm * HeroScale);
		HeroSkelMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, HeroScale)));
		// add by K2 (R1): the skeletal hero keeps whatever materials the mesh asset carries.
		// HeroMaterial is baked against the static kat atlas, and the two meshes have
		// different UV layouts, so forcing it here would sample the wrong atlas.

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
	if (KnifeMesh)
	{
		KnifeMesh->SetVisibility(false);
		KnifeMesh->SetHiddenInGame(true);
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
		HeroRuntimeBaseLocation =
			HeroMeshOffset + HeroPivotOffsetCm * FMath::Max(0.01f, HeroScale);
		bHeroRuntimeBaseLocationCached = true;
		ApplyHeroZCompensation(IsEditorPreviewWorld(this));
		HeroSkelMesh->SetVisibility(bSkeletalReady);
		HeroSkelMesh->SetHiddenInGame(!bSkeletalReady);
	}
	if (KnifeMesh)
	{
		if (bSkeletalReady)
		{
			AttachKnifeToHand();
		}
		KnifeMesh->SetVisibility(bSkeletalReady);
		KnifeMesh->SetHiddenInGame(!bSkeletalReady);
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
	ScheduleCameraKicksForClip(Clip); //add by K2

	if (UAnimInstance* Instance = GetSlotDrivenInstance(HeroSkelMesh))
	{
		// The montage defaults (250ms both ends) are far too long for clips of 370-490ms: they
		// would still be fading the slash in at its 179ms contact frame and would already be
		// fading it out before reaching it. Hence the tunables, clamped to stay clear of the
		// anchor notifies. Playing the sequence as a dynamic montage keeps the sequence's own
		// notifies, so Contact / Land stay where they were measured, and it avoids having to
		// maintain montage assets for two one-shot clips.
		const float ClipLength = Clip->GetPlayLength();
		const float AnchorSeconds =
			FMath::Max(10.f, bAttack ? AttackAnchorMs : JumpAnchorMs) * 0.001f;
		// Never let the fade start before the anchor, whatever the clip is swapped to.
		const float BlendOut = FMath::Clamp(
			ActionBlendOutSeconds, 0.f, FMath::Max(0.f, ClipLength - AnchorSeconds));
		const float BlendIn = FMath::Clamp(ActionBlendInSeconds, 0.f, AnchorSeconds);

		Instance->PlaySlotAnimationAsDynamicMontage(
			Clip, TEXT("DefaultSlot"), BlendIn, BlendOut, HeroAnimPlayRate, 1, -1.f, 0.f);
		return;
	}

	// 不循环：跳/斩都是一次性动作，播完停在末帧等下一次输入（idle 动画到位前的权宜）
	HeroSkelMesh->PlayAnimation(Clip, false);
	HeroSkelMesh->SetPlayRate(HeroAnimPlayRate);
}

// add by K2 (R1)
void ANightCoursePawn::PlayFailCameraShake()
{
	if (!FailCameraShake)
	{
		FailCameraShake = LoadClass<UCameraShakeBase>(
			nullptr,
			TEXT("/Game/Night/Course/Camera/CS_CameraShake_Return.CS_CameraShake_Return_C"));
		if (!FailCameraShake)
		{
			return;
		}
	}
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	PC->ClientStartCameraShake(FailCameraShake, FMath::Max(0.f, FailCameraShakeScale));
}

// add by K2 (R1)
float ANightCoursePawn::GetHeroActionRemainingSeconds() const
{
	if (!HeroSkelMesh || !HeroSkelMesh->GetSkeletalMeshAsset())
	{
		return 0.f;
	}

	if (UAnimInstance* Instance = GetSlotDrivenInstance(HeroSkelMesh))
	{
		UAnimMontage* Active = Instance->GetCurrentActiveMontage();
		if (!Active)
		{
			return 0.f;
		}

		const float MontageRate = FMath::Max(0.05f, FMath::Abs(Instance->Montage_GetPlayRate(Active)));
		const float MontageLeft = Active->GetPlayLength() - Instance->Montage_GetPosition(Active);
		return FMath::Max(0.f, MontageLeft) / MontageRate;
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

	// Runs unconditionally: the slash plays while standing still, so gating this on bTrackAdvancing
	// would drop the contact impulse entirely and leave a half-decayed kick frozen on screen.
	UpdateCameraKicks(DeltaSeconds); //add by K2

	if (!bTrackAdvancing)
	{
		return;
	}

	const FVector Current = GetActorLocation();
	const FVector Next = FMath::VInterpConstantTo(Current, AdvanceTargetLocation, DeltaSeconds, AdvanceSpeed);
	// 朝向用恒定角速度平滑插值，避免进入分支时朝向瞬间弹到 TrackForward 造成的 POP。
	const FRotator NextRot = FMath::RInterpConstantTo(GetActorRotation(), AdvanceTargetRotation, DeltaSeconds, 540.f);
	SetActorLocationAndRotation(Next, NextRot);

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
		// add by K2 (R1): packaged Android otherwise loads DefaultVirtualJoysticks from
		// DefaultInput.ini, which would put a stick on a Jump+Attack-only game.
		PC->ActivateTouchInterface(nullptr);

		// Default PC captures the mouse (DefaultViewportMouseCaptureMode=CapturePermanently),
		// which hides the cursor. The HUD pads are clicked/tapped, so the pointer has to be visible
		// in PIE and on device when a mouse is attached.
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);

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

	// add by K2 (R1): pads are Canvas rects, not widgets, so Enhanced Input cannot hit-test them.
	// Mouse (PIE) and Touch (device) share the same classify path. Q/E stay on the mapping context.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ANightCoursePawn::OnHudPointerPressed);
	
PlayerInputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &ANightCoursePawn::OnAnyKeyPressed);
	PlayerInputComponent->BindTouch(IE_Pressed, this, &ANightCoursePawn::OnHudTouchPressed);
}

void ANightCoursePawn::OnAnyKeyPressed(FKey Key)
{
	
(void)Key;
	// Q/E and the mouse button also drive gameplay handlers; let those handlers dismiss first
	// so the opening input is not accidentally consumed as a jump, attack, or pad press.
	if (Key == EKeys::Q || Key == EKeys::E || Key == EKeys::LeftMouseButton)
	{
		return;
	}
	
if (APlayerController* PC = Cast<APlayerController>(GetController()))
	
{
	
	
if (ANightCourseHUD* NightHUD = Cast<ANightCourseHUD>(PC->GetHUD()))
	
	
{
	
	
	
NightHUD->DismissStartScreenIfVisible();
	
	
}
	
}

}


void ANightCoursePawn::OnHudPointerPressed()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	float ScreenX = 0.f;
	float ScreenY = 0.f;
	if (!PC->GetMousePosition(ScreenX, ScreenY))
	{
		return;
	}
	TryResolveHudPointer(ScreenX, ScreenY);
}

void ANightCoursePawn::OnHudTouchPressed(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	(void)FingerIndex;
	TryResolveHudPointer(Location.X, Location.Y);
}

void ANightCoursePawn::TryResolveHudPointer(float ScreenX, float ScreenY)
{
	if (!FeelStub && !CourseDirector)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	int32 ViewX = 0;
	int32 ViewY = 0;
	PC->GetViewportSize(ViewX, ViewY);

	ANightCourseHUD* NightHUD = Cast<ANightCourseHUD>(PC->GetHUD());
	
if (NightHUD && NightHUD->DismissStartScreenIfVisible())
	
{
	
	
return;
	
}
	ENightFeelInput Input = ENightFeelInput::Jump;
	if (!NightHUD
		|| !NightHUD->HitTestActionButtons(
			ScreenX,
			ScreenY,
			static_cast<float>(ViewX),
			static_cast<float>(ViewY),
			Input))
	{
		return;
	}
	if (CourseDirector && CourseDirector->IsForkChoiceActive())
	{
		if (Input == ENightFeelInput::Jump)
		{
			CourseDirector->ChooseForkLeft();
		}
		else
		{
			CourseDirector->ChooseForkRight();
		}
		return;
	}
	if (!FeelStub)
	{
		return;
	}
	FeelStub->TryResolveInput_Implementation(Input);
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
		const float CatchUpRate = BaseTime / TargetTime;
		HeroAnimPlayRate *= CatchUpRate;

		// Queued impulses hold delays in seconds, so speeding the clip up has to pull them in by
		// the same factor or the landing dip would fire after the hero has already landed.
		for (FNightCameraKick& Kick : PendingCameraKicks)
		{
			Kick.DelaySeconds /= CatchUpRate;
		}

		if (UAnimInstance* Instance = GetSlotDrivenInstance(HeroSkelMesh))
		{
			if (UAnimMontage* Active = Instance->GetCurrentActiveMontage())
			{
				Instance->Montage_SetPlayRate(Active, HeroAnimPlayRate);
			}
		}
		else
		{
			HeroSkelMesh->SetPlayRate(HeroAnimPlayRate);
		}
	}
}

void ANightCoursePawn::OnJumpPressed(const FInputActionValue& Value)
{
	(void)Value;
	
if (APlayerController* PC = Cast<APlayerController>(GetController()))
	
{
	
	
if (ANightCourseHUD* NightHUD = Cast<ANightCourseHUD>(PC->GetHUD()))
	
	
{
	
	
	
if (NightHUD->DismissStartScreenIfVisible())
	
	
	
{
	
	
	
	
return;
	
	
	
}
	
	
}
	
}
	if (CourseDirector && CourseDirector->IsForkChoiceActive())
	{
		CourseDirector->ChooseForkLeft();
		return;
	}
	if (FeelStub)
	{
		// add by K2 (R1): 移动中的输入不再丢弃，由 Feel 决定缓存 / 忽略 / 判定
		FeelStub->TryResolveInput_Implementation(ENightFeelInput::Jump);
	}
}

void ANightCoursePawn::OnAttackPressed(const FInputActionValue& Value)
{
	(void)Value;
	
if (APlayerController* PC = Cast<APlayerController>(GetController()))
	
{
	
	
if (ANightCourseHUD* NightHUD = Cast<ANightCourseHUD>(PC->GetHUD()))
	
	
{
	
	
	
if (NightHUD->DismissStartScreenIfVisible())
	
	
	
{
	
	
	
	
return;
	
	
	
}
	
	
}
	
}

	// LeftMouseButton is still mapped to IA_NightAttack in IMC_NightCourse. The HUD pads own the
	// mouse now, so swallow that chord here and let OnHudPointerPressed classify the click;
	// otherwise tapping Jump would also fire Attack on the same frame.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			return;
		}
	}

	if (CourseDirector && CourseDirector->IsForkChoiceActive())
	{
		CourseDirector->ChooseForkRight();
		return;
	}
	if (FeelStub)
	{
		FeelStub->TryResolveInput_Implementation(ENightFeelInput::Attack);
	}
}
#pragma endregion K2 moonyfli
