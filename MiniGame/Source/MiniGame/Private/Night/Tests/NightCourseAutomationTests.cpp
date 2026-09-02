#include "Misc/AutomationTest.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightCourseForkAtomActor.h"
#include "Night/Course/NightCourseAtomRouteData.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseRuleData.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "Night/Course/NightCourseRoadsideActor.h"
#include "Night/Course/NightForkController.h"
#include "Night/Course/NightRouteRules.h"
#include "Night/Course/NightTrackGenerator.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

#if WITH_AUTOMATION_TESTS && !MINIGAME_DEFER_AUTOMATION_TESTS

namespace NightCourseAutomation_Private
{
	static FNightRuleAtomEntry MakeEntry(
		const ENightNodeKind Action0,
		const ENightNodeKind Action1,
		const ENightNodeKind Action2,
		const int32 Weight = 1)
	{
		FNightRuleAtomEntry Entry;
		Entry.Actions = { Action0, Action1, Action2 };
		Entry.Weight = Weight;
		return Entry;
	}

	static TSoftClassPtr<ANightCourseAtomActor> MakeAtomClass(const TCHAR* AssetPath)
	{
		return TSoftClassPtr<ANightCourseAtomActor>(FSoftObjectPath(AssetPath));
	}

	static void AddBranchQueue(
		UNightCourseRuleData* Rule,
		const ENightRouteId RouteId,
		const FNightRuleAtomEntry& First,
		const FNightRuleAtomEntry& Second,
		const int32 TargetAtomCount = 0)
	{
		FNightRuleAtomQueue Queue;
		Queue.Atoms = { First, Second };
		Queue.TargetAtomCount = TargetAtomCount;
		Rule->BranchRoutes.Add(RouteId, MoveTemp(Queue));
	}

	static UNightG1CourseConfig* MakeAtomConfig(
		UObject* Outer,
		const bool bEnableFork,
		const int32 RuleSeed = 1001)
	{
		UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
		UNightG1CourseConfig* Config =
			NewObject<UNightG1CourseConfig>(EffectiveOuter);
		Config->bEnableFork = bEnableFork;
		Config->BranchEnterBufferSeconds = 0.f;
		Config->ExitBufferSeconds = 0.f;
		Config->bKeySwapOnlyOnRouteC = false;
		Config->bEnableCourseTips = false;
		Config->bSwapControlsOnEnterRouteC = false;
		Config->DefaultFoeId = EFoeId::M01;
		Config->FoeActorMap.Add(
			EFoeId::M01,
			TSoftClassPtr<ANightCourseStoneActor>(
				ANightCourseStoneActor::StaticClass()));
		Config->FoeDropMap.Add(EFoeId::M01, EIngredientId::F01_LingGu);
		Config->FoeDropMap.Add(EFoeId::M02, EIngredientId::F02_YinShanJun);
		Config->FoeDropMap.Add(EFoeId::M03, EIngredientId::F03_ChiYanJiao);
		Config->FoeDropMap.Add(EFoeId::M04, EIngredientId::F04_YueLinYu);
		Config->FoeDropMap.Add(EFoeId::M05, EIngredientId::F05_XuanYuQin);

		UNightCourseAtomRouteData* AtomRoute =
			NewObject<UNightCourseAtomRouteData>(Config);
		AtomRoute->bEnabled = true;
		AtomRoute->TransitionJumpGapCm = 520.f;
		AtomRoute->AtomMap.Add(
			TEXT("A"),
			MakeAtomClass(TEXT(
				"/Game/Night/Course/Atoms/BP_NightAtom_Art_A.BP_NightAtom_Art_A_C")));
		AtomRoute->AtomMap.Add(
			TEXT("B"),
			MakeAtomClass(TEXT(
				"/Game/Night/Course/Atoms/BP_NightAtom_Art_B.BP_NightAtom_Art_B_C")));
		AtomRoute->AtomMap.Add(
			TEXT("C"),
			MakeAtomClass(TEXT(
				"/Game/Night/Course/Atoms/BP_NightAtom_Art_C.BP_NightAtom_Art_C_C")));
		Config->AtomRoute = AtomRoute;

		UNightCourseRuleData* Rule = NewObject<UNightCourseRuleData>(Config);
		Rule->bEnabled = true;
		Rule->bAutoSelectAtomKeys = true;
		FNightRuleAtomQueue MainQueue;
		MainQueue.Atoms = {
			MakeEntry(ENightNodeKind::Hazard, ENightNodeKind::Enemy, ENightNodeKind::Hazard),
			MakeEntry(ENightNodeKind::Enemy, ENightNodeKind::Hazard, ENightNodeKind::Enemy)
		};
		Rule->RouteModes.Add(ENightRouteId::A, MainQueue);
		Rule->RouteModes.Add(ENightRouteId::B, MainQueue);
		Rule->RouteModes.Add(ENightRouteId::C, MainQueue);
		if (bEnableFork)
		{
			AddBranchQueue(
				Rule,
				ENightRouteId::A,
				MakeEntry(ENightNodeKind::Hazard, ENightNodeKind::Enemy, ENightNodeKind::Hazard),
				MakeEntry(ENightNodeKind::Enemy, ENightNodeKind::Hazard, ENightNodeKind::Enemy));
			AddBranchQueue(
				Rule,
				ENightRouteId::B,
				MakeEntry(ENightNodeKind::Enemy, ENightNodeKind::Enemy, ENightNodeKind::Hazard),
				MakeEntry(ENightNodeKind::Hazard, ENightNodeKind::Hazard, ENightNodeKind::Enemy));
			AddBranchQueue(
				Rule,
				ENightRouteId::C,
				MakeEntry(ENightNodeKind::Hazard, ENightNodeKind::Hazard, ENightNodeKind::Enemy),
				MakeEntry(ENightNodeKind::Enemy, ENightNodeKind::Hazard, ENightNodeKind::Hazard));
			Config->ForkAfterBaseAtomIndex = 2;
		}
		Config->CourseRuleData = Rule;

		UNightRouteRulesAsset* RouteRules = NewObject<UNightRouteRulesAsset>(Config);
		RouteRules->Rows = {
			UNightRouteRulesAsset::MakeDefaultRule(ENightRouteId::A),
			UNightRouteRulesAsset::MakeDefaultRule(ENightRouteId::B),
			UNightRouteRulesAsset::MakeDefaultRule(ENightRouteId::C)
		};
		Config->RouteRules = RouteRules;
		return Config;
	}

	static FNightRoadsideGenerationSettings MakeRoadsideSettings(
		const float SpacingCm,
		const float LeftOffsetCm,
		const float RightOffsetCm,
		const int32 SeedOffset)
	{
		FNightRoadsideGenerationSettings Settings;
		Settings.bEnabled = true;
		Settings.SpacingCm = SpacingCm;
		Settings.LeftBridgeOffsetCm = LeftOffsetCm;
		Settings.RightBridgeOffsetCm = RightOffsetCm;
		Settings.RandomSeedOffset = SeedOffset;
		FNightRoadsideBlueprintEntry Entry;
		Entry.Blueprint = TSoftClassPtr<ANightRoadsideSegmentActor>(
			ANightRoadsideSegmentActor::StaticClass());
		Entry.Weight = 1.f;
		Settings.BlueprintPool.Add(Entry);
		return Settings;
	}

	static void MakeStraightRoadsideTestCourse(
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBridgeSpec>& OutBridges)
	{
		OutStones.Reset();
		OutBridges.Reset();
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FNightStoneSpec Stone;
			Stone.bUseWorldPose = true;
			Stone.WorldLocation =
				FVector(Index * 1000.f, Index * 200.f, Index * 120.f);
			OutStones.Add(Stone);
		}

		FNightBridgeSpec FirstBridge;
		FirstBridge.FromStoneIndex = 0;
		FirstBridge.ToStoneIndex = 1;
		FirstBridge.WorldLocation = FVector(500.f, 100.f, 60.f);
		FirstBridge.YawDeg = 0.f;
		OutBridges.Add(FirstBridge);

