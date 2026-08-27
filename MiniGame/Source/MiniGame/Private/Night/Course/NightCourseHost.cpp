#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightBridgeSegmentActor.h"
#include "Night/Course/NightCourseGameMode.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseAtomRouteData.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightCourseHUD.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightCourseRoadsideActor.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "../../../SStandaloneSandbox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h" //add by K2
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
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

#if WITH_EDITOR
		Actor->SetActorLabel(Label);
		Actor->SetIsTemporarilyHiddenInEditor(false);
#endif
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
#if WITH_EDITOR
		Actor->SetActorLabel(Label);
#endif
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
				StoneActor->ApplyFoeZCompensation(true);
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

	static ANightRoadsideSegmentActor* SpawnEditorPreviewRoadside(
		UWorld* World,
		const FNightRoadsidePropSpec& Spec,
		const int32 Index)
	{
		if (!World || !Spec.PropClass)
		{
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANightRoadsideSegmentActor* Actor =
			World->SpawnActor<ANightRoadsideSegmentActor>(
				Spec.PropClass,
				Spec.WorldTransform,
				Params);
		if (!Actor)
		{
			return nullptr;
		}

#if WITH_EDITOR
		Actor->SetActorLabel(
			FString::Printf(
				TEXT("EditorPreview_Roadside_%s_%s_%d"),
				Spec.Kind == ENightRoadsideKind::House
					? TEXT("House")
					: TEXT("Pole"),
				Spec.Side < 0 ? TEXT("Left") : TEXT("Right"),
				Index));
#endif
		Actor->SetActorTransform(Spec.WorldTransform);
#if WITH_EDITOR
		Actor->SetIsTemporarilyHiddenInEditor(false);
#endif
		Actor->SetActorHiddenInGame(false);
		Actor->SetActorEnableCollision(false);
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

	static ENightLevelId ToNightLevelId(const FName LevelId)
	{
		if (LevelId == TEXT("L1"))
		{
			return ENightLevelId::L1;
		}
		if (LevelId == TEXT("L2"))
		{
			return ENightLevelId::L2;
		}
		if (LevelId == TEXT("L3"))
		{
			return ENightLevelId::L3;
		}
		return ENightLevelId::T0;
	}

	static ENightForkPair ToNightForkPair(const FName ForkPair)
	{
		if (ForkPair == TEXT("AC"))
		{
			return ENightForkPair::AC;
		}
		if (ForkPair == TEXT("BC"))
		{
			return ENightForkPair::BC;
		}
		return ENightForkPair::AB;
	}

	static ENightRouteId ToNightRouteId(const FName RouteId)
	{
		if (RouteId.IsNone() || RouteId.ToString().Equals(TEXT("A"), ESearchCase::IgnoreCase))
		{
			return ENightRouteId::A;
		}
		if (RouteId.ToString().Equals(TEXT("B"), ESearchCase::IgnoreCase))
		{
			return ENightRouteId::B;
		}
		if (RouteId.ToString().Equals(TEXT("C"), ESearchCase::IgnoreCase))
		{
			return ENightRouteId::C;
		}
		return ENightRouteId::None;
	}

	static FNightBootstrap MakeNightBootstrap(const FSNightBootstrap& Source)
	{
		FNightBootstrap Bootstrap;
		Bootstrap.LevelId = ToNightLevelId(Source.LevelId);
		Bootstrap.DefaultRoute = ToNightRouteId(Source.DefaultRoute);
		Bootstrap.ForkPair = ToNightForkPair(Source.ForkPair);
		Bootstrap.GiftBuffs.bGuideKite = Source.GiftBuffState.bGuideKite;
		Bootstrap.GiftBuffs.bSpareLamp = Source.GiftBuffState.bLifeLamp;
		Bootstrap.GiftBuffs.bKeyCoin = Source.GiftBuffState.bBeatCoin;
		Bootstrap.GiftBuffs.bTaotieBox = Source.GiftBuffState.bGluttonBox;
		Bootstrap.GiftBuffs.PreForkGatherAmountBonus = Source.GiftBuffState.PreForkGatherAmountBonus;
		Bootstrap.GiftBuffs.MatchShieldCharges = Source.GiftBuffState.MatchShieldCharges;
		Bootstrap.GiftBuffs.PostForkInvulnerableSeconds = Source.GiftBuffState.PostForkInvulnDashSeconds;
		Bootstrap.GiftBuffs.NearDeathHealAmount = Source.GiftBuffState.NearDeathHeal;
		Bootstrap.GiftBuffs.NearDeathThreshold = Source.GiftBuffState.NearDeathThreshold;
		Bootstrap.Seed = Source.Seed;
		return Bootstrap;
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
	TArray<FNightForkAtomSpec> PreviewForkAtoms;
	TArray<FNightRoadsidePropSpec> PreviewRoadsideSpecs;
	if (!Director->BuildCourseForPreview(
		PreviewStones,
		PreviewBeats,
		PreviewBridges,
		VisualBindings,
		PreviewForkAtoms))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Preview] Build failed for Config='%s'; no preview actors will be created."),
			*Config->GetPathName());
		return;
	}
	if (!Director->BuildRoadsideSpecs(
		PreviewStones,
		PreviewBridges,
		PreviewForkAtoms,
		PreviewRoadsideSpecs))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Preview] Roadside build failed for Config='%s'."),
			*Config->GetPathName());
		return;
	}

	for (int32 ForkIndex = 0; ForkIndex < PreviewForkAtoms.Num(); ++ForkIndex)
	{
		const FNightForkAtomSpec& ForkSpec = PreviewForkAtoms[ForkIndex];
		if (!ForkSpec.ActorClass)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=Preview] Fork Atom %d has no Blueprint class."),
				ForkIndex);
			continue;
		}
		if (AActor* Actor = NightCourseStage_Private::SpawnEditorPreviewVisual(
			GetWorld(),
			ForkSpec.ActorClass.Get(),
			ForkSpec.WorldTransform,
			nullptr,
			INDEX_NONE,
			nullptr,
			FString::Printf(
				TEXT("EditorPreview_Fork_%d_%d"),
				static_cast<int32>(ForkSpec.ForkPair),
				ForkIndex)))
		{
			EditorPreviewMeshActors.Add(Actor);
		}
	}

	TSet<int32> ArtBridgeIndexes;
	TSet<int32> PreviewedFoeStoneIndexes;
	for (const FNightAtomVisualBinding& Binding : VisualBindings)
	{
		if (Binding.bIsBridge)
		{
			UClass* VisualClass = Binding.VisualPrefabClass.Get();
			if (!VisualClass)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse][Stage=Preview] Bridge binding %d has no Blueprint class."),
					Binding.BridgeIndex);
				continue;
			}
			if (!PreviewBridges.IsValidIndex(Binding.BridgeIndex))
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse][Stage=Preview] Bridge binding %d has no matching bridge spec."),
					Binding.BridgeIndex);
				continue;
			}
			if (!VisualClass->IsChildOf(ANightBridgeSegmentActor::StaticClass()))
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[NightCourse][Stage=Preview] Bridge binding %d resolves to a non-Bridge BP."),
					Binding.BridgeIndex);
				continue;
			}
			if (PreviewBridges.IsValidIndex(Binding.BridgeIndex))
			{
				if (AActor* Actor = NightCourseStage_Private::SpawnEditorPreviewVisual(
					GetWorld(),
					VisualClass,
					Binding.LocalTransform,
					nullptr,
					INDEX_NONE,
					&PreviewBridges[Binding.BridgeIndex],
					FString::Printf(
						TEXT("EditorPreview_Atom_%s_Bridge_%d"),
						Binding.AtomKey.IsEmpty() ? TEXT("Legacy") : *Binding.AtomKey,
						Binding.BridgeIndex)))
				{
					EditorPreviewMeshActors.Add(Actor);
					ArtBridgeIndexes.Add(Binding.BridgeIndex);
				}
			}
			continue;
		}

		if (!PreviewStones.IsValidIndex(Binding.StoneIndex)
			|| !PreviewStones[Binding.StoneIndex].bHasFoe)
		{
			continue;
		}

		FString FoeError;
		UClass* FoeClass = Config->ResolveFoeActorClass(
			PreviewStones[Binding.StoneIndex].FoeId,
			FoeError);
		if (!FoeClass)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=Preview] stone=%d FoeId=%d has no mapped Blueprint: %s."),
				Binding.StoneIndex,
				static_cast<int32>(PreviewStones[Binding.StoneIndex].FoeId),
				FoeError.IsEmpty() ? TEXT("<unknown>") : *FoeError);
			continue;
		}
		if (AActor* Actor = NightCourseStage_Private::SpawnEditorPreviewVisual(
			GetWorld(),
			FoeClass,
			Binding.LocalTransform,
			&PreviewStones[Binding.StoneIndex],
			Binding.StoneIndex,
			nullptr,
			FString::Printf(
				TEXT("EditorPreview_Atom_%s_Foe_%d"),
				Binding.AtomKey.IsEmpty() ? TEXT("Legacy") : *Binding.AtomKey,
				Binding.StoneIndex)))
		{
			EditorPreviewMeshActors.Add(Actor);
			PreviewedFoeStoneIndexes.Add(Binding.StoneIndex);
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
		if (PreviewedFoeStoneIndexes.Contains(StoneIndex))
		{
			continue;
		}
		if (!Stone.bHasFoe)
		{
			continue;
		}
		FString FoeError;
		UClass* FoeClass = Config->ResolveFoeActorClass(Stone.FoeId, FoeError);
		if (!FoeClass)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=Preview] fallback stone=%d FoeId=%d has no mapped Blueprint: %s."),
				StoneIndex,
				static_cast<int32>(Stone.FoeId),
				FoeError.IsEmpty() ? TEXT("<unknown>") : *FoeError);
			continue;
		}
		const FTransform PreviewTransform(
			FRotator(0.f, Stone.YawDeg, 0.f),
			Stone.WorldLocation,
			FVector::OneVector);
		if (AActor* Actor = NightCourseStage_Private::SpawnEditorPreviewVisual(
			GetWorld(),
			FoeClass,
			PreviewTransform,
			&Stone,
			StoneIndex,
			nullptr,
			FString::Printf(
				TEXT("EditorPreview_Foe_M%02d_%d"),
				static_cast<int32>(Stone.FoeId),
				StoneIndex)))
		{
			EditorPreviewMeshActors.Add(Actor);
			PreviewedFoeStoneIndexes.Add(StoneIndex);
		}
	}

	for (int32 RoadsideIndex = 0;
		RoadsideIndex < PreviewRoadsideSpecs.Num();
		++RoadsideIndex)
	{
		if (AActor* Actor = NightCourseStage_Private::SpawnEditorPreviewRoadside(
			GetWorld(),
			PreviewRoadsideSpecs[RoadsideIndex],
			RoadsideIndex))
		{
			EditorPreviewMeshActors.Add(Actor);
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Preview] Rebuild complete stones=%d beats=%d bridges=%d visualBindings=%d forkAtoms=%d roadside=%d spawnedPreviewActors=%d."),
		PreviewStones.Num(),
		PreviewBeats.Num(),
		PreviewBridges.Num(),
		VisualBindings.Num(),
		PreviewForkAtoms.Num(),
		PreviewRoadsideSpecs.Num(),
		EditorPreviewMeshActors.Num());
}

