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
	NightFog->FogDensity = 0.012f;
	NightFog->FogHeightFalloff = 0.18f;
	NightFog->FogInscatteringLuminance = FLinearColor(0.03f, 0.07f, 0.18f);
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
	PreviewKeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("PreviewKeyLight"));
	PreviewKeyLight->SetRelativeRotation(FRotator(-35.f, -35.f, 0.f));
	PreviewKeyLight->Intensity = 4.f;
	PreviewKeyLight->LightColor = FColor(180, 210, 255);
	PreviewKeyLight->SetHiddenInGame(true);

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
	if (!PreviewBridgeA || !PreviewBridgeB)
	{
		return;
	}

	PreviewBridgeA->ClearInstances();
	PreviewBridgeB->ClearInstances();
	PreviewFoeM01->ClearInstances();
	PreviewFoeM02->ClearInstances();
	PreviewFoeM03->ClearInstances();
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
	PreviewFoeM01->SetStaticMesh(FoeM01);
	PreviewFoeM02->SetStaticMesh(FoeM02);
	PreviewFoeM03->SetStaticMesh(FoeM03);

	UMaterialInterface* PreviewMat = EditorPreviewMaterial.LoadSynchronous();
	if (!PreviewMat)
	{
		PreviewMat = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (PreviewMat)
	{
		const auto ApplyPreviewMaterial = [PreviewMat](
			UInstancedStaticMeshComponent* Component,
			const FLinearColor& Color)
		{
			if (!Component || !Component->GetStaticMesh())
			{
				return;
			}
			Component->SetMaterial(0, PreviewMat);
			if (UMaterialInstanceDynamic* MID =
				Component->CreateAndSetMaterialInstanceDynamic(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), Color);
			}
		};
		ApplyPreviewMaterial(PreviewBridgeA, EditorPreviewBridgeColorA);
		ApplyPreviewMaterial(PreviewBridgeB, EditorPreviewBridgeColorB);
		ApplyPreviewMaterial(PreviewFoeM01, EditorPreviewFoeColorM01);
		ApplyPreviewMaterial(PreviewFoeM02, EditorPreviewFoeColorM02);
		ApplyPreviewMaterial(PreviewFoeM03, EditorPreviewFoeColorM03);
	}

	const FNightGeneratedCourse Preview = UNightTrackGenerator::GenerateBaseOnly(
		Config->ProcParams,
		Config->TrackOrigin,
		Config->TrackForward);
	for (const FNightBridgeSpec& Bridge : Preview.Bridges)
	{
		const FTransform InstanceTransform(
			FRotator(0.f, Bridge.YawDeg - 90.f, 0.f),
			Bridge.WorldLocation + FVector(0.f, 0.f, 8.f),
			FVector(12.f, FMath::Max(0.05f, Bridge.LengthScale), 4.f));
		if (Bridge.MeshVariant == 0 && MeshA)
		{
			PreviewBridgeA->AddInstance(InstanceTransform);
		}
		else if (MeshB)
		{
			PreviewBridgeB->AddInstance(InstanceTransform);
		}
	}
	for (const FNightStoneSpec& Stone : Preview.Stones)
	{
		if (!Stone.bHasFoe)
		{
			continue;
		}
		UInstancedStaticMeshComponent* FoePreview = PreviewFoeM01;
		if (Stone.FoeId == EFoeId::M02)
		{
			FoePreview = PreviewFoeM02;
		}
		else if (Stone.FoeId == EFoeId::M03
			|| Stone.FoeId == EFoeId::M04
			|| Stone.FoeId == EFoeId::M05)
		{
			FoePreview = PreviewFoeM03;
		}
		if (FoePreview && FoePreview->GetStaticMesh())
		{
			FoePreview->AddInstance(FTransform(
				FRotator(0.f, Stone.YawDeg + Config->FoeYawOffsetDeg, 0.f),
				Stone.WorldLocation + FVector(0.f, 0.f, Config->FoeHeightOffsetCm),
				FVector(
					Config->FoeScale,
					Config->FoeScale,
					Config->FoeScale)));
		}
	}
}

void ANightCourseHost::BeginPlay()
{
	Super::BeginPlay();

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
			CoursePawn->ApplyHeroMesh(Config->HeroMesh.LoadSynchronous());
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
