#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseAtomRouteData.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h" //add by K2
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#pragma region K2 moonyfli
namespace NightCourseStage_Private
{
	static void TintMesh(UStaticMeshComponent* Mesh, UMaterialInterface* BaseMat, const FLinearColor& Color)
	{
		if (!Mesh || !BaseMat)
		{
			return;
		}
		Mesh->SetMaterial(0, BaseMat);
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMat))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	static AStaticMeshActor* SpawnEditorPreviewMesh(
		UWorld* World,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FTransform& Transform,
		const FString& Label)
	{
		if (!World || !Mesh)
		{
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(),
			Transform,
			Params);
		if (!Actor)
		{
			return nullptr;
		}

		Actor->SetActorLabel(Label);
		Actor->SetIsTemporarilyHiddenInEditor(false);
		if (UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent())
		{
			MeshComponent->SetStaticMesh(Mesh);
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (Material)
			{
				for (int32 MaterialIndex = 0;
					MaterialIndex < MeshComponent->GetNumMaterials();
					++MaterialIndex)
				{
					MeshComponent->SetMaterial(MaterialIndex, Material);
				}
			}
		}
		return Actor;
	}

	static ANightBridgeSegmentActor* SpawnEditorPreviewBridge(
		UWorld* World,
		UClass* BridgeClass,
		const FNightBridgeSpec& Spec,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& PivotOffsetCm,
		float GlobalScaleMultiplier,
		const FString& Label)
	{
		if (!World || !BridgeClass)
		{
			return nullptr;
		}

		ANightBridgeSegmentActor* Actor =
			World->SpawnActorDeferred<ANightBridgeSegmentActor>(
				BridgeClass,
				FTransform::Identity,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor)
		{
			return nullptr;
		}

		Actor->SetFlags(RF_Transient);
		Actor->FinishSpawning(FTransform::Identity);
		Actor->SetActorLabel(Label);
		Actor->SetupBridge(
			Spec,
			Mesh,
			Material,
			PivotOffsetCm,
			GlobalScaleMultiplier);
		return Actor;
	}

	static AActor* SpawnEditorPreviewVisual(
		UWorld* World,
		UClass* VisualClass,
		const FTransform& Transform,
		const FNightStoneSpec* StoneSpec,
		int32 StoneIndex,
		const FNightBridgeSpec* BridgeSpec,
		const FString& Label)
	{
		if (!World || !VisualClass)
		{
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(VisualClass, Transform, Params);
		if (!Actor)
		{
			return nullptr;
		}

#if WITH_EDITOR
		Actor->SetActorLabel(Label);
#endif
		Actor->SetActorTransform(Transform);
		Actor->SetActorEnableCollision(false);

		if (StoneSpec)
		{
			if (ANightCourseStoneActor* StoneActor = Cast<ANightCourseStoneActor>(Actor))
			{
				StoneActor->SetupStone(StoneIndex, *StoneSpec);
			}
			else if (ANightBridgeSegmentActor* BridgeActor = Cast<ANightBridgeSegmentActor>(Actor))
			{
				FNightBridgeSpec PadSpec;
				PadSpec.FromStoneIndex = StoneIndex;
				PadSpec.ToStoneIndex = StoneIndex;
				PadSpec.WorldLocation = Transform.GetLocation();
				PadSpec.YawDeg = Transform.GetRotation().Rotator().Yaw;
				PadSpec.LengthScale = 1.f;
				BridgeActor->SetupBridge(
					PadSpec,
					nullptr,
					nullptr,
					FVector::ZeroVector,
					1.f);
			}
		}
		else if (BridgeSpec)
		{
			if (ANightBridgeSegmentActor* BridgeActor = Cast<ANightBridgeSegmentActor>(Actor))
			{
				BridgeActor->SetupBridge(
					*BridgeSpec,
					nullptr,
					nullptr,
					FVector::ZeroVector,
					1.f);
			}
		}
		return Actor;
	}

	static AStaticMeshActor* SpawnBox(
		UWorld* World,
		UStaticMesh* Cube,
		UMaterialInterface* Mat,
		const FVector& Location,
		const FVector& Scale,
		const FLinearColor& Color,
		const FName& Label)
	{
		if (!World || !Cube)
		{
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator, Params);
		if (!Actor)
		{
			return nullptr;
		}

#if WITH_EDITOR
		Actor->SetActorLabel(Label.ToString());
#endif
		Actor->Tags.Add(Label);
		Actor->SetActorScale3D(Scale);
		if (UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent())
		{
			SMC->SetStaticMesh(Cube);
			SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			TintMesh(SMC, Mat, Color);
		}
		return Actor;
	}
}