void ANightCourseHost::PrepareChefNightFlow()
{
	if (!bUseChefDayFlow)
	{
		return;
	}

	USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
	if (!GameInstance)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[NightCourse][Stage=Flow] No USChefGameInstance found; using the Host Bootstrap without Day flow."));
		return;
	}

	if (!GameInstance->PrepareNightForCourse())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Flow] Could not establish NightRunning in USChefGameInstance."));
		return;
	}

	if (GameInstance->Phase == ESGamePhase::NightRunning)
	{
		Bootstrap = NightCourseStage_Private::MakeNightBootstrap(
			GameInstance->GetPendingNightBootstrap());
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=Flow] NightRunning established from Day flow: stage=%s defaultRoute=%s seed=%d forkPair=%s."),
			*GameInstance->StageId.ToString(),
			*GameInstance->GetPendingNightBootstrap().DefaultRoute.ToString(),
			Bootstrap.Seed,
			*GameInstance->GetPendingNightBootstrap().ForkPair.ToString());
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[NightCourse][Stage=Flow] GameInstance phase=%d after PrepareNightForCourse; Host Bootstrap was not replaced."),
			static_cast<int32>(GameInstance->Phase));
	}
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
	PrepareChefNightFlow();
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
			CoursePawn->HeroZCompensationCm = HeroDefaults->HeroZCompensationCm;
			CoursePawn->bApplyHeroZCompensationInPreview =
				HeroDefaults->bApplyHeroZCompensationInPreview;
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
		TEXT("[NightCourse][Stage=StartRequest] Host='%s' Config='%s' Director='%s' Level=%d DefaultRoute=%d Seed=%d ForkPair=%d."),
		*GetNameSafe(this),
		Config ? *Config->GetPathName() : TEXT("<null>"),
		*GetNameSafe(Director),
		static_cast<int32>(Bootstrap.LevelId),
		static_cast<int32>(Bootstrap.DefaultRoute),
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
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimer);
	}
	if (Director)
	{
		Director->ResetCourse();
	}
	ClearPendingResultPresentation(true);
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

	USChefGameInstance* GameInstance =
		bUseChefDayFlow ? GetGameInstance<USChefGameInstance>() : nullptr;
	const bool bDayFlowAccepted = GameInstance
		? GameInstance->ConsumeNightCourseResult(Result)
		: !bUseChefDayFlow;
	if (bUseChefDayFlow && !GameInstance)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Flow] Night result cannot reach Day: USChefGameInstance is missing."));
	}
	else if (bUseChefDayFlow && !bDayFlowAccepted)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Flow] Day rejected the Night result; automatic transition/retry was not applied."));
	}

	if (bWaitForResultPresentation)
	{
		ANightCourseHUD* NightHUD = nullptr;
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			NightHUD = Cast<ANightCourseHUD>(PC->GetHUD());
		}
		if (NightHUD)
		{
			NightHUD->OnNightResultContinueRequested.RemoveDynamic(
				this,
				&ANightCourseHost::HandleNightResultContinueRequested);
			NightHUD->OnNightResultContinueRequested.AddDynamic(
				this,
				&ANightCourseHost::HandleNightResultContinueRequested);
			bPendingDayFlowAccepted = bDayFlowAccepted;
			bAwaitingResultContinue = true;
			const bool bPresented = NightHUD->PresentNightResult(Result);
			if (bPresented || !bAwaitingResultContinue)
			{
				return;
			}

			// Missing embedded result widget: preserve the previous immediate flow.
			ClearPendingResultPresentation(false);
		}
	}

	ApplyPostResultFlow(bDayFlowAccepted);
}