		FNightBridgeSpec LastBridge;
		LastBridge.FromStoneIndex = 2;
		LastBridge.ToStoneIndex = 3;
		LastBridge.WorldLocation = FVector(2500.f, 500.f, 300.f);
		LastBridge.YawDeg = 0.f;
		OutBridges.Add(LastBridge);
	}

	static TArray<FString> CaptureAtomKeys(
		const TArray<FNightAtomVisualBinding>& Bindings)
	{
		TArray<FString> Keys;
		int32 LastAtomSlotIndex = INDEX_NONE;
		for (const FNightAtomVisualBinding& Binding : Bindings)
		{
			if (Binding.AtomSlotIndex == LastAtomSlotIndex)
			{
				continue;
			}
			LastAtomSlotIndex = Binding.AtomSlotIndex;
			Keys.Add(Binding.AtomKey);
		}
		return Keys;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseFloorActorResolutionTest,
	"MiniGame.Night.Course.FloorActorResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseFloorActorResolutionTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(
		nullptr,
		UWorld::StaticClass(),
		TEXT("NightCourseFloorResolutionWorld"),
		EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		WorldName,
		GetTransientPackage());
	TestNotNull(TEXT("transient floor-resolution world is created"), World);
	if (!World)
	{
		GEngine->DestroyWorldContext(World);
		return false;
	}
	World->AddToRoot();
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	AActor* Owner = World->SpawnActor<AActor>();
	UNightCourseDirector* Director = NewObject<UNightCourseDirector>(Owner);
	Owner->AddInstanceComponent(Director);
	Director->RegisterComponent();
	UNightG1CourseConfig* Config = NewObject<UNightG1CourseConfig>(Director);
	Director->Config = Config;

	AStaticMeshActor* ConfiguredTagActor = World->SpawnActor<AStaticMeshActor>();
	ConfiguredTagActor->Tags.Add(Config->FloorMeshActorTag);

	AStaticMeshActor* LegacyTagActor = World->SpawnActor<AStaticMeshActor>();
	LegacyTagActor->Tags.Add(Config->FloorMeshActorName);

	FActorSpawnParameters LegacyNameParams;
	LegacyNameParams.Name = Config->FloorMeshActorName;
	AStaticMeshActor* LegacyNameActor =
		World->SpawnActor<AStaticMeshActor>(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			LegacyNameParams);

	TestNotNull(TEXT("configured-tag actor is spawned"), ConfiguredTagActor);
	TestNotNull(TEXT("legacy-tag actor is spawned"), LegacyTagActor);
	TestNotNull(TEXT("legacy-name actor is spawned"), LegacyNameActor);
	TestTrue(
		TEXT("configured Actor Tag takes priority over legacy matches"),
		Director->ResolveCourseFloorMeshActor() == ConfiguredTagActor);

	Director->ManagedFloorMeshActor.Reset();
	AStaticMeshActor* DuplicateTagActor = World->SpawnActor<AStaticMeshActor>();
	DuplicateTagActor->Tags.Add(Config->FloorMeshActorTag);
	AddExpectedError(
		TEXT("Expected exactly one actor for Actor Tag"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestNull(
		TEXT("duplicate configured Actor Tags are rejected"),
		Director->ResolveCourseFloorMeshActor());

	Director->UnregisterComponent();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseRuleQueueJsonTest,
	"MiniGame.Night.Course.RuleQueueJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseRuleQueueJsonTest::RunTest(const FString& Parameters)
{
	UNightCourseRuleData* Rule =
		NewObject<UNightCourseRuleData>(GetTransientPackage());
	const FString Json = TEXT(R"JSON(
		{
			"seed": 42,
			"routeModes": {
				"A": {
					"targetAtomCount": 8,
					"atoms": [{"atomKey":"A","actions":["Jump","Kill"],"weight":5}]
				}
			},
			"branchRoutes": {
				"A": {
					"targetAtomCount": 6,
					"atoms": [{"atomKey":"A","actions":["Kill","Jump"],"weight":3}]
				},
				"B": {
					"targetAtomCount": 4,
					"atoms": [{"atomKey":"B","actions":["Jump","Jump"]}]
				},
				"C": {
					"targetAtomCount": 5,
					"atoms": [{"atomKey":"A","actions":["Kill","Kill"]}]
				}
			},
			"forkAfterBaseAtomIndex": 1,
			"autoSelectAtomKeys": false
		}
	)JSON");

	FString Error;
	TestTrue(TEXT("branch queue JSON imports"), Rule->ImportJson(Json, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestTrue(TEXT("route mode target atom count is imported"), Rule->GetRouteModeLength(ENightRouteId::A) == 8);
	TestTrue(TEXT("A branch is populated"), Rule->HasBranchRoute(ENightRouteId::A));
	TestTrue(TEXT("B branch is populated"), Rule->HasBranchRoute(ENightRouteId::B));
	TestTrue(TEXT("C branch is populated"), Rule->HasBranchRoute(ENightRouteId::C));
	TestTrue(TEXT("duplicate atom keys remain reusable"), Rule->BranchRoutes[ENightRouteId::A].Atoms[0].AtomKey == TEXT("A"));
	TestTrue(TEXT("route mode weight is imported"), Rule->RouteModes[ENightRouteId::A].Atoms[0].Weight == 5);
	TestTrue(TEXT("branch target atom count is imported"), Rule->BranchRoutes[ENightRouteId::A].TargetAtomCount == 6);
	TestTrue(TEXT("rule validates"), Rule->ValidateRule(Error));
	FString ExportedJson;
	TestTrue(TEXT("weighted queue JSON exports"), Rule->ExportJson(ExportedJson, Error));
	TestTrue(TEXT("export uses routeModes"), ExportedJson.Contains(TEXT("\"routeModes\"")));
	TestFalse(TEXT("export removes legacy baseRoute"), ExportedJson.Contains(TEXT("\"baseRoute\"")));
	TestTrue(TEXT("export includes route mode target count"), ExportedJson.Contains(TEXT("\"targetAtomCount\"")));
	TestTrue(TEXT("export includes branch target count"), ExportedJson.Contains(TEXT("\"targetAtomCount\"")));

	UNightCourseRuleData* LegacyRule =
		NewObject<UNightCourseRuleData>(GetTransientPackage());
	const FString LegacyJson = TEXT(R"JSON(
		{
			"seed": 7,
			"baseAtomCount": 3,
			"baseRoute": [{"atomKey":"A","actions":["Jump","Kill"]}]
		}
	)JSON");
	Error.Reset();
	TestTrue(TEXT("legacy baseRoute JSON imports for migration"), LegacyRule->ImportJson(LegacyJson, Error));
	TestTrue(
		TEXT("legacy baseRoute migrates to RouteModes A"),
		LegacyRule->GetRouteModeLength(ENightRouteId::A) == 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseForkPairTest,
	"MiniGame.Night.Course.ForkPairResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseForkPairTest::RunTest(const FString& Parameters)
{
	ENightRouteId Left = ENightRouteId::None;
	ENightRouteId Right = ENightRouteId::None;
	bool bForcedAB = false;
	UNightForkController::ResolvePairRoutes(
		ENightForkPair::AC,
		Left,
		Right,
		bForcedAB);
	TestEqual(TEXT("AC left route"), Left, ENightRouteId::A);
	TestEqual(TEXT("AC right route"), Right, ENightRouteId::C);
	TestFalse(TEXT("AC is not forced AB"), bForcedAB);

	UNightForkController::ResolvePairRoutes(
		ENightForkPair::BC,
		Left,
		Right,
		bForcedAB);
	TestEqual(TEXT("BC left route"), Left, ENightRouteId::B);
	TestEqual(TEXT("BC right route"), Right, ENightRouteId::C);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseForkAtomCompositionTest,
	"MiniGame.Night.Course.ForkAtomComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseForkAtomCompositionTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, true, 441);
	Config->PreviewForkPair = ENightForkPair::AB;
	Config->ForkAtomMap.Add(
		ENightForkPair::AB,
		TSoftClassPtr<ANightCourseForkAtomActor>(
			ANightCourseForkAtomActor::StaticClass()));

	FString ValidationError;
	TestTrue(
		TEXT("configured fork Atom map validates"),
		Config->ValidateForkAtomMap(ValidationError));
	if (!ValidationError.IsEmpty())
	{
		AddError(ValidationError);
	}

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	UBoxComponent* ForkBounds =
		NewObject<UBoxComponent>(GetTransientPackage());
	ForkBounds->SetBoxExtent(FVector(100000.f, 600.f, 10000.f));
	Director->SetLayoutBoundsComponent(ForkBounds, true);

	TArray<FNightStoneSpec> BaseStones;
	TArray<FNightBeatSpec> BaseBeats;
	TArray<FNightBridgeSpec> BaseBridges;
	TArray<FNightAtomVisualBinding> BaseBindings;
	TArray<FNightForkAtomSpec> BaseForkAtoms;
	Config->PreviewRoute = ENightRouteId::None;
	TestTrue(
		TEXT("base preview includes the configured fork Atom"),
		Director->BuildCourseForPreview(
			BaseStones,
			BaseBeats,
			BaseBridges,
			BaseBindings,
			BaseForkAtoms));
	TestEqual(TEXT("one special fork Atom is resolved"), BaseForkAtoms.Num(), 1);
	if (BaseForkAtoms.Num() == 1)
	{
		TestTrue(
			TEXT("fork Atom class is preserved"),
			BaseForkAtoms[0].ActorClass == ANightCourseForkAtomActor::StaticClass());
		TestFalse(
			TEXT("fork exits are distinct"),
			BaseForkAtoms[0].LeftExitTransform.GetLocation().Equals(
				BaseForkAtoms[0].RightExitTransform.GetLocation()));
		TestTrue(
			TEXT("fork entry is reset to world Y zero"),
			FMath::IsNearlyZero(
				BaseForkAtoms[0].WorldTransform.GetLocation().Y,
				0.01f));
	}

	auto BuildPreviewRoute =
		[&Director, &Config](
			const ENightRouteId RouteId,
			TArray<FNightStoneSpec>& OutStones,
			TArray<FNightForkAtomSpec>& OutForkAtoms)
		{
			Config->PreviewRoute = RouteId;
			TArray<FNightBeatSpec> Beats;
			TArray<FNightBridgeSpec> Bridges;
			TArray<FNightAtomVisualBinding> Bindings;
			return Director->BuildCourseForPreview(
				OutStones,
				Beats,
				Bridges,
				Bindings,
				OutForkAtoms);
		};

	TArray<FNightStoneSpec> AStones;
	TArray<FNightForkAtomSpec> AForkAtoms;
	TestTrue(
		TEXT("left route preview builds from the left fork exit"),
		BuildPreviewRoute(ENightRouteId::A, AStones, AForkAtoms));

	TArray<FNightStoneSpec> BStones;
	TArray<FNightForkAtomSpec> BForkAtoms;
	TestTrue(
		TEXT("right route preview builds from the right fork exit"),
		BuildPreviewRoute(ENightRouteId::B, BStones, BForkAtoms));
	if (BaseStones.Num() < AStones.Num()
		&& BaseStones.Num() < BStones.Num()
		&& AForkAtoms.Num() == 1
		&& BForkAtoms.Num() == 1)
	{
		const FVector ABranchStart = AStones[BaseStones.Num()].WorldLocation;
		const FVector BBranchStart = BStones[BaseStones.Num()].WorldLocation;
		TestTrue(
			TEXT("A branch begins nearer the configured left exit"),
			FVector::DistSquared(
				ABranchStart,
				AForkAtoms[0].LeftExitTransform.GetLocation())
			< FVector::DistSquared(
				ABranchStart,
				AForkAtoms[0].RightExitTransform.GetLocation()));
		TestTrue(
			TEXT("B branch begins nearer the configured right exit"),
			FVector::DistSquared(
				BBranchStart,
				BForkAtoms[0].RightExitTransform.GetLocation())
			< FVector::DistSquared(
				BBranchStart,
				BForkAtoms[0].LeftExitTransform.GetLocation()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseProcSeedTest,
	"MiniGame.Night.Course.ProcSeedDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseProcSeedTest::RunTest(const FString& Parameters)
{
	FNightProcCourseParams Params;
	Params.Seed = 777;
	Params.TotalNodes = 12;
	const FNightGeneratedCourse First = UNightTrackGenerator::GenerateBaseOnly(
		Params,
		FVector::ZeroVector,
		FVector::ForwardVector);
	const FNightGeneratedCourse Second = UNightTrackGenerator::GenerateBaseOnly(
		Params,
		FVector::ZeroVector,
		FVector::ForwardVector);

	TestEqual(TEXT("same seed stone count"), First.Stones.Num(), Second.Stones.Num());
	TestEqual(TEXT("same seed beat count"), First.Beats.Num(), Second.Beats.Num());
	for (int32 Index = 0; Index < FMath::Min(First.Stones.Num(), Second.Stones.Num()); ++Index)
	{
		TestTrue(
			FString::Printf(TEXT("same seed stone %d pose"), Index),
			First.Stones[Index].WorldLocation.Equals(Second.Stones[Index].WorldLocation)
			&& FMath::IsNearlyEqual(
				First.Stones[Index].YawDeg,
				Second.Stones[Index].YawDeg));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseAtomSeedSelectionTest,
	"MiniGame.Night.Course.AtomSeedSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseAtomSeedSelectionTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* FirstConfig =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 1001);
	UNightCourseDirector* FirstDirector =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	FirstDirector->Config = FirstConfig;

	TArray<FNightStoneSpec> FirstStones;
	TArray<FNightBeatSpec> FirstBeats;
	TArray<FNightBridgeSpec> FirstBridges;
	TArray<FNightAtomVisualBinding> FirstBindings;
	TestTrue(
		TEXT("first Seed builds a canonical Atom course"),
		FirstDirector->BuildCourseForPreview(
			FirstStones,
			FirstBeats,
			FirstBridges,
			FirstBindings));
	const TArray<FString> FirstKeys =
		NightCourseAutomation_Private::CaptureAtomKeys(FirstBindings);

	TArray<FNightStoneSpec> SameSeedStones;
	TArray<FNightBeatSpec> SameSeedBeats;
	TArray<FNightBridgeSpec> SameSeedBridges;
	TArray<FNightAtomVisualBinding> SameSeedBindings;
	TestTrue(
		TEXT("same Seed rebuild succeeds"),
		FirstDirector->BuildCourseForPreview(
			SameSeedStones,
			SameSeedBeats,
			SameSeedBridges,
			SameSeedBindings));
	TestEqual(
		TEXT("same Seed selects the same Atom keys"),
		NightCourseAutomation_Private::CaptureAtomKeys(SameSeedBindings),
		FirstKeys);
	TestEqual(TEXT("same Seed keeps stone count"), SameSeedStones.Num(), FirstStones.Num());
	if (SameSeedStones.Num() == FirstStones.Num())
	{
		for (int32 Index = 0; Index < FirstStones.Num(); ++Index)
		{
			TestTrue(
				FString::Printf(TEXT("same Seed keeps stone %d transform"), Index),
				SameSeedStones[Index].WorldLocation.Equals(FirstStones[Index].WorldLocation)
				&& FMath::IsNearlyEqual(
					SameSeedStones[Index].YawDeg,
					FirstStones[Index].YawDeg));
		}
	}

	bool bDifferentSeedChangedSelection = false;
	for (int32 CandidateSeed = 1002; CandidateSeed < 1100; ++CandidateSeed)
	{
		TArray<FNightStoneSpec> CandidateStones;
		TArray<FNightBeatSpec> CandidateBeats;
		TArray<FNightBridgeSpec> CandidateBridges;
		TArray<FNightAtomVisualBinding> CandidateBindings;
		if (!FirstDirector->BuildCourseForPreview(
			CandidateStones,
			CandidateBeats,
			CandidateBridges,
			CandidateBindings))
		{
			continue;
		}
		if (NightCourseAutomation_Private::CaptureAtomKeys(CandidateBindings) != FirstKeys)
		{
			bDifferentSeedChangedSelection = true;
			break;
		}
	}
	TestTrue(
		TEXT("different Seed selects a different valid combination when candidates exist"),
		bDifferentSeedChangedSelection);
	FirstConfig->CourseRuleData->RouteModes[ENightRouteId::A].Atoms[0].AtomKey = TEXT("B");
	TArray<FNightStoneSpec> ExplicitStones;
	TArray<FNightBeatSpec> ExplicitBeats;
	TArray<FNightBridgeSpec> ExplicitBridges;
	TArray<FNightAtomVisualBinding> ExplicitBindings;
	TestTrue(
		TEXT("explicit AtomKey build succeeds"),
		FirstDirector->BuildCourseForPreview(
			ExplicitStones,
			ExplicitBeats,
			ExplicitBridges,
			ExplicitBindings));
	const TArray<FString> ExplicitKeys =
		NightCourseAutomation_Private::CaptureAtomKeys(ExplicitBindings);
	TestTrue(
		TEXT("explicit AtomKey remains locked"),
		ExplicitKeys.Num() > 0 && ExplicitKeys[0] == TEXT("B"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseAtomBranchRebuildDeterminismTest,
	"MiniGame.Night.Course.AtomBranchRebuildDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseAtomBranchRebuildDeterminismTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, true, 1207);
	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	FNightBootstrap Bootstrap;
	Bootstrap.Seed = 1207;
	Bootstrap.ForkPair = ENightForkPair::AB;
	Director->StartNight(Bootstrap);

	bool bReachedFork = false;
	for (int32 Guard = 0; Guard < 128 && Director->IsRunning(); ++Guard)
	{
		if (Director->IsForkChoiceActive())
		{
			bReachedFork = true;
			Director->ChooseForkRight();
			break;
		}
		if (Director->IsAwaitingInput())
		{
			Director->NotifyFeelResolved(
				Director->GetActiveNodeIndex(),
				ENightJudgeOutcome::Success);
		}
	}
	TestTrue(TEXT("Atom course reaches its configured fork"), bReachedFork);

	TArray<FNightStoneSpec> FirstBranchStones;
	TArray<FNightBeatSpec> FirstBranchBeats;
	TArray<FNightBridgeSpec> FirstBranchBridges;
	TArray<FNightAtomVisualBinding> FirstBranchBindings;
	TArray<FNightStoneSpec> SecondBranchStones;
	TArray<FNightBeatSpec> SecondBranchBeats;
	TArray<FNightBridgeSpec> SecondBranchBridges;
	TArray<FNightAtomVisualBinding> SecondBranchBindings;
	TestTrue(
		TEXT("first branch rebuild succeeds"),
		Director->BuildCourseForPreview(
			FirstBranchStones,
			FirstBranchBeats,
			FirstBranchBridges,
			FirstBranchBindings));
	TestTrue(
		TEXT("second branch rebuild succeeds"),
		Director->BuildCourseForPreview(
			SecondBranchStones,
			SecondBranchBeats,
			SecondBranchBridges,
			SecondBranchBindings));
	TestEqual(
		TEXT("branch rebuild keeps Atom selection stable"),
		NightCourseAutomation_Private::CaptureAtomKeys(SecondBranchBindings),
		NightCourseAutomation_Private::CaptureAtomKeys(FirstBranchBindings));
	TestEqual(
		TEXT("branch rebuild keeps stone transforms stable"),
		SecondBranchStones.Num(),
		FirstBranchStones.Num());
	Director->ResetCourse();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseWeightedAtomCountTest,
	"MiniGame.Night.Course.WeightedAtomCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseWeightedAtomCountTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 1401);
	UNightCourseRuleData* Rule = Config->CourseRuleData;
	Rule->bAutoSelectAtomKeys = false;
	Rule->RouteModes.Remove(ENightRouteId::B);
	Rule->RouteModes.Remove(ENightRouteId::C);
	Rule->RouteModes[ENightRouteId::A].TargetAtomCount = 30;
	Rule->RouteModes[ENightRouteId::A].Atoms = {
		NightCourseAutomation_Private::MakeEntry(
			ENightNodeKind::Hazard,
			ENightNodeKind::Enemy,
			ENightNodeKind::Hazard,
			5),
		NightCourseAutomation_Private::MakeEntry(
			ENightNodeKind::Enemy,
			ENightNodeKind::Enemy,
			ENightNodeKind::Enemy,
			3)
	};
	Rule->RouteModes[ENightRouteId::A].Atoms[0].AtomKey = TEXT("A");
	Rule->RouteModes[ENightRouteId::A].Atoms[1].AtomKey = TEXT("A");
	Rule->RouteModes[ENightRouteId::A].Atoms[0].Actions.Add(ENightNodeKind::Enemy);
	Rule->RouteModes[ENightRouteId::A].Atoms[1].Actions.Add(ENightNodeKind::Enemy);

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;

	TArray<FNightStoneSpec> Stones;
	TArray<FNightBeatSpec> Beats;
	TArray<FNightBridgeSpec> Bridges;
	TArray<FNightAtomVisualBinding> Bindings;
	TestTrue(
		TEXT("weighted base queue builds"),
		Director->BuildCourseForPreview(Stones, Beats, Bridges, Bindings));
	TestEqual(TEXT("target base atom count is generated"), Stones.Num(), 30 * 5);
	TestEqual(TEXT("internal and transition beats are generated"), Beats.Num(), 30 * 4 + 29);

	int32 HazardEnemyHazardCount = 0;
	int32 EnemyEnemyEnemyCount = 0;
	for (int32 AtomIndex = 0; AtomIndex < 30; ++AtomIndex)
	{
		const int32 StoneOffset = AtomIndex * 5;
		if (!Stones.IsValidIndex(StoneOffset + 4))
		{
			continue;
		}
		if (!Stones[StoneOffset + 1].bHasFoe
			&& Stones[StoneOffset + 2].bHasFoe
			&& !Stones[StoneOffset + 3].bHasFoe
			&& Stones[StoneOffset + 4].bHasFoe)
		{
			++HazardEnemyHazardCount;
		}
		else if (Stones[StoneOffset + 1].bHasFoe
			&& Stones[StoneOffset + 2].bHasFoe
			&& Stones[StoneOffset + 3].bHasFoe
			&& Stones[StoneOffset + 4].bHasFoe)
		{
			++EnemyEnemyEnemyCount;
		}
	}
	TestTrue(
		TEXT("weighted templates produce both authored action patterns"),
		HazardEnemyHazardCount > 0 && EnemyEnemyEnemyCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseAtomCompatibilityFailureTest,
	"MiniGame.Night.Course.IncompatibleAtomCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseAtomCompatibilityFailureTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 1301);
	Config->CourseRuleData->RouteModes[ENightRouteId::A].Atoms[0].Actions = {
		ENightNodeKind::Hazard,
		ENightNodeKind::Enemy
	};
	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	FString Error;
	AddExpectedError(
		TEXT("Route mode A has no compatible Atom BP candidate"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(
		TEXT("incompatible Atom candidates fail validation"),
		Director->ValidateConfiguration(Error));
	TestTrue(
		TEXT("incompatible Atom error explains the candidate mismatch"),
		Error.Contains(TEXT("compatible"))
		|| Error.Contains(TEXT("actions")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseCanonicalAssetReferenceTest,
	"MiniGame.Night.Course.CanonicalAssetReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseCanonicalAssetReferenceTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Course = LoadObject<UNightG1CourseConfig>(
		nullptr,
		TEXT("/Game/Night/Course/Config/DA_Course.DA_Course"));
	TestNotNull(TEXT("canonical Course asset exists"), Course);
	if (!Course)
	{
		return true;
	}
	TestTrue(TEXT("canonical Course has Atom library"), Course->AtomRoute != nullptr);
	TestTrue(TEXT("canonical Course has Rule data"), Course->CourseRuleData != nullptr);
	TestTrue(TEXT("canonical Course has Route rules"), Course->RouteRules != nullptr);
	if (Course->AtomRoute)
	{
		TestTrue(
			TEXT("Course points at canonical DA_Atoms"),
			Course->AtomRoute->GetPathName().StartsWith(
				TEXT("/Game/Night/Course/Config/DA_Atoms")));
	}
	if (Course->CourseRuleData)
	{
		TestTrue(
			TEXT("Course points at canonical DA_Rules"),
			Course->CourseRuleData->GetPathName().StartsWith(
				TEXT("/Game/Night/Course/Config/DA_Rules")));
	}
	if (Course->RouteRules)
	{
		TestTrue(
			TEXT("Course points at canonical DA_RouteRules"),
			Course->RouteRules->GetPathName().StartsWith(
				TEXT("/Game/Night/Course/Config/DA_RouteRules")));
	}
	FString FoeMapError;
	TestTrue(
		TEXT("canonical Course foe actor map is valid"),
		Course->ValidateFoeActorMap(FoeMapError));
	TestTrue(TEXT("canonical Course maps M01"), Course->FoeActorMap.Contains(EFoeId::M01));
	TestTrue(TEXT("canonical Course maps M02"), Course->FoeActorMap.Contains(EFoeId::M02));
	TestTrue(TEXT("canonical Course maps M03"), Course->FoeActorMap.Contains(EFoeId::M03));
	TestTrue(TEXT("canonical Course maps M04"), Course->FoeActorMap.Contains(EFoeId::M04));
	TestTrue(TEXT("canonical Course maps M05"), Course->FoeActorMap.Contains(EFoeId::M05));
	TestEqual(TEXT("canonical Course maps exactly five foe IDs"), Course->FoeActorMap.Num(), 5);
	FString FoeDropMapError;
	TestTrue(
		TEXT("canonical Course foe drop map is valid"),
		Course->ValidateFoeDropMap(FoeDropMapError));
	TestTrue(TEXT("canonical Course drop maps M01"), Course->FoeDropMap.Contains(EFoeId::M01));
	TestTrue(TEXT("canonical Course drop maps M02"), Course->FoeDropMap.Contains(EFoeId::M02));
	TestTrue(TEXT("canonical Course drop maps M03"), Course->FoeDropMap.Contains(EFoeId::M03));
	TestTrue(TEXT("canonical Course drop maps M04"), Course->FoeDropMap.Contains(EFoeId::M04));
	TestTrue(TEXT("canonical Course drop maps M05"), Course->FoeDropMap.Contains(EFoeId::M05));
	TestEqual(TEXT("canonical Course maps exactly five foe drops"), Course->FoeDropMap.Num(), 5);
	if (Course->CourseRuleData)
	{
		TestTrue(
			TEXT("canonical Rules has route mode A"),
			Course->CourseRuleData->RouteModes.Contains(ENightRouteId::A));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseFoeActorMapValidationTest,
	"MiniGame.Night.Course.FoeActorMapValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseFoeActorMapValidationTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 1501);
	FString Error;
	TestTrue(
		TEXT("a mapped NightCourseStoneActor class is accepted"),
		Config->ValidateFoeActorMap(Error));

	Config->FoeActorMap.Remove(EFoeId::M01);
	Error.Reset();
	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	AddExpectedError(
		TEXT("FoeActorMap has no Blueprint for FoeId=1."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(
		TEXT("a selected foe with no map entry fails course validation"),
		Director->ValidateConfiguration(Error));
	TestTrue(
		TEXT("missing foe mapping reports the map"),
		Error.Contains(TEXT("FoeActorMap")));

	Config->FoeActorMap.Add(
		EFoeId::M01,
		TSoftClassPtr<ANightCourseStoneActor>(
			ANightCourseAtomActor::StaticClass()));
	Error.Reset();
	TestFalse(
		TEXT("a non-StoneActor class is rejected"),
		Config->ValidateFoeActorMap(Error));
	TestTrue(
		TEXT("invalid foe class reports the mapped foe entry"),
		Error.Contains(TEXT("FoeActorMap"))
		|| Error.Contains(TEXT("not an ANightCourseStoneActor")));

	Config->FoeActorMap.Add(
		EFoeId::M01,
		TSoftClassPtr<ANightCourseStoneActor>(
			ANightCourseStoneActor::StaticClass()));
	Config->DefaultFoeId = EFoeId::None;
	Error.Reset();
	AddExpectedError(
		TEXT("Course Config DefaultFoeId must be a mapped foe ID."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(
		TEXT("None cannot be used as the default foe ID"),
		Director->ValidateConfiguration(Error));
	TestTrue(
		TEXT("None default error is explicit"),
		Error.Contains(TEXT("DefaultFoeId")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseFoeDropMapTest,
	"MiniGame.Night.Course.FoeDropMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseFoeDropMapTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 1502);
	Config->DefaultDropCount = 3;
	Config->bRandomizeEnemyDrops = true;
	Config->IngredientDropPool = { EIngredientId::F05_XuanYuQin };
	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;

	TArray<FNightStoneSpec> Stones;
	TArray<FNightBeatSpec> Beats;
	TArray<FNightBridgeSpec> Bridges;
	TArray<FNightAtomVisualBinding> Bindings;
	TestTrue(
		TEXT("mapped foe drops build successfully"),
		Director->BuildCourseForPreview(Stones, Beats, Bridges, Bindings));
	for (const FNightStoneSpec& Stone : Stones)
	{
		if (!Stone.bHasFoe)
		{
			continue;
		}
		TestEqual(
			TEXT("FoeDropMap overrides the random ingredient pool"),
			Stone.DropId,
			EIngredientId::F01_LingGu);
		TestEqual(
			TEXT("mapped foe uses DefaultDropCount"),
			Stone.DropCount,
			3);
	}

	FString Error;
	Config->FoeDropMap.Remove(EFoeId::M05);
	TestFalse(
		TEXT("missing one of M01-M05 drop mappings is rejected"),
		Config->ValidateFoeDropMap(Error));
	TestTrue(TEXT("missing drop mapping reports FoeDropMap"), Error.Contains(TEXT("FoeDropMap")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseDefaultRouteModeTest,
	"MiniGame.Night.Course.DefaultRouteMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseDefaultRouteModeTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 1503);
	Config->PreviewDefaultRoute = ENightRouteId::B;
	Config->CourseRuleData->bAutoSelectAtomKeys = false;
	for (TPair<ENightRouteId, FNightRuleAtomQueue>& RoutePair :
		Config->CourseRuleData->RouteModes)
	{
		for (FNightRuleAtomEntry& Entry : RoutePair.Value.Atoms)
		{
			if (Entry.AtomKey.IsEmpty())
			{
				Entry.AtomKey = TEXT("B");
			}
		}
	}
	Config->CourseRuleData->RouteModes[ENightRouteId::A].Atoms[0].AtomKey = TEXT("A");
	Config->CourseRuleData->RouteModes[ENightRouteId::A].Atoms[0].Actions.Add(
		ENightNodeKind::Enemy);
	Config->CourseRuleData->RouteModes[ENightRouteId::B].Atoms[0].AtomKey = TEXT("B");
	Config->CourseRuleData->RouteModes[ENightRouteId::C].Atoms[0].AtomKey = TEXT("C");

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	TArray<FNightStoneSpec> Stones;
	TArray<FNightBeatSpec> Beats;
	TArray<FNightBridgeSpec> Bridges;
	TArray<FNightAtomVisualBinding> Bindings;
	TestTrue(
		TEXT("preview uses the selected RouteMode"),
		Director->BuildCourseForPreview(Stones, Beats, Bridges, Bindings));
	TestTrue(
		TEXT("RouteModes B produces a B Atom binding"),
		Bindings.Num() > 0 && Bindings[0].AtomKey == TEXT("B"));

	FNightBootstrap Bootstrap;
	Bootstrap.DefaultRoute = ENightRouteId::C;
	Director->StartNight(Bootstrap);
	TestTrue(TEXT("runtime accepts a Day-selected route mode"), Director->IsRunning());
	TestEqual(
		TEXT("runtime stores the Day-selected default route"),
		Director->GetActiveDefaultRoute(),
		ENightRouteId::C);
	Director->ResetCourse();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseLayoutBoundsFallbackTest,
	"MiniGame.Night.Course.LayoutBoundsFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseLayoutBoundsFallbackTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 1601);
	Config->TrackOrigin = FVector(0.f, 20000.f, 0.f);
	Config->TrackForward = FVector::ForwardVector;

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;

	UBoxComponent* Bounds =
		NewObject<UBoxComponent>(GetTransientPackage());
	Bounds->SetBoxExtent(FVector(10000.f, 10000.f, 10000.f));

	TArray<FNightStoneSpec> UnboundedStones;
	TArray<FNightBeatSpec> UnboundedBeats;
	TArray<FNightBridgeSpec> UnboundedBridges;
	TArray<FNightAtomVisualBinding> UnboundedVisualBindings;
	Director->SetLayoutBoundsComponent(Bounds, false);
	TestTrue(
		TEXT("baseline course is valid without bounds enforcement"),
		Director->BuildCourseForPreview(
			UnboundedStones,
			UnboundedBeats,
			UnboundedBridges,
			UnboundedVisualBindings));

	Director->SetLayoutBoundsComponent(Bounds, true);

	TArray<FNightStoneSpec> Stones;
	TArray<FNightBeatSpec> Beats;
	TArray<FNightBridgeSpec> Bridges;
	TArray<FNightAtomVisualBinding> VisualBindings;
	TestTrue(
		TEXT("course falls back to translating an out-of-bounds Atom"),
		Director->BuildCourseForPreview(
			Stones,
			Beats,
			Bridges,
			VisualBindings));
	for (const FNightStoneSpec& Stone : Stones)
	{
		TestTrue(
			TEXT("translated Atom stones remain inside the Y bounds"),
			FMath::Abs(Stone.WorldLocation.Y) <= 10000.f);
	}
	TestEqual(
		TEXT("Y-only fallback preserves the stone count"),
		Stones.Num(),
		UnboundedStones.Num());
	if (Stones.Num() == UnboundedStones.Num())
	{
		for (int32 StoneIndex = 0; StoneIndex < Stones.Num(); ++StoneIndex)
		{
			TestTrue(
				TEXT("Y-only fallback preserves X and Z"),
				FMath::IsNearlyEqual(
					Stones[StoneIndex].WorldLocation.X,
					UnboundedStones[StoneIndex].WorldLocation.X,
					0.01f)
				&& FMath::IsNearlyEqual(
					Stones[StoneIndex].WorldLocation.Z,
					UnboundedStones[StoneIndex].WorldLocation.Z,
					0.01f));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseRoadsideGenerationTest,
	"MiniGame.Night.Course.RoadsideGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseRoadsideGenerationTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NewObject<UNightG1CourseConfig>(GetTransientPackage());
	Config->HouseRoadside =
		NightCourseAutomation_Private::MakeRoadsideSettings(
			0.f,
			300.f,
			450.f,
			17);
	Config->PoleRoadside =
		NightCourseAutomation_Private::MakeRoadsideSettings(
			400.f,
			250.f,
			350.f,
			29);
	Config->PoleRoadside.RandomYawRangeDeg = 5.f;

	FString Error;
	TestTrue(
		TEXT("native roadside segment contract validates"),
		Config->ValidateRoadsideConfiguration(Error));

	TArray<FNightStoneSpec> Stones;
	TArray<FNightBridgeSpec> Bridges;
	NightCourseAutomation_Private::MakeStraightRoadsideTestCourse(
		Stones,
		Bridges);

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	TArray<FNightRoadsidePropSpec> FirstSpecs;
	TArray<FNightRoadsidePropSpec> SameSeedSpecs;
	TestTrue(
		TEXT("roadside specs build from stone and bridge path"),
		Director->BuildRoadsideSpecs(Stones, Bridges, FirstSpecs));
	TestTrue(
		TEXT("roadside generation produces both categories"),
		FirstSpecs.ContainsByPredicate(
			[](const FNightRoadsidePropSpec& Spec)
			{
				return Spec.Kind == ENightRoadsideKind::House;
			})
		&& FirstSpecs.ContainsByPredicate(
			[](const FNightRoadsidePropSpec& Spec)
			{
				return Spec.Kind == ENightRoadsideKind::Pole;
			}));
	TestTrue(
		TEXT("roadside specs reproduce with the same seed"),
		Director->BuildRoadsideSpecs(Stones, Bridges, SameSeedSpecs));
	TestEqual(
		TEXT("same seed produces the same roadside count"),
		SameSeedSpecs.Num(),
		FirstSpecs.Num());
	for (int32 Index = 0; Index < FirstSpecs.Num(); ++Index)
	{
		TestTrue(
			TEXT("same seed preserves roadside placement"),
			FirstSpecs[Index].Kind == SameSeedSpecs[Index].Kind
			&& FirstSpecs[Index].Side == SameSeedSpecs[Index].Side
			&& FirstSpecs[Index].PropClass == SameSeedSpecs[Index].PropClass
			&& FirstSpecs[Index].WorldTransform.Equals(
				SameSeedSpecs[Index].WorldTransform,
				0.01f));
	}

	const ANightRoadsideSegmentActor* Defaults =
		ANightRoadsideSegmentActor::StaticClass()
			->GetDefaultObject<ANightRoadsideSegmentActor>();
	FVector MarkerStart;
	FVector MarkerEnd;
	Defaults->GetRoadsideMarkerLocations(MarkerStart, MarkerEnd);

	auto TestContinuousHouseSide =
		[this, &FirstSpecs, MarkerStart, MarkerEnd](
			const int32 Side,
			const float ExpectedY,
			const TCHAR* Label)
	{
		FVector PreviousEnd = FVector::ZeroVector;
		bool bHasPrevious = false;
		int32 Count = 0;
		for (const FNightRoadsidePropSpec& Spec : FirstSpecs)
		{
			if (Spec.Kind != ENightRoadsideKind::House
				|| Spec.Side != Side)
			{
				continue;
			}
			const FVector Start =
				Spec.WorldTransform.TransformPosition(MarkerStart);
			const FVector End =
				Spec.WorldTransform.TransformPosition(MarkerEnd);
			TestTrue(
				Label,
				FMath::IsNearlyEqual(Start.Y, ExpectedY, 0.01f)
				&& FMath::IsNearlyEqual(End.Y, ExpectedY, 0.01f)
				&& FMath::IsNearlyEqual(Start.Z, 0.f, 0.01f)
				&& FMath::IsNearlyEqual(End.Z, 0.f, 0.01f)
				&& End.X > Start.X);
			if (!bHasPrevious)
			{
				bHasPrevious = true;
			}
			else
			{
				TestTrue(
					TEXT("house End marker meets the next Start marker"),
					PreviousEnd.Equals(Start, 0.01f));
			}
			PreviousEnd = End;
			++Count;
		}
		TestTrue(Label, Count > 1);
	};
	TestContinuousHouseSide(-1, -300.f, TEXT("left house row uses the configured offset"));
	TestContinuousHouseSide(1, 450.f, TEXT("right house row uses the configured offset"));
	for (const FNightRoadsidePropSpec& Spec : FirstSpecs)
	{
		if (Spec.Side == 1)
		{
			TestTrue(
				TEXT("every right roadside actor is mirrored on Y"),
				Spec.WorldTransform.GetScale3D().Y < -0.99f);
		}
	}

	TestTrue(
		TEXT("a no-bridge stone interval still receives roadside specs"),
		FirstSpecs.ContainsByPredicate(
			[](const FNightRoadsidePropSpec& Spec)
			{
				return Spec.PathSegmentIndex == 1;
			}));

	int32 PoleCount = 0;
	float PreviousPoleDistance = 0.f;
	bool bHasPreviousPole = false;
	for (const FNightRoadsidePropSpec& Spec : FirstSpecs)
	{
		if (Spec.Kind != ENightRoadsideKind::Pole || Spec.Side != 1)
		{
			continue;
		}
		if (bHasPreviousPole)
		{
			TestTrue(
				TEXT("pole spacing is independent from house spacing"),
				FMath::IsNearlyEqual(
					Spec.DistanceAlongPath - PreviousPoleDistance,
					500.f,
					0.01f));
		}
		PreviousPoleDistance = Spec.DistanceAlongPath;
		bHasPreviousPole = true;
		++PoleCount;
	}
	TestTrue(TEXT("right pole row contains multiple poles"), PoleCount > 1);

	Config->HouseRoadside.BlueprintPool[0].Blueprint =
		TSoftClassPtr<ANightRoadsideSegmentActor>(AActor::StaticClass());
	Error.Reset();
	TestFalse(
		TEXT("non-roadside Blueprint classes are rejected"),
		Config->ValidateRoadsideConfiguration(Error));
	TestTrue(
		TEXT("invalid roadside class error names the contract"),
		Error.Contains(TEXT("House roadside"))
		|| Error.Contains(TEXT("ANightRoadsideSegmentActor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseRouteRuleValidationTest,
	"MiniGame.Night.Course.RouteRuleValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseRouteRuleValidationTest::RunTest(const FString& Parameters)
{
	UNightRouteRulesAsset* Rules =
		NewObject<UNightRouteRulesAsset>(GetTransientPackage());
	FNightRouteRuleRow A = UNightRouteRulesAsset::MakeDefaultRule(ENightRouteId::A);
	FNightRouteRuleRow B = UNightRouteRulesAsset::MakeDefaultRule(ENightRouteId::B);
	FNightRouteRuleRow C = UNightRouteRulesAsset::MakeDefaultRule(ENightRouteId::C);
	Rules->Rows = { A, B, C };
	FString Error;
	TestTrue(TEXT("default A/B/C rows validate"), Rules->ValidateRules(Error));
	Rules->Rows[1].DropRhythmEveryN = 0;
	TestFalse(TEXT("invalid drop rhythm is rejected"), Rules->ValidateRules(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseDirectorProcLoopTest,
	"MiniGame.Night.Course.DirectorAtomForkLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseDirectorProcLoopTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, true, 321);
	Config->bKeySwapOnlyOnRouteC = false;
	FNightKeySwapCue Cue;
	Cue.TriggerAfterBranchBeats = 1;
	Cue.WarningSeconds = 0.01f;
	Cue.SafetyHoldSeconds = 0.01f;
	Config->KeySwapCues.Add(Cue);

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	FNightBootstrap Bootstrap;
	Bootstrap.Seed = 321;
	Bootstrap.ForkPair = ENightForkPair::AB;
	Director->StartNight(Bootstrap);
	TestTrue(TEXT("Atom course starts"), Director->IsRunning());

	bool bSelectedFork = false;
	for (int32 Guard = 0; Guard < 256 && Director->IsRunning(); ++Guard)
	{
		if (Director->IsForkChoiceActive())
		{
			Director->ChooseForkRight();
			bSelectedFork = true;
			break;
		}
		if (Director->IsAwaitingInput())
		{
			Director->NotifyFeelResolved(
				Director->GetActiveNodeIndex(),
				ENightJudgeOutcome::Success);
		}
	}

	TestTrue(TEXT("Atom course reaches a fork"), bSelectedFork);
	TestTrue(
		TEXT("branch composition enters the buffer without partial failure"),
		Director->GetPhase() == ENightCoursePhase::BranchEnterBuffer);
	TestEqual(TEXT("selected route is retained"), Director->GetCurrentRoute(), ENightRouteId::B);
	Director->ResetCourse();
	TestEqual(TEXT("reset returns to idle"), Director->GetPhase(), ENightCoursePhase::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseWrongInputTest,
	"MiniGame.Night.Course.WrongInputKeepsWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseWrongInputTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 101);
	Config->WrongPenalty = 7.f;

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	UNightFeelStubComponent* Feel =
		NewObject<UNightFeelStubComponent>(GetTransientPackage());
	Feel->Soul = 100.f;
	Director->BindFeelBridge(Feel);

	FNightBootstrap Bootstrap;
	Bootstrap.Seed = 101;
	Director->StartNight(Bootstrap);
	TestTrue(TEXT("course starts for wrong-input test"), Director->IsRunning());
	const int32 ActiveNode = Director->GetActiveNodeIndex();
	Director->NotifyFeelResolved(ActiveNode, ENightJudgeOutcome::WrongButton);

	TestTrue(TEXT("wrong input keeps the same judge window open"), Director->IsAwaitingInput());
	TestTrue(
		TEXT("wrong input uses Director penalty"),
		FMath::IsNearlyEqual(Feel->Soul, 93.f));
	Director->ResetCourse();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseFailureTransactionTest,
	"MiniGame.Night.Course.InvalidAtomConfigFailsTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseFailureTransactionTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NewObject<UNightG1CourseConfig>(GetTransientPackage());
	Config->CourseRuleData = NewObject<UNightCourseRuleData>(Config);
	Config->CourseRuleData->bEnabled = true;

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	AddExpectedError(
		TEXT("Canonical AtomRoute is missing or disabled."),
		EAutomationExpectedErrorFlags::Contains,
		3);
	Director->StartNight(FNightBootstrap());

	TestFalse(TEXT("invalid atom configuration does not start"), Director->IsRunning());
	TestEqual(
		TEXT("invalid atom configuration enters Failed"),
		Director->GetPhase(),
		ENightCoursePhase::Failed);
	Director->ResetCourse();
	TestEqual(TEXT("failed course can be reset"), Director->GetPhase(), ENightCoursePhase::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseSoulFailureCleanupTest,
	"MiniGame.Night.Course.SoulFailureAndRestartCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseSoulFailureCleanupTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 202);
	Config->WrongPenalty = 7.f;

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	UNightFeelStubComponent* Feel =
		NewObject<UNightFeelStubComponent>(GetTransientPackage());
	Feel->Soul = 5.f;
	Director->BindFeelBridge(Feel);

	AddExpectedError(
		TEXT("Soul reached zero after a wrong input."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	Director->StartNight(FNightBootstrap());
	Director->NotifyFeelResolved(
		Director->GetActiveNodeIndex(),
		ENightJudgeOutcome::WrongButton);

	TestTrue(TEXT("soul depletion enters Failed"), Director->IsCourseFailed());
	TestFalse(TEXT("failed course stops running"), Director->IsRunning());
	TestFalse(TEXT("failed course clears the judge request"), Feel->bHasActiveRequest);

	Director->ResetCourse();
	Feel->Soul = 100.f;
	Director->StartNight(FNightBootstrap());
	TestTrue(TEXT("reset course can start again"), Director->IsRunning());
	Director->ResetCourse();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseBranchFinishTest,
	"MiniGame.Night.Course.BranchFinishesWithRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseBranchFinishTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, true, 654);
	Config->bKeySwapOnlyOnRouteC = false;
	FNightKeySwapCue Cue;
	Cue.TriggerAfterBranchBeats = 1;
	Cue.WarningSeconds = 0.01f;
	Cue.SafetyHoldSeconds = 0.01f;
	Config->KeySwapCues.Add(Cue);

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	FNightBootstrap Bootstrap;
	Bootstrap.Seed = 654;
	Bootstrap.ForkPair = ENightForkPair::AB;
	Director->StartNight(Bootstrap);

	bool bReachedFork = false;
	for (int32 Guard = 0; Guard < 64 && Director->IsRunning(); ++Guard)
	{
		if (Director->IsForkChoiceActive())
		{
			bReachedFork = true;
			Director->ChooseForkRight();
			break;
		}
		if (Director->IsAwaitingInput())
		{
			Director->NotifyFeelResolved(
				Director->GetActiveNodeIndex(),
				ENightJudgeOutcome::Success);
		}
	}
	TestTrue(TEXT("branch finish test reaches fork"), bReachedFork);
	TestEqual(TEXT("right route is selected"), Director->GetCurrentRoute(), ENightRouteId::B);

	for (int32 Guard = 0; Guard < 64 && Director->IsRunning(); ++Guard)
	{
		if (Director->IsAwaitingInput())
		{
			Director->NotifyFeelResolved(
				Director->GetActiveNodeIndex(),
				ENightJudgeOutcome::Success);
		}
		else
		{
			Director->TickComponent(0.02f, LEVELTICK_All, nullptr);
		}
	}

	TestFalse(TEXT("branch course reaches a terminal state"), Director->IsRunning());
	TestEqual(
		TEXT("branch course finishes successfully"),
		Director->GetPhase(),
		ENightCoursePhase::Finished);
	TestEqual(TEXT("finished result keeps route"), Director->GetCurrentRoute(), ENightRouteId::B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseResultApiTest,
	"MiniGame.Night.Course.ResultApiAndDebugReason",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseResultApiTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 909);

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;

	FString Error;
	TestTrue(
		TEXT("TryStartNight accepts a valid configuration"),
		Director->TryStartNight(FNightBootstrap(), Error));
	TestTrue(TEXT("start error is empty"), Error.IsEmpty());
	TestTrue(TEXT("course is running after TryStartNight"), Director->IsRunning());

	Director->DebugForceFinish(true);
	TestTrue(TEXT("successful finish exposes a result"), Director->HasNightResult());
	TestTrue(TEXT("successful result is marked success"), Director->GetNightResult().bSuccess);
	TestFalse(
		TEXT("successful result is not marked as midway failure"),
		Director->GetNightResult().bFailedMidway);
	TestTrue(
		TEXT("successful result has no failure reason"),
		Director->GetLastFailureReason().IsEmpty());

	Director->ResetCourse();
	TestFalse(TEXT("reset invalidates the previous result"), Director->HasNightResult());
	TestTrue(
		TEXT("reset clears the previous failure reason"),
		Director->GetLastFailureReason().IsEmpty());

	TestTrue(
		TEXT("course can start again after result reset"),
		Director->TryStartNight(FNightBootstrap(), Error));
	Director->DebugForceFinish(false);
	TestTrue(TEXT("failed finish exposes a result"), Director->HasNightResult());
	TestFalse(TEXT("failed result is not marked success"), Director->GetNightResult().bSuccess);
	TestTrue(
		TEXT("failed result is marked as midway failure"),
		Director->GetNightResult().bFailedMidway);
	TestEqual(
		TEXT("debug failure reason is exposed"),
		Director->GetLastFailureReason(),
		FString(TEXT("DebugForceFinish(false).")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseGiftProtectionTest,
	"MiniGame.Night.Course.GiftProtectionNumbers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseGiftProtectionTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 1701);
	Config->WrongPenalty = 7.f;
	Config->StartingSoul = 100.f;

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	UNightFeelStubComponent* Feel =
		NewObject<UNightFeelStubComponent>(GetTransientPackage());
	Feel->Soul = 20.f;
	Director->BindFeelBridge(Feel);

	FNightBootstrap Bootstrap;
	Bootstrap.GiftBuffs.MatchShieldCharges = 1;
	Bootstrap.GiftBuffs.NearDeathHealAmount = 40.f;
	Bootstrap.GiftBuffs.NearDeathThreshold = 15.f;
	Director->StartNight(Bootstrap);
	TestTrue(TEXT("gift protection course starts"), Director->IsRunning());
	const int32 ActiveNode = Director->GetActiveNodeIndex();

	Director->NotifyFeelResolved(ActiveNode, ENightJudgeOutcome::WrongButton);
	TestTrue(
		TEXT("first judgement damage is blocked by the one-charge shield"),
		FMath::IsNearlyEqual(Feel->Soul, 20.f));

	Director->NotifyFeelResolved(ActiveNode, ENightJudgeOutcome::WrongButton);
	TestTrue(
		TEXT("second damage crosses threshold and consumes the one-time heal"),
		FMath::IsNearlyEqual(Feel->Soul, 53.f));

	Director->NotifyFeelResolved(ActiveNode, ENightJudgeOutcome::WrongButton);
	TestTrue(
		TEXT("near-death heal cannot trigger twice in one night"),
		FMath::IsNearlyEqual(Feel->Soul, 46.f));
	Director->ResetCourse();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCourseFloatGatherTest,
	"MiniGame.Night.Course.FloatGatherQuantization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCourseFloatGatherTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, false, 1702);
	Config->DefaultDropCount = 2;
	Config->ExitBufferSeconds = 0.f;

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	FNightBootstrap Bootstrap;
	Bootstrap.GiftBuffs.PreForkGatherAmountBonus = 0.3f;
	Director->StartNight(Bootstrap);

	for (int32 Guard = 0; Guard < 128 && Director->IsRunning(); ++Guard)
	{
		if (Director->IsAwaitingInput())
		{
			Director->NotifyFeelResolved(
				Director->GetActiveNodeIndex(),
				ENightJudgeOutcome::Success);
		}
		else
		{
			Director->TickComponent(0.02f, LEVELTICK_All, nullptr);
		}
	}

	TestEqual(
		TEXT("float gather course finishes successfully"),
		Director->GetPhase(),
		ENightCoursePhase::Finished);
	float RuntimeAmount = 0.f;
	for (const FIngredientFloatStack& Stack : Director->GetCollectedIngredients())
	{
		if (Stack.Id == EIngredientId::F01_LingGu)
		{
			RuntimeAmount = Stack.Amount;
			break;
		}
	}
	TestTrue(
		TEXT("three two-unit drops retain their 30 percent fractions at Night"),
		FMath::IsNearlyEqual(RuntimeAmount, 7.8f, 0.001f));

	int32 DayCount = 0;
	for (const FIngredientStack& Stack : Director->GetNightResult().Ingredients)
	{
		if (Stack.Id == EIngredientId::F01_LingGu)
		{
			DayCount = Stack.Count;
			break;
		}
	}
	TestEqual(TEXT("Night to Day quantization floors 7.8 to 7"), DayCount, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightCoursePostForkGiftTest,
	"MiniGame.Night.Course.PostForkInvulnerabilityNumber",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightCoursePostForkGiftTest::RunTest(const FString& Parameters)
{
	UNightG1CourseConfig* Config =
		NightCourseAutomation_Private::MakeAtomConfig(nullptr, true, 1703);
	Config->WrongPenalty = 7.f;
	Config->BranchEnterBufferSeconds = 0.f;

	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	UNightFeelStubComponent* Feel =
		NewObject<UNightFeelStubComponent>(GetTransientPackage());
	Feel->Soul = 100.f;
	Director->BindFeelBridge(Feel);

	FNightBootstrap Bootstrap;
	Bootstrap.ForkPair = ENightForkPair::AB;
	Bootstrap.GiftBuffs.PostForkInvulnerableSeconds = 2.5f;
	Director->StartNight(Bootstrap);
	for (int32 Guard = 0; Guard < 128 && Director->IsRunning(); ++Guard)
	{
		if (Director->IsForkChoiceActive())
		{
			Director->ChooseForkRight();
			break;
		}
		if (Director->IsAwaitingInput())
		{
			Director->NotifyFeelResolved(
				Director->GetActiveNodeIndex(),
				ENightJudgeOutcome::Success);
		}
	}
	for (int32 Guard = 0;
		Guard < 16 && Director->IsRunning() && !Director->IsAwaitingInput();
		++Guard)
	{
		Director->TickComponent(0.02f, LEVELTICK_All, nullptr);
	}

	TestTrue(TEXT("post-fork branch opens a judgement window"), Director->IsAwaitingInput());
	const float SoulBeforeWrong = Feel->Soul;
	Director->NotifyFeelResolved(
		Director->GetActiveNodeIndex(),
		ENightJudgeOutcome::WrongButton);
	TestTrue(
		TEXT("post-fork invulnerability blocks judgement damage during its numeric window"),
		FMath::IsNearlyEqual(Feel->Soul, SoulBeforeWrong, 0.001f));
	Director->ResetCourse();
	return true;
}
#endif