ANightCourseHost::ANightCourseHost()
{
	PrimaryActorTick.bCanEverTick = false;
	Director = CreateDefaultSubobject<UNightCourseDirector>(TEXT("Director"));
	NightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("NightFog"));
	NightFog->FogDensity = 0.003f;
	NightFog->FogHeightFalloff = 0.18f;
	NightFog->FogInscatteringLuminance = FLinearColor(0.08f, 0.14f, 0.3f);
	NightFog->DirectionalInscatteringExponent = 4.f;
	NightFog->DirectionalInscatteringStartDistance = 0.f;

	PreviewBridgeA = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewBridgeA"));
	PreviewBridgeA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewBridgeA->SetHiddenInGame(true);
	PreviewBridgeB = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewBridgeB"));
	PreviewBridgeB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewBridgeB->SetHiddenInGame(true);
	PreviewFoeM01 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewFoeM01"));
	PreviewFoeM01->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewFoeM01->SetHiddenInGame(true);
	PreviewFoeM02 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewFoeM02"));
	PreviewFoeM02->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewFoeM02->SetHiddenInGame(true);
	PreviewFoeM03 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewFoeM03"));
	PreviewFoeM03->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewFoeM03->SetHiddenInGame(true);
	PreviewFoeM04 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewFoeM04"));
	PreviewFoeM04->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewFoeM04->SetHiddenInGame(true);
	PreviewFoeM05 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewFoeM05"));
	PreviewFoeM05->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewFoeM05->SetHiddenInGame(true);
	PreviewKeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("PreviewKeyLight"));
	PreviewKeyLight->SetRelativeRotation(FRotator(-35.f, -35.f, 0.f));
	PreviewKeyLight->Intensity = 5000.f;
	PreviewKeyLight->LightColor = FColor(180, 210, 255);
	PreviewKeyLight->SetHiddenInGame(false);

	LayoutBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("LayoutBounds"));
	LayoutBounds->SetBoxExtent(FVector(10000.f, 10000.f, 3000.f));
	LayoutBounds->ShapeColor = FColor(80, 180, 255, 45);
	LayoutBounds->SetBoxExtent(LayoutBoundsExtent);
	LayoutBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LayoutBounds->SetHiddenInGame(true);
	LayoutBounds->SetVisibility(true);

}

void ANightCourseHost::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (LayoutBounds)
	{
		LayoutBounds->SetBoxExtent(LayoutBoundsExtent);
	}
	if (Director)
	{
		Director->SetLayoutBoundsComponent(LayoutBounds, bEnforceLayoutBounds);
	}
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		RebuildEditorPreview();
	}
}