void ANightCourseHost::ApplyPostResultFlow(bool bDayFlowAccepted, bool bManualContinue)
{
	if (LastResult.bSuccess && !LastResult.bFailedMidway)
	{
		if (bDayFlowAccepted)
		{
			TravelToDay();
		}
		return;
	}

	// 手动"重试"按钮复用 continue 通道进入此处；必须能直接重启，不应被 bAutoRetryOnFailure 卡住。
	// bAutoRetryOnFailure 仅控制"无操作自动重开"，与玩家主动点击的重试解耦。
	if (!LastResult.bSuccess
		&& Director
		&& Director->DidEnterRuntimeCourse()
		&& bDayFlowAccepted
		&& (bAutoRetryOnFailure || bManualContinue))
	{
		if (UWorld* World = GetWorld())
		{
			const float RetryDelay = bManualContinue ? 0.f : FMath::Max(0.05f, AutoRetryDelaySeconds);
			World->GetTimerManager().SetTimer(
				RetryTimer,
				this,
				&ANightCourseHost::RetryAfterFailure,
				RetryDelay,
				false);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=Flow] Gameplay failure accepted; retry scheduled (%s)."),
				bManualContinue ? TEXT("manual") : TEXT("auto"));
		}
	}
}

void ANightCourseHost::HandleNightResultContinueRequested()
{
	ContinueAfterNightResult();
}

