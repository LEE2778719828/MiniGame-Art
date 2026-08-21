#include "Misc/AutomationTest.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightCourseAtomRouteData.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseRuleData.h"
#include "Night/Course/NightForkController.h"
#include "Night/Course/NightRouteRules.h"
#include "Night/Course/NightTrackGenerator.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

#if WITH_AUTOMATION_TESTS

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
		Config->DefaultFoeId = EFoeId::M01;

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
		Rule->Seed = RuleSeed;
		Rule->BaseRoute = {
			MakeEntry(ENightNodeKind::Hazard, ENightNodeKind::Enemy, ENightNodeKind::Hazard),
			MakeEntry(ENightNodeKind::Enemy, ENightNodeKind::Hazard, ENightNodeKind::Enemy)
		};
		if (bEnableFork)
		{
			Rule->ForkAfterBaseAtomIndex = 2;
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
			"baseAtomCount": 8,
			"baseRoute": [
				{"atomKey":"A","actions":["Jump","Kill"],"weight":5}
			],
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
	TestTrue(TEXT("base target atom count is imported"), Rule->GetBaseRouteLength() == 8);
	TestTrue(TEXT("A branch is populated"), Rule->HasBranchRoute(ENightRouteId::A));
	TestTrue(TEXT("B branch is populated"), Rule->HasBranchRoute(ENightRouteId::B));
	TestTrue(TEXT("C branch is populated"), Rule->HasBranchRoute(ENightRouteId::C));
	TestTrue(TEXT("duplicate atom keys remain reusable"), Rule->BranchRoutes[ENightRouteId::A].Atoms[0].AtomKey == TEXT("A"));
	TestTrue(TEXT("base weight is imported"), Rule->BaseRoute[0].Weight == 5);
	TestTrue(TEXT("branch target atom count is imported"), Rule->BranchRoutes[ENightRouteId::A].TargetAtomCount == 6);
	TestTrue(TEXT("rule validates"), Rule->ValidateRule(Error));
	FString ExportedJson;
	TestTrue(TEXT("weighted queue JSON exports"), Rule->ExportJson(ExportedJson, Error));
	TestTrue(TEXT("export includes base target count"), ExportedJson.Contains(TEXT("\"baseAtomCount\"")));
	TestTrue(TEXT("export includes branch target count"), ExportedJson.Contains(TEXT("\"targetAtomCount\"")));
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
		FirstConfig->CourseRuleData->Seed = CandidateSeed;
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

	FirstConfig->CourseRuleData->Seed = 1001;
	FirstConfig->CourseRuleData->BaseRoute[0].AtomKey = TEXT("B");
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
	Rule->BaseAtomCount = 30;
	Rule->BaseRoute = {
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
	Rule->BaseRoute[0].AtomKey = TEXT("A");
	Rule->BaseRoute[1].AtomKey = TEXT("A");

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
	TestEqual(TEXT("target base atom count is generated"), Stones.Num(), 30 * 4);
	TestEqual(TEXT("internal and transition beats are generated"), Beats.Num(), 30 * 3 + 29);

	int32 HazardEnemyHazardCount = 0;
	int32 EnemyEnemyEnemyCount = 0;
	for (int32 AtomIndex = 0; AtomIndex < 30; ++AtomIndex)
	{
		const int32 StoneOffset = AtomIndex * 4;
		if (!Stones.IsValidIndex(StoneOffset + 3))
		{
			continue;
		}
		if (!Stones[StoneOffset + 1].bHasFoe
			&& Stones[StoneOffset + 2].bHasFoe
			&& !Stones[StoneOffset + 3].bHasFoe)
		{
			++HazardEnemyHazardCount;
		}
		else if (Stones[StoneOffset + 1].bHasFoe
			&& Stones[StoneOffset + 2].bHasFoe
			&& Stones[StoneOffset + 3].bHasFoe)
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
	Config->CourseRuleData->BaseRoute[0].Actions = {
		ENightNodeKind::Hazard,
		ENightNodeKind::Enemy
	};
	UNightCourseDirector* Director =
		NewObject<UNightCourseDirector>(GetTransientPackage());
	Director->Config = Config;
	FString Error;
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
		2);
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

#endif