void ANightCourseHost::RebuildEditorPreview()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Preview] Rebuild begin Host='%s' Config='%s' Director='%s' enforceBounds=%d."),
		*GetNameSafe(this),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		*GetNameSafe(Director),
		bEnforceLayoutBounds ? 1 : 0);

	if (!PreviewBridgeA || !PreviewBridgeB
		|| !PreviewFoeM01 || !PreviewFoeM02 || !PreviewFoeM03
		|| !PreviewFoeM04 || !PreviewFoeM05)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Preview] Rebuild aborted: one or more preview instanced components are missing on Host='%s'."),
			*GetNameSafe(this));
		return;
	}

	PreviewBridgeA->ClearInstances();
	PreviewBridgeB->ClearInstances();
	PreviewFoeM01->ClearInstances();
	PreviewFoeM02->ClearInstances();
	PreviewFoeM03->ClearInstances();
	PreviewFoeM04->ClearInstances();
	PreviewFoeM05->ClearInstances();
	PreviewBridgeA->SetVisibility(false);
	PreviewBridgeB->SetVisibility(false);
	PreviewFoeM01->SetVisibility(false);
	PreviewFoeM02->SetVisibility(false);
	PreviewFoeM03->SetVisibility(false);
	PreviewFoeM04->SetVisibility(false);
	PreviewFoeM05->SetVisibility(false);
	for (AActor* Actor : EditorPreviewMeshActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	EditorPreviewMeshActors.Reset();
	if (!Config || !Director)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Preview] Rebuild aborted: Config='%s' Director='%s'. Assign DA_Course and ensure the Director component exists."),
			Config ? *Config->GetPathName() : TEXT("<null>"),
			*GetNameSafe(Director));
		return;
	}

	Director->Config = Config;
	Director->SetLayoutBoundsComponent(LayoutBounds, bEnforceLayoutBounds);
	Director->ResetCourse();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Preview] Building Config='%s' Rule='%s' AtomLibrary='%s' RouteRules='%s'."),
		*Config->GetPathName(),
		Config->CourseRuleData ? *Config->CourseRuleData->GetPathName() : TEXT("<null>"),
		Config->AtomRoute ? *Config->AtomRoute->GetPathName() : TEXT("<null>"),
		Config->RouteRules ? *Config->RouteRules->GetPathName() : TEXT("<null>"));
	TArray<FNightStoneSpec> PreviewStones;
	TArray<FNightBeatSpec> PreviewBeats;
	TArray<FNightBridgeSpec> PreviewBridges;
	TArray<FNightAtomVisualBinding> VisualBindings;
	if (!Director->BuildCourseForPreview(
		PreviewStones,
		PreviewBeats,
		PreviewBridges,
		VisualBindings))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Preview] Build failed for Config='%s'; no preview actors will be created."),
			*Config->GetPathName());
		return;
	}

	TSet<int32> ArtBridgeIndexes;
	TSet<int32> ArtStoneIndexes;
	for (const FNightAtomVisualBinding& Binding : VisualBindings)
	{
		TSubclassOf<AActor> VisualClass = Binding.VisualPrefabClass;
		if (!Binding.bIsBridge
			&& PreviewStones.IsValidIndex(Binding.StoneIndex)
			&& PreviewStones[Binding.StoneIndex].bHasFoe
			&& Binding.AlternateVisualPrefabClass)
		{
			VisualClass = Binding.AlternateVisualPrefabClass;
		}
		if (Binding.bIsBridge)
		{
			ArtBridgeIndexes.Add(Binding.BridgeIndex);
		}
		else if (VisualClass)
		{
			ArtStoneIndexes.Add(Binding.StoneIndex);
		}
		if (!VisualClass)
		{
			// A landing point's normal character preview is editor-only. If
			// this point is not an Enemy beat, there is no runtime/Host
			// landing actor to spawn.
			continue;
		}
		if (!Binding.bIsBridge
			&& VisualClass->IsChildOf(ANightBridgeSegmentActor::StaticClass()))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse] LandingPoint preview binding %d resolves to a Bridge BP; configure a character/enemy BP."),
				Binding.StoneIndex);
			continue;
		}
		AActor* Actor = nullptr;
		if (Binding.bIsBridge && PreviewBridges.IsValidIndex(Binding.BridgeIndex))
		{
			Actor = NightCourseStage_Private::SpawnEditorPreviewVisual(
				GetWorld(),
				VisualClass.Get(),
				Binding.LocalTransform,
				nullptr,
				INDEX_NONE,
				&PreviewBridges[Binding.BridgeIndex],
				FString::Printf(
					TEXT("EditorPreview_Atom_%s_Bridge_%d"),
					Binding.AtomKey.IsEmpty() ? TEXT("Legacy") : *Binding.AtomKey,
					Binding.BridgeIndex));
		}
		else if (!Binding.bIsBridge && PreviewStones.IsValidIndex(Binding.StoneIndex))
		{
			Actor = NightCourseStage_Private::SpawnEditorPreviewVisual(
				GetWorld(),
				VisualClass.Get(),
				Binding.LocalTransform,
				&PreviewStones[Binding.StoneIndex],
				Binding.StoneIndex,
				nullptr,
				FString::Printf(
					TEXT("EditorPreview_Atom_%s_Landing_%d"),
					Binding.AtomKey.IsEmpty() ? TEXT("Legacy") : *Binding.AtomKey,
					Binding.StoneIndex));
		}
		if (Actor)
		{
			EditorPreviewMeshActors.Add(Actor);
		}
	}
	for (const FNightBridgeSpec& Bridge : PreviewBridges)
	{
		if (ArtBridgeIndexes.Contains(&Bridge - PreviewBridges.GetData()))
		{
			continue;
		}
		// Atom bridge visuals are authored by the Atom BP. A missing visual
		// intentionally leaves only a native compatibility actor.
		UClass* BridgeClass = ANightBridgeSegmentActor::StaticClass();
		const FString Label = FString::Printf(
			TEXT("EditorPreview_Bridge_%s_%d"),
			Bridge.MeshVariant == 0 ? TEXT("A") : TEXT("B"),
			Bridge.FromStoneIndex);
		if (ANightBridgeSegmentActor* Actor =
			NightCourseStage_Private::SpawnEditorPreviewBridge(
			GetWorld(), BridgeClass, Bridge, nullptr, nullptr,
			FVector::ZeroVector, 1.f, Label))
		{
			EditorPreviewMeshActors.Add(Actor);
		}
	}
	for (int32 StoneIndex = 0; StoneIndex < PreviewStones.Num(); ++StoneIndex)
	{
		const FNightStoneSpec& Stone = PreviewStones[StoneIndex];
		if (ArtStoneIndexes.Contains(StoneIndex))
		{
			continue;
		}
		if (!Stone.bHasFoe)
		{
			continue;
		}
		// Enemy visuals are authored by the Atom LandingPoint. A missing
		// visual intentionally produces only the native gameplay carrier.
		UClass* FoeClass = ANightCourseStoneActor::StaticClass();
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ANightCourseStoneActor* Actor = GetWorld()->SpawnActor<ANightCourseStoneActor>(
			FoeClass, Stone.WorldLocation, FRotator(0.f, Stone.YawDeg, 0.f), Params))
		{
			Actor->SetupStone(StoneIndex, Stone);
			Actor->SetActorLabel(FString::Printf(
				TEXT("EditorPreview_Foe_M%02d_%d"),
				static_cast<int32>(Stone.FoeId), StoneIndex));
			EditorPreviewMeshActors.Add(Actor);
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Preview] Rebuild complete stones=%d beats=%d bridges=%d visualBindings=%d spawnedPreviewActors=%d."),
		PreviewStones.Num(),
		PreviewBeats.Num(),
		PreviewBridges.Num(),
		VisualBindings.Num(),
		EditorPreviewMeshActors.Num());
}