void ANightCourseHost::ContinueAfterNightResult()
{
	if (!bAwaitingResultContinue)
	{
		return;
	}

	const bool bDayFlowAccepted = bPendingDayFlowAccepted;
	ClearPendingResultPresentation(true);
	ApplyPostResultFlow(bDayFlowAccepted, /*bManualContinue=*/true);
}

void ANightCourseHost::ClearPendingResultPresentation(bool bHideHUD)
{
	bAwaitingResultContinue = false;
	bPendingDayFlowAccepted = false;
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (ANightCourseHUD* NightHUD = Cast<ANightCourseHUD>(PC->GetHUD()))
		{
			NightHUD->OnNightResultContinueRequested.RemoveDynamic(
				this,
				&ANightCourseHost::HandleNightResultContinueRequested);
			if (bHideHUD)
			{
				NightHUD->HideNightResult();
			}
		}
	}
}

void ANightCourseHost::RetryAfterFailure()
{
	USChefGameInstance* GameInstance =
		bUseChefDayFlow ? GetGameInstance<USChefGameInstance>() : nullptr;
	if (GameInstance)
	{
		if (GameInstance->Phase != ESGamePhase::NightRunning
			&& !GameInstance->StartNight())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=Flow] Automatic retry could not re-enter NightRunning."));
			return;
		}
		if (GameInstance->Phase == ESGamePhase::NightRunning)
		{
			Bootstrap = NightCourseStage_Private::MakeNightBootstrap(
				GameInstance->GetPendingNightBootstrap());
		}
	}

	FString Error;
	if (!TryStartCourse(Error))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Flow] Automatic retry failed to start the course: %s."),
			Error.IsEmpty() ? TEXT("<empty>") : *Error);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=Flow] Automatic retry started with seed=%d."),
			Bootstrap.Seed);
	}
}

