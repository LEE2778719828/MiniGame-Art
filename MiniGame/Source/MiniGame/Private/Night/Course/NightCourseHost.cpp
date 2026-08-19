#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightG1CourseConfig.h"
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
	if (!Config || !Director)
	{
		return;
	}

	Director->Config = Config;
	Director->SetLayoutBoundsComponent(LayoutBounds, bEnforceLayoutBounds);
	FNightGeneratedCourse Preview;
	TArray<FNightAtomVisualBinding> VisualBindings;
	if (!Director->BuildCourseForPreview(
		Preview.Stones,
		Preview.Beats,
		Preview.Bridges,
		VisualBindings))
	{
		return;
	}

	TSet<int32> ArtBridgeIndexes;
	TSet<int32> ArtStoneIndexes;
	for (const FNightAtomVisualBinding& Binding : VisualBindings)
	{
		TSubclassOf<AActor> VisualClass = Binding.VisualPrefabClass;
		if (!Binding.bIsBridge
			&& Preview.Stones.IsValidIndex(Binding.StoneIndex)
			&& Preview.Stones[Binding.StoneIndex].bHasFoe
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
		if (Binding.bIsBridge && Preview.Bridges.IsValidIndex(Binding.BridgeIndex))
		{
			Actor = NightCourseStage_Private::SpawnEditorPreviewVisual(
				GetWorld(),
				VisualClass.Get(),
				Binding.LocalTransform,
				nullptr,
				INDEX_NONE,
				&Preview.Bridges[Binding.BridgeIndex],
				FString::Printf(
					TEXT("EditorPreview_Atom_%s_Bridge_%d"),
					Binding.AtomKey.IsEmpty() ? TEXT("Legacy") : *Binding.AtomKey,
					Binding.BridgeIndex));
		}
		else if (!Binding.bIsBridge && Preview.Stones.IsValidIndex(Binding.StoneIndex))
		{
			Actor = NightCourseStage_Private::SpawnEditorPreviewVisual(
				GetWorld(),
				VisualClass.Get(),
				Binding.LocalTransform,
				&Preview.Stones[Binding.StoneIndex],
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
	for (const FNightBridgeSpec& Bridge : Preview.Bridges)
	{
		if (ArtBridgeIndexes.Contains(&Bridge - Preview.Bridges.GetData()))
		{
			continue;
		}
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
		if (ArtStoneIndexes.Contains(StoneIndex))
		{
			continue;
		}
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
		Config = NewObject<UNightG1CourseConfig>(this, TEXT("RuntimeG1Config"));
	}

	BuildPlayableStage();

	if (Director)
	{
		Director->Config = Config;
		Director->SetLayoutBoundsComponent(LayoutBounds, bEnforceLayoutBounds);
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