void ANightCourseHost::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=HostBeginPlay] Host='%s' Config='%s' Director='%s' autoStart=%d."),
		*GetNameSafe(this),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		*GetNameSafe(Director),
		bAutoStart ? 1 : 0);
	if (LayoutBounds)
	{
		LayoutBounds->SetBoxExtent(LayoutBoundsExtent);
	}

	if (PreviewKeyLight)
	{
		PreviewKeyLight->SetHiddenInGame(false);
		PreviewKeyLight->SetVisibility(true);
		PreviewKeyLight->SetIntensity(RuntimeKeyLightIntensity);
		PreviewKeyLight->SetLightColor(RuntimeKeyLightColor);
	}
	if (NightFog)
	{
		NightFog->FogDensity = RuntimeFogDensity;
	}

	if (!Config)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=HostBeginPlay] Config is missing; assign the canonical DA_Course asset in the editor. Course generation will not start."));
		return;
	}

	BuildPlayableStage();

	if (Director)
	{
		Director->Config = Config;
		Director->SetLayoutBoundsComponent(LayoutBounds, bEnforceLayoutBounds);
		Director->OnFinished.RemoveDynamic(this, &ANightCourseHost::HandleFinished);
		Director->OnFinished.AddDynamic(this, &ANightCourseHost::HandleFinished);
		Director->OnDebugMessage.RemoveDynamic(
			this,
			&ANightCourseHost::HandleDirectorDebugMessage);
		Director->OnDebugMessage.AddDynamic(
			this,
			&ANightCourseHost::HandleDirectorDebugMessage);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=HostBeginPlay] Director bound to Config='%s' LayoutBounds='%s' enforceBounds=%d."),
			*Config->GetPathName(),
			*GetNameSafe(LayoutBounds),
			bEnforceLayoutBounds ? 1 : 0);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=HostBeginPlay] Director component is missing; course cannot start."));
	}

	if (bAutoStart)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				AutoStartTimer,
				this,
				&ANightCourseHost::StartCourse,
				0.2f,
				false);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=HostBeginPlay] Auto-start timer scheduled for 0.2 seconds."));
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=HostBeginPlay] Cannot schedule auto-start: World is null."));
		}
	}
	else
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=HostBeginPlay] Auto-start disabled; call StartCourse manually."));
	}
}

