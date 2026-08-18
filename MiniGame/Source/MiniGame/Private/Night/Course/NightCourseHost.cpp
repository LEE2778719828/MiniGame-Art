#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCoursePawn.h"
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
#include "UObject/ConstructorHelpers.h"

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		StageCubeMesh = CubeMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitMat(TEXT("/Game/Night/Course/Materials/M_NightUnlitColor.M_NightUnlitColor"));
	if (UnlitMat.Succeeded())
	{
		StageMaterial = UnlitMat.Object;
	}
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
	for (AStaticMeshActor* Actor : EditorPreviewMeshActors)
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

	UStaticMesh* MeshA = Config->BridgeMeshA.LoadSynchronous();
	UStaticMesh* MeshB = Config->BridgeMeshB.LoadSynchronous();
	if (!MeshA)
	{
		MeshA = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Night/Course/Art/Bridge/muban1.muban1"));
	}
	if (!MeshB)
	{
		MeshB = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Night/Course/Art/Bridge/muban2.muban2"));
	}
	PreviewBridgeA->SetStaticMesh(MeshA);
	PreviewBridgeB->SetStaticMesh(MeshB);

	UStaticMesh* FoeM01 = Config->FoeMeshM01.LoadSynchronous();
	UStaticMesh* FoeM02 = Config->FoeMeshM02.LoadSynchronous();
	UStaticMesh* FoeM03 = Config->FoeMeshM03.LoadSynchronous();
	UStaticMesh* FoeM04 = Config->FoeMeshM04.LoadSynchronous();
	UStaticMesh* FoeM05 = Config->FoeMeshM05.LoadSynchronous();
	if (!FoeM01)
	{
		FoeM01 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Night/Course/Art/Foe/fish.fish"));
	}
	if (!FoeM02)
	{
		FoeM02 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Night/Course/Art/Foe/box1.box1"));
	}
	if (!FoeM03)
	{
		FoeM03 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Night/Course/Art/Foe/box2.box2"));
	}
	if (!FoeM04)
	{
		FoeM04 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Night/Course/Art/Foe/box3.box3"));
	}
	if (!FoeM05)
	{
		FoeM05 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Night/Course/Art/Foe/cantingguai.cantingguai"));
	}
	PreviewFoeM01->SetStaticMesh(FoeM01);
	PreviewFoeM02->SetStaticMesh(FoeM02);
	PreviewFoeM03->SetStaticMesh(FoeM03);
	PreviewFoeM04->SetStaticMesh(FoeM04);
	PreviewFoeM05->SetStaticMesh(FoeM05);

	UMaterialInterface* DefaultPreviewMat = EditorPreviewMaterial.LoadSynchronous();
	if (!DefaultPreviewMat)
	{
		DefaultPreviewMat = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (DefaultPreviewMat)
	{
		const auto ApplyPreviewMaterial = [](
			UInstancedStaticMeshComponent* Component,
			UMaterialInterface* Material,
			const FLinearColor& Color)
		{
			if (!Component || !Component->GetStaticMesh() || !Material)
			{
				return;
			}
			for (int32 MaterialIndex = 0; MaterialIndex < Component->GetNumMaterials(); ++MaterialIndex)
			{
				Component->SetMaterial(MaterialIndex, Material);
			}
		};
		const auto ResolveMaterial = [this, DefaultPreviewMat](const TSoftObjectPtr<UMaterialInterface>& ConfigMaterial)
		{
			UMaterialInterface* Material = ConfigMaterial.LoadSynchronous();
			return Material ? Material : DefaultPreviewMat;
		};
		ApplyPreviewMaterial(PreviewBridgeA, ResolveMaterial(Config->BridgeMaterialA), EditorPreviewBridgeColorA);
		ApplyPreviewMaterial(PreviewBridgeB, ResolveMaterial(Config->BridgeMaterialB), EditorPreviewBridgeColorB);
		ApplyPreviewMaterial(PreviewFoeM01, ResolveMaterial(Config->FoeMaterialM01), EditorPreviewFoeColorM01);
		ApplyPreviewMaterial(PreviewFoeM02, ResolveMaterial(Config->FoeMaterialM02), EditorPreviewFoeColorM02);
		ApplyPreviewMaterial(PreviewFoeM03, ResolveMaterial(Config->FoeMaterialM03), EditorPreviewFoeColorM03);
		ApplyPreviewMaterial(PreviewFoeM04, ResolveMaterial(Config->FoeMaterialM04), EditorPreviewFoeColorM03);
		ApplyPreviewMaterial(PreviewFoeM05, ResolveMaterial(Config->FoeMaterialM05), EditorPreviewFoeColorM03);
	}

	const FNightGeneratedCourse Preview = UNightTrackGenerator::GenerateBaseOnly(
		Config->ProcParams,
		Config->TrackOrigin,
		Config->TrackForward);
	for (const FNightBridgeSpec& Bridge : Preview.Bridges)
	{
		UInstancedStaticMeshComponent* BridgePreview =
			Bridge.MeshVariant == 0 ? PreviewBridgeA : PreviewBridgeB;
		UStaticMesh* BridgeMesh = BridgePreview ? BridgePreview->GetStaticMesh() : nullptr;
		const FRotator BridgeRotation(0.f, Bridge.YawDeg - 90.f, 0.f);
		const float BridgeGlobalScale = FMath::Max(
			0.05f,
			Bridge.LengthScale * FMath::Max(0.01f, Config->BridgeGlobalScale));
		const FVector BridgeScale(BridgeGlobalScale);
		const FVector BridgeMeshCenter = BridgeMesh
			? BridgeMesh->GetBounds().Origin
			: FVector::ZeroVector;
		const FVector BridgeCenterOffset = BridgeRotation.RotateVector(
			(Config->BridgePivotOffsetCm - BridgeMeshCenter) * BridgeScale);
		const FTransform InstanceTransform(
			BridgeRotation,
			Bridge.WorldLocation + FVector(0.f, 0.f, 8.f) + BridgeCenterOffset,
			BridgeScale);
		UInstancedStaticMeshComponent* BridgePreviewComponent =
			Bridge.MeshVariant == 0 ? PreviewBridgeA : PreviewBridgeB;
		if (UStaticMesh* PreviewMesh = BridgePreviewComponent
			? BridgePreviewComponent->GetStaticMesh()
			: nullptr)
		{
			UMaterialInterface* PreviewMaterial =
				BridgePreviewComponent->GetMaterial(0);
			const FString Label = FString::Printf(
				TEXT("EditorPreview_Bridge_%s_%d"),
				Bridge.MeshVariant == 0 ? TEXT("A") : TEXT("B"),
				Bridge.FromStoneIndex);
			if (AStaticMeshActor* Actor = NightCourseStage_Private::SpawnEditorPreviewMesh(
				GetWorld(),
				PreviewMesh,
				PreviewMaterial,
				InstanceTransform,
				Label))
			{
				EditorPreviewMeshActors.Add(Actor);
			}
		}
	}
	for (int32 StoneIndex = 0; StoneIndex < Preview.Stones.Num(); ++StoneIndex)
	{
		const FNightStoneSpec& Stone = Preview.Stones[StoneIndex];
		if (!Stone.bHasFoe)
		{
			continue;
		}
		UInstancedStaticMeshComponent* FoePreview = PreviewFoeM01;
		if (Stone.FoeId == EFoeId::M02)
		{
			FoePreview = PreviewFoeM02;
		}
		else if (Stone.FoeId == EFoeId::M03)
		{
			FoePreview = PreviewFoeM03;
		}
		else if (Stone.FoeId == EFoeId::M04)
		{
			FoePreview = PreviewFoeM04;
		}
		else if (Stone.FoeId == EFoeId::M05)
		{
			FoePreview = PreviewFoeM05;
		}
		if (FoePreview && FoePreview->GetStaticMesh())
		{
			const float FoeScale = Config->FoeScale;
			const FRotator FoeRotation(0.f, Stone.YawDeg + Config->FoeYawOffsetDeg, 0.f);
			const FVector MeshCenter = FoePreview->GetStaticMesh()->GetBounds().Origin;
			const FVector CenterOffset = FoeRotation.RotateVector(
				(Config->FoePivotOffsetCm - MeshCenter) * FoeScale);
			const FTransform FoeTransform(
				FoeRotation,
				Stone.WorldLocation + FVector(0.f, 0.f, Config->FoeHeightOffsetCm) + CenterOffset,
				FVector(
					FoeScale,
					FoeScale,
					FoeScale));
			const FString Label = FString::Printf(
				TEXT("EditorPreview_Foe_M%02d_%d"),
				static_cast<int32>(Stone.FoeId),
				StoneIndex);
			if (AStaticMeshActor* Actor = NightCourseStage_Private::SpawnEditorPreviewMesh(
				GetWorld(),
				FoePreview->GetStaticMesh(),
				FoePreview->GetMaterial(0),
				FoeTransform,
				Label))
			{
				EditorPreviewMeshActors.Add(Actor);
			}
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
		if (Config)
		{
			UMaterialInterface* HeroMaterial = Config->HeroMaterial.LoadSynchronous();
			if (!HeroMaterial)
			{
				HeroMaterial = Config->DefaultArtMaterial.LoadSynchronous();
			}
			CoursePawn->ApplyHeroMesh(
				Config->HeroMesh.LoadSynchronous(),
				HeroMaterial,
				Config->HeroPivotOffsetCm);
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