void ANightCourseHost::TravelToDay()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bTravelToDayOnSuccess)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=Flow] Day travel is disabled on Host='%s'."),
			*GetNameSafe(this));
		return;
	}

	FSoftObjectPath DayLevelPath = SuccessDayLevel.ToSoftObjectPath();
	FSoftObjectPath DayGameModePath = SuccessDayGameMode.ToSoftObjectPath();
	bool bUsingGameModeFallback = false;
	if (DayLevelPath.IsNull())
	{
		const ANightCourseGameMode* NightGameMode =
			Cast<ANightCourseGameMode>(World->GetAuthGameMode());
		if (NightGameMode
			&& NightGameMode->bTravelToDayOnSuccess
			&& !NightGameMode->SuccessDayLevel.IsNull())
		{
			DayLevelPath = NightGameMode->SuccessDayLevel.ToSoftObjectPath();
			if (DayGameModePath.IsNull())
			{
				DayGameModePath = NightGameMode->SuccessDayGameMode.ToSoftObjectPath();
			}
			bUsingGameModeFallback = true;
		}
	}

	const FString DayLevelPackage = DayLevelPath.GetLongPackageName();
	if (DayLevelPackage.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=Flow] No Day level configured. Set Host='%s' SuccessDayLevel (and optionally SuccessDayGameMode) in the Blueprint or level instance."),
			*GetNameSafe(this));
		return;
	}

	FString Options;
	if (!DayGameModePath.IsNull())
	{
		Options = FString::Printf(
			TEXT("?game=%s"),
			*DayGameModePath.ToString());
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=Flow] Night succeeded; opening Day level='%s' gameMode='%s' source=%s."),
		*DayLevelPackage,
		DayGameModePath.IsNull() ? TEXT("<map default>") : *DayGameModePath.ToString(),
		bUsingGameModeFallback ? TEXT("GameMode fallback") : TEXT("Host config"));
	UGameplayStatics::OpenLevel(
		this,
		FName(*DayLevelPackage),
		true,
		Options);
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