void ANightCourseHost::BuildPlayableStage()
{
	// Stone chain owns all pads (including stone 0 under the runner). No extra road.
}

void ANightCourseHost::WireFeelFromPlayer()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	}

	if (!Pawn)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=BindPlayer] No player Pawn found; Director cannot bind runner or Feel."));
		return;
	}

	ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(Pawn);
	if (!CoursePawn)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=BindPlayer] Player Pawn='%s' class='%s' is not ANightCoursePawn."),
			*GetNameSafe(Pawn),
			*GetNameSafe(Pawn->GetClass()));
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=BindPlayer] Found Pawn='%s' class='%s' Config='%s'."),
		*GetNameSafe(CoursePawn),
		*GetNameSafe(CoursePawn->GetClass()),
		Config ? *Config->GetPathName() : TEXT("<null>"));

	if (Config && Config->HeroClass)
	{
		const ANightCoursePawn* HeroDefaults =
			Config->HeroClass->GetDefaultObject<ANightCoursePawn>();
		if (HeroDefaults)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=BindPlayer] Applying HeroClass='%s' visual defaults."),
				*GetNameSafe(Config->HeroClass));
			CoursePawn->HeroSkeletalMesh = HeroDefaults->HeroSkeletalMesh;
			// add by K2 (R1): HeroClass 只有属性会被拷过来，蓝图里直接挂在
			// HeroSkelMesh 组件上的网格传不过去，结果是配了骨骼却退到静态模、
			// 动画播在隐藏组件上。属性为空时回退去读组件，让蓝图里看到的即生效。
			if (!CoursePawn->HeroSkeletalMesh && HeroDefaults->HeroSkelMesh)
			{
				CoursePawn->HeroSkeletalMesh = HeroDefaults->HeroSkelMesh->GetSkeletalMeshAsset();
			}
			CoursePawn->HeroStaticMesh = HeroDefaults->HeroStaticMesh;
			CoursePawn->HeroMaterial = HeroDefaults->HeroMaterial;
			CoursePawn->HeroScale = HeroDefaults->HeroScale;
			CoursePawn->HeroPivotOffsetCm = HeroDefaults->HeroPivotOffsetCm;
			CoursePawn->ApplyConfiguredHeroVisual();
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=BindPlayer] HeroClass='%s' has no valid ANightCoursePawn CDO."),
				*GetNameSafe(Config->HeroClass));
		}
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[NightCourse][Stage=BindPlayer] Config has no HeroClass; using Pawn's existing visual defaults."));
	}

	if (Director)
	{
		Director->BindRunnerPawn(CoursePawn);
		CoursePawn->BindCourseDirector(Director);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=BindPlayer] Runner Pawn and CourseDirector bound."));
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=BindPlayer] Director is null; runner binding skipped."));
	}
	if (CoursePawn->FeelStub && Director)
	{
		Director->BindFeelBridge(CoursePawn->FeelStub);
		CoursePawn->FeelStub->OnInputResolved.RemoveDynamic(this, &ANightCourseHost::HandleFeelResolved);
		CoursePawn->FeelStub->OnInputResolved.AddDynamic(this, &ANightCourseHost::HandleFeelResolved);
		if (Config)
		{
			CoursePawn->FeelStub->Soul = Config->StartingSoul;
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=BindPlayer] FeelStub bound; startingSoul=%.1f."),
			Config ? Config->StartingSoul : 0.f);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=BindPlayer] Feel binding skipped: FeelStub='%s' Director='%s'."),
			*GetNameSafe(CoursePawn->FeelStub),
			*GetNameSafe(Director));
	}
}

