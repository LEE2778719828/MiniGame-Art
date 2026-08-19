#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Night/Course/NightTrackGenerator.h"
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

}

void ANightCourseHost::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		RebuildEditorPreview();
	}
}

void ANightCourseHost::RebuildEditorPreview()
{
	if (!PreviewBridgeA || !PreviewBridgeB
		|| !PreviewFoeM01 || !PreviewFoeM02 || !PreviewFoeM03
		|| !PreviewFoeM04 || !PreviewFoeM05)
	{
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
	if (!Config || !Config->ProcParams.bEnableProcGenerator)
	{
		return;
	}

	const FNightGeneratedCourse Preview = UNightTrackGenerator::GenerateBaseOnly(
		Config->ProcParams,
		Config->TrackOrigin,
		Config->TrackForward);
	for (const FNightBridgeSpec& Bridge : Preview.Bridges)
	{
		UClass* BridgeClass =
			Bridge.MeshVariant == 0 ? Config->BridgeClassA.Get() : Config->BridgeClassB.Get();
		if (!BridgeClass)
		{
			// Missing bridge BP intentionally produces an empty actor.
			BridgeClass = ANightBridgeSegmentActor::StaticClass();
		}
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
	for (int32 StoneIndex = 0; StoneIndex < Preview.Stones.Num(); ++StoneIndex)
	{
		const FNightStoneSpec& Stone = Preview.Stones[StoneIndex];
		if (!Stone.bHasFoe)
		{
			continue;
		}
		UClass* FoeClass = nullptr;
		switch (Stone.FoeId)
		{
		case EFoeId::M01: FoeClass = Config->FoeClassM01.Get(); break;
		case EFoeId::M02: FoeClass = Config->FoeClassM02.Get(); break;
		case EFoeId::M03: FoeClass = Config->FoeClassM03.Get(); break;
		case EFoeId::M04: FoeClass = Config->FoeClassM04.Get(); break;
		case EFoeId::M05: FoeClass = Config->FoeClassM05.Get(); break;
		default: break;
		}
		if (!FoeClass)
		{
			// Missing foe BP intentionally produces an empty actor.
			FoeClass = ANightCourseStoneActor::StaticClass();
		}
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
}

void ANightCourseHost::BeginPlay()
{
	Super::BeginPlay();

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
		Config = NewObject<UNightG1CourseConfig>(this, TEXT("RuntimeG1Config"));
	}

	BuildPlayableStage();

	if (Director)
	{
		Director->Config = Config;
		Director->OnFinished.AddDynamic(this, &ANightCourseHost::HandleFinished);
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
		}
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

	if (ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(Pawn))
	{
		if (Config && Config->HeroClass)
		{
			const ANightCoursePawn* HeroDefaults =
				Config->HeroClass->GetDefaultObject<ANightCoursePawn>();
			if (HeroDefaults)
			{
				CoursePawn->HeroSkeletalMesh = HeroDefaults->HeroSkeletalMesh;
				CoursePawn->HeroStaticMesh = HeroDefaults->HeroStaticMesh;
				CoursePawn->HeroMaterial = HeroDefaults->HeroMaterial;
				CoursePawn->HeroScale = HeroDefaults->HeroScale;
				CoursePawn->HeroPivotOffsetCm = HeroDefaults->HeroPivotOffsetCm;
				CoursePawn->ApplyConfiguredHeroVisual();
			}
		}
		if (Director)
		{
			Director->BindRunnerPawn(CoursePawn);
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
		}
	}
}

void ANightCourseHost::StartCourse()
{
	WireFeelFromPlayer();
	if (Director)
	{
		Director->Config = Config;
		if (!Director->Config)
		{
			Director->Config = NewObject<UNightG1CourseConfig>(this, TEXT("RuntimeG1Config"));
			Config = Director->Config;
		}
		Director->StartNight(Bootstrap);
	}
}

void ANightCourseHost::HandleFinished(const FNightResult& Result)
{
	LastResult = Result;
	UE_LOG(LogTemp, Warning, TEXT("[NightCourseHost] Finished success=%d drops=%d soul=%.1f"),
		Result.bSuccess ? 1 : 0, Result.Ingredients.Num(), Result.SoulLeft);
	for (const FIngredientStack& Stack : Result.Ingredients)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Drop Id=%d Count=%d"), static_cast<int32>(Stack.Id), Stack.Count);
	}
}

void ANightCourseHost::HandleFeelResolved(int32 NodeIndex, ENightJudgeOutcome Outcome)
{
	if (Director)
	{
		Director->NotifyFeelResolved(NodeIndex, Outcome);
	}
}

void ANightCourseHost::DebugDumpState() const
{
	if (!Director)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NightCourseHost] No Director"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[NightCourseHost] Running=%d Phase=%d Elapsed=%.2f ActiveNode=%d Ingredients=%d"),
		Director->IsRunning() ? 1 : 0,
		static_cast<int32>(Director->GetPhase()),
		Director->GetElapsedSeconds(),
		Director->GetActiveNodeIndex(),
		Director->GetCollectedIngredients().Num());
}
#pragma endregion K2 moonyfli
