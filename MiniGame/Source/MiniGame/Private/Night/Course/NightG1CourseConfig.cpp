#include "Night/Course/NightG1CourseConfig.h"

#pragma region K2 moonyfli
UNightG1CourseConfig::UNightG1CourseConfig()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FadeMat(
		TEXT("/Game/Night/Course/Materials/M_NightUnlitFade.M_NightUnlitFade"));
	if (FadeMat.Succeeded())
	{
		DistanceFadeMaterial = FadeMat.Object;
	}

	BranchA.BeatCount = 3;
	BranchA.bDefaultPreferAttack = false;

	BranchB.BeatCount = 4;
	BranchB.bDefaultPreferAttack = true;

	BranchC.BeatCount = 5;
	BranchC.bDefaultPreferAttack = true;
	BranchC.DropId = EIngredientId::F03_ChiYanJiao;
	BranchC.DropCount = 1;

	LevelRows = {
		MakeDefaultLevelSettings(ENightLevelId::T0),
		MakeDefaultLevelSettings(ENightLevelId::L1),
		MakeDefaultLevelSettings(ENightLevelId::L2),
		MakeDefaultLevelSettings(ENightLevelId::L3)
	};

#pragma region K2 moonyfli
	ProcParams.TotalNodes = 12;
	ProcParams.MaxYawDeltaDeg = 8.f;
	ProcParams.ForkNodeMin = 4;
	ProcParams.ForkNodeMax = 8;
	ProcParams.Seed = 1001;
	ProcParams.bEnableProcGenerator = true;

	BridgeMeshA = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Night/Course/Art/Bridge/muban1.muban1")));
	BridgeMeshB = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Night/Course/Art/Bridge/muban2.muban2")));
	HeroMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Night/Course/Art/Hero/zhujue.zhujue")));
	FoeMeshM01 = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Night/Course/Art/Foe/fish_moneter.fish_moneter")));
	FoeMeshM02 = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Night/Course/Art/Foe/cantingguai.cantingguai")));
	FoeMeshM03 = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Night/Course/Art/Foe/box1.box1")));
#pragma endregion K2 moonyfli
}

FNightLevelCourseSettings UNightG1CourseConfig::MakeDefaultLevelSettings(ENightLevelId LevelId)
{
	FNightLevelCourseSettings Row;
	Row.LevelId = LevelId;
	Row.bKeySwapOnlyOnRouteC = true;
	Row.RecommendedSeed = 1001 + static_cast<int32>(LevelId);

	switch (LevelId)
	{
	case ENightLevelId::L1:
		Row.ForkPair = ENightForkPair::AB;
		break;
	case ENightLevelId::L2:
	{
		Row.ForkPair = ENightForkPair::AC;
		FNightKeySwapCue A;
		A.TriggerAfterBranchBeats = 1;
		A.WarningSeconds = 0.8f;
		A.SafetyHoldSeconds = 0.6f;
		FNightKeySwapCue B;
		B.TriggerAfterBranchBeats = 3;
		B.WarningSeconds = 0.8f;
		B.SafetyHoldSeconds = 0.6f;
		Row.KeySwaps = { A, B };
		break;
	}
	case ENightLevelId::L3:
	{
		Row.ForkPair = ENightForkPair::BC;
		FNightKeySwapCue A;
		A.TriggerAfterBranchBeats = 1;
		FNightKeySwapCue B;
		B.TriggerAfterBranchBeats = 2;
		FNightKeySwapCue C;
		C.TriggerAfterBranchBeats = 4;
		Row.KeySwaps = { A, B, C };
		break;
	}
	case ENightLevelId::T0:
	default:
		Row.ForkPair = ENightForkPair::AB;
		break;
	}
	return Row;
}

FNightLevelCourseSettings UNightG1CourseConfig::GetLevelSettings(ENightLevelId LevelId) const
{
	for (const FNightLevelCourseSettings& Row : LevelRows)
	{
		if (Row.LevelId == LevelId)
		{
			return Row;
		}
	}
	return MakeDefaultLevelSettings(LevelId);
}

void UNightG1CourseConfig::ApplyLevelDefaultsToBootstrap(FNightBootstrap& InOutBootstrap) const
{
	const FNightLevelCourseSettings Row = GetLevelSettings(InOutBootstrap.LevelId);
	InOutBootstrap.ForkPair = Row.ForkPair;
	if (InOutBootstrap.Seed == 0)
	{
		InOutBootstrap.Seed = Row.RecommendedSeed;
	}
}

FNightBranchLayoutSettings UNightG1CourseConfig::ResolveBranchLayout(ENightRouteId RouteId) const
{
	FNightBranchLayoutSettings Layout;
	switch (RouteId)
	{
	case ENightRouteId::B:
		Layout = BranchB;
		if (Layout.BeatCount <= 0)
		{
			Layout.BeatCount = BranchBBeatCount;
			Layout.PatternOverride = BranchBPatternOverride;
			Layout.bDefaultPreferAttack = true;
		}
		break;
	case ENightRouteId::C:
		Layout = BranchC;
		if (Layout.BeatCount <= 0)
		{
			Layout.BeatCount = 5;
			Layout.bDefaultPreferAttack = true;
		}
		break;
	case ENightRouteId::A:
	default:
		Layout = BranchA;
		if (Layout.BeatCount <= 0)
		{
			Layout.BeatCount = BranchABeatCount;
			Layout.PatternOverride = BranchAPatternOverride;
			Layout.bDefaultPreferAttack = false;
		}
		break;
	}
	return Layout;
}

void UNightG1CourseConfig::BuildCourse(TArray<FNightStoneSpec>& OutStones, TArray<FNightBeatSpec>& OutBeats) const
{
	OutStones.Reset();
	OutBeats.Reset();

	FNightStoneSpec Start;
	Start.TrackDistance = FirstStoneDistance;
	Start.bHasFoe = false;
	OutStones.Add(Start);

	float Cursor = FirstStoneDistance;
	for (int32 Beat = 0; Beat < BeatCount; ++Beat)
	{
		ENightNodeKind Action = ENightNodeKind::Hazard;
		if (PatternOverride.IsValidIndex(Beat))
		{
			Action = PatternOverride[Beat];
		}
		else
		{
			Action = (Beat % 2 == 0) ? ENightNodeKind::Hazard : ENightNodeKind::Enemy;
		}

		const bool bAttack = (Action == ENightNodeKind::Enemy);
		Cursor += bAttack ? KillGapCm : JumpGapCm;

		FNightStoneSpec Stone;
		Stone.TrackDistance = Cursor;
		Stone.bHasFoe = bAttack;
		if (bAttack)
		{
			Stone.FoeId = static_cast<EFoeId>(1 + (Beat % 5));
			Stone.DropId = DefaultDropId;
			Stone.DropCount = DefaultDropCount;
		}
		OutStones.Add(Stone);

		FNightBeatSpec BeatSpec;
		BeatSpec.FromStoneIndex = Beat;
		BeatSpec.ToStoneIndex = Beat + 1;
		BeatSpec.Action = Action;
		OutBeats.Add(BeatSpec);
	}
}
#pragma endregion K2 moonyfli