void ANightCourseHost::StartCourse()
{
	FString Error;
	const bool bStarted = TryStartCourse(Error);
	if (bStarted)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=StartCourse] Start succeeded Host='%s' Config='%s'."),
			*GetNameSafe(this),
			Config ? *Config->GetPathName() : TEXT("<null>"));
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=StartCourse] Start failed Host='%s' Config='%s' Error='%s'."),
			*GetNameSafe(this),
			Config ? *Config->GetPathName() : TEXT("<null>"),
			Error.IsEmpty() ? TEXT("<empty>") : *Error);
	}
}

bool ANightCourseHost::TryStartCourse(FString& OutError)
{
	return StartNight_Implementation(Bootstrap, OutError);
}

bool ANightCourseHost::StartNight_Implementation(
	const FNightBootstrap& InBootstrap,
	FString& OutError)
{
	OutError.Reset();
	ClearCourseResult();
	Bootstrap = InBootstrap;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=StartRequest] Host='%s' Config='%s' Director='%s' Level=%d Seed=%d ForkPair=%d."),
		*GetNameSafe(this),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		*GetNameSafe(Director),
		static_cast<int32>(Bootstrap.LevelId),
		Bootstrap.Seed,
		static_cast<int32>(Bootstrap.ForkPair));
	WireFeelFromPlayer();
	if (!Director)
	{
		OutError = TEXT("NightCourseHost has no Director.");
		UE_LOG(LogTemp, Error, TEXT("[NightCourse][Stage=StartRequest] %s"), *OutError);
		EmitDebugMessage(
			FString::Printf(TEXT("Start rejected: %s"), *OutError),
			true);
		return false;
	}

	Director->Config = Config;
	if (!Director->Config)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=StartRequest] Config is null; creating transient fallback '%s'. It has no canonical Rule/Atom references."),
			TEXT("RuntimeG1Config"));
		Director->Config = NewObject<UNightG1CourseConfig>(
			this,
			TEXT("RuntimeG1Config"));
		Config = Director->Config;
	}

	const bool bStarted = Director->TryStartNight(Bootstrap, OutError);
	if (!bStarted && OutError.IsEmpty())
	{
		OutError = Director->GetLastFailureReason();
	}
	if (!bStarted && OutError.IsEmpty())
	{
		OutError = TEXT("NightCourseDirector rejected the start request.");
		EmitDebugMessage(
			FString::Printf(TEXT("Start rejected: %s"), *OutError),
			true);
	}
	if (bStarted)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=StartRequest] Director result started=%d Error='%s' LastFailure='%s'."),
			1,
			OutError.IsEmpty() ? TEXT("<empty>") : *OutError,
			Director->GetLastFailureReason().IsEmpty() ? TEXT("<empty>") : *Director->GetLastFailureReason());
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=StartRequest] Director result started=%d Error='%s' LastFailure='%s'."),
			0,
			OutError.IsEmpty() ? TEXT("<empty>") : *OutError,
			Director->GetLastFailureReason().IsEmpty() ? TEXT("<empty>") : *Director->GetLastFailureReason());
	}
	return bStarted;
}

void ANightCourseHost::ResetCourse()
{
	if (Director)
	{
		Director->ResetCourse();
	}
	ClearCourseResult();
	EmitDebugMessage(TEXT("NightCourse reset."), false);
}

