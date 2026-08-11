#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
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
	UWorld* World = GetWorld();
	if (!World || !StageCubeMesh)
	{
		return;
	}

	UMaterialInterface* Mat = StageMaterial.Get();
	using namespace NightCourseStage_Private;

	const float FirstNode = Config ? Config->FirstNodeDistance : 500.f;
	const float CourseLen = Config
		? (Config->FirstNodeDistance + Config->NodeSpacing * FMath::Max(Config->NodeCount, 1) + 400.f)
		: 4000.f;
	const float MidX = CourseLen * 0.5f;

	SpawnBox(World, StageCubeMesh, Mat, FVector(MidX, 0.f, -20.f), FVector(CourseLen / 100.f, 5.f, 0.35f),
		FLinearColor(0.22f, 0.32f, 0.55f), TEXT("G1_Stage_Floor"));

	SpawnBox(World, StageCubeMesh, Mat, FVector(MidX, -280.f, 40.f), FVector(CourseLen / 100.f, 0.25f, 1.0f),
		FLinearColor(0.35f, 0.7f, 1.0f), TEXT("G1_Stage_RailL"));
	SpawnBox(World, StageCubeMesh, Mat, FVector(MidX, 280.f, 40.f), FVector(CourseLen / 100.f, 0.25f, 1.0f),
		FLinearColor(0.35f, 0.7f, 1.0f), TEXT("G1_Stage_RailR"));

	// Gate marks the first beat ahead of the player.
	SpawnBox(World, StageCubeMesh, Mat, FVector(FirstNode, -180.f, 120.f), FVector(0.25f, 0.25f, 2.6f),
		FLinearColor(1.f, 0.85f, 0.25f), TEXT("G1_Stage_GateL"));
	SpawnBox(World, StageCubeMesh, Mat, FVector(FirstNode, 180.f, 120.f), FVector(0.25f, 0.25f, 2.6f),
		FLinearColor(1.f, 0.85f, 0.25f), TEXT("G1_Stage_GateR"));
	SpawnBox(World, StageCubeMesh, Mat, FVector(FirstNode, 0.f, 250.f), FVector(0.25f, 3.8f, 0.25f),
		FLinearColor(1.f, 0.9f, 0.35f), TEXT("G1_Stage_GateTop"));

	SpawnBox(World, StageCubeMesh, Mat, FVector(CourseLen + 200.f, 0.f, 500.f), FVector(0.5f, 28.f, 14.f),
		FLinearColor(0.12f, 0.1f, 0.22f), TEXT("G1_Stage_Backdrop"));

	SpawnBox(World, StageCubeMesh, Mat, FVector(MidX, -900.f, 400.f), FVector(CourseLen / 100.f + 2.f, 0.4f, 12.f),
		FLinearColor(0.08f, 0.09f, 0.16f), TEXT("G1_Stage_WallL"));
	SpawnBox(World, StageCubeMesh, Mat, FVector(MidX, 900.f, 400.f), FVector(CourseLen / 100.f + 2.f, 0.4f, 12.f),
		FLinearColor(0.08f, 0.09f, 0.16f), TEXT("G1_Stage_WallR"));

	SpawnBox(World, StageCubeMesh, Mat, FVector(0.f, 0.f, -60.f), FVector(8.f, 12.f, 0.4f),
		FLinearColor(0.16f, 0.2f, 0.3f), TEXT("G1_Stage_Pad"));
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
