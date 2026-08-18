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
		if (ProcParamsAsset)
		{
			Director->ProcParamsAsset = ProcParamsAsset; //add by K2
		}
		FNightBootstrap Boot = Bootstrap;
		if (bApplyLevelTableOnStart && Config)
		{
			Config->ApplyLevelDefaultsToBootstrap(Boot);
		}
		Director->StartNight(Boot);
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

	UE_LOG(LogTemp, Warning, TEXT("[NightCourseHost] Running=%d Phase=%d Elapsed=%.2f ActiveNode=%d Route=%d Scheme=%d Ingredients=%d Fork=%d Swap=%d Awaiting=%d Seed=%d"),
		Director->IsRunning() ? 1 : 0,
		static_cast<int32>(Director->GetPhase()),
		Director->GetElapsedSeconds(),
		Director->GetActiveNodeIndex(),
		static_cast<int32>(Director->GetRouteTaken()),
		static_cast<int32>(Director->GetActiveControlScheme()),
		Director->GetCollectedIngredients().Num(),
		Director->IsForkChoiceActive() ? 1 : 0,
		(Director->IsKeySwapWarningActive() || Director->IsKeySwapSafetyActive()) ? 1 : 0,
		Director->IsAwaitingInput() ? 1 : 0,
		Director->GetResolvedProcSeed()); //add by K2

#pragma region K2 moonyfli
	if (Director->GetActiveNodeIndex() != INDEX_NONE
		&& Director->GetBeatSpecs().IsValidIndex(Director->GetActiveNodeIndex()))
	{
		const FNightBeatSpec& Beat = Director->GetBeatSpecs()[Director->GetActiveNodeIndex()];
		UE_LOG(LogTemp, Warning, TEXT("[NightCourseHost] RequiredAction=%d FromStone=%d ToStone=%d"),
			static_cast<int32>(Beat.Action), Beat.FromStoneIndex, Beat.ToStoneIndex);
	}
#pragma endregion K2 moonyfli
}
#pragma endregion K2 moonyfli