void ANightCourseHost::ResetNight_Implementation()
{
	ResetCourse();
}

bool ANightCourseHost::HasNightResult_Implementation() const
{
	return bHasResult;
}

FNightResult ANightCourseHost::GetNightResult_Implementation() const
{
	return LastResult;
}

void ANightCourseHost::HandleFinished(const FNightResult& Result)
{
	LastResult = Result;
	bHasResult = true;
	LastFailureReason = Director
		? Director->GetLastFailureReason()
		: FString();
	OnNightFinished.Broadcast(Result);
	if (Result.bSuccess && !Result.bFailedMidway)
	{
		OnNightSucceeded.Broadcast(Result);
	}
	else
	{
		OnNightFailed.Broadcast(Result);
	}
	UE_LOG(LogTemp, Warning, TEXT("[NightCourseHost] Finished success=%d route=%d failedMidway=%d drops=%d soul=%.1f"),
		Result.bSuccess ? 1 : 0,
		static_cast<int32>(Result.RouteTaken),
		Result.bFailedMidway ? 1 : 0,
		Result.Ingredients.Num(),
		Result.SoulLeft);
	for (const FIngredientStack& Stack : Result.Ingredients)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Drop Id=%d Count=%d"), static_cast<int32>(Stack.Id), Stack.Count);
	}
}

void ANightCourseHost::HandleDirectorDebugMessage(
	const FString& Message,
	bool bIsError)
{
	EmitDebugMessage(Message, bIsError);
}

void ANightCourseHost::HandleFeelResolved(int32 NodeIndex, ENightJudgeOutcome Outcome)
{
	if (Director)
	{
		Director->NotifyFeelResolved(NodeIndex, Outcome);
	}
}

void ANightCourseHost::ClearCourseResult()
{
	bHasResult = false;
	LastResult = FNightResult();
	LastFailureReason.Reset();
}

void ANightCourseHost::EmitDebugMessage(
	const FString& Message,
	bool bIsError)
{
	OnDebugMessage.Broadcast(Message, bIsError);
	if (bIsError)
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourseHost][Debug] %s"), *Message);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("[NightCourseHost][Debug] %s"), *Message);
	}
}

void ANightCourseHost::DebugDumpState() const
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Dump] Host='%s' Config='%s' Director='%s' Rule='%s' AtomLibrary='%s' RouteRules='%s'."),
		*GetNameSafe(this),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		*GetNameSafe(Director),
		Config && Config->CourseRuleData ? *Config->CourseRuleData->GetPathName() : TEXT("<null>"),
		Config && Config->AtomRoute ? *Config->AtomRoute->GetPathName() : TEXT("<null>"),
		Config && Config->RouteRules ? *Config->RouteRules->GetPathName() : TEXT("<null>"));
	if (!Director)
	{
		UE_LOG(LogTemp, Error, TEXT("[NightCourse][Stage=Dump] No Director."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[NightCourse][Stage=Dump] Running=%d Phase=%d Route=%d Fork=%.2f BranchBeats=%d Elapsed=%.2f ActiveNode=%d Ingredients=%d"),
		Director->IsRunning() ? 1 : 0,
		static_cast<int32>(Director->GetPhase()),
		static_cast<int32>(Director->GetCurrentRoute()),
		Director->GetForkSecondsRemaining(),
		Director->GetBranchBeatCount(),
		Director->GetElapsedSeconds(),
		Director->GetActiveNodeIndex(),
		Director->GetCollectedIngredients().Num());
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[NightCourse][Stage=Dump] ResultValid=%d resultSuccess=%d failedMidway=%d route=%d failureReason=%s"),
		bHasResult ? 1 : 0,
		LastResult.bSuccess ? 1 : 0,
		LastResult.bFailedMidway ? 1 : 0,
		static_cast<int32>(LastResult.RouteTaken),
		LastFailureReason.IsEmpty() ? TEXT("<none>") : *LastFailureReason);
}
#pragma endregion K2 moonyfli
