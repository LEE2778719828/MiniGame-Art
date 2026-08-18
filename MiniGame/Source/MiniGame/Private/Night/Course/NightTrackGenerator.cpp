#include "Night/Course/NightTrackGenerator.h"

#pragma region K2 moonyfli
float UNightTrackGenerator::QuantizeYaw01(float Deg)
{
	return FMath::RoundToFloat(Deg * 10.f) / 10.f;
}

int32 UNightTrackGenerator::ResolveSeed(int32 InSeed)
{
	if (InSeed != 0)
	{
		return InSeed;
	}
	const int32 Rolled = FMath::Rand();
	return (Rolled == 0) ? 1 : Rolled;
}

ENightForkPair UNightTrackGenerator::ForkEnvToPair(ENightForkEnv Env, ENightForkPair CustomPair)
{
	switch (Env)
	{
	case ENightForkEnv::ClearAB: return ENightForkPair::AB;
	case ENightForkEnv::FogAC: return ENightForkPair::AC;
	case ENightForkEnv::ReverseBC: return ENightForkPair::BC;
	default: return CustomPair;
	}
}

ENightNodeKind UNightTrackGenerator::PickAction(
	FRandomStream& Rng,
	float AttackBias,
	ENightNodeKind Last,
	int32& Streak,
	int32 MaxStreak)
{
	const bool bWantAttack = Rng.FRand() < FMath::Clamp(AttackBias, 0.f, 1.f);
	ENightNodeKind Pick = bWantAttack ? ENightNodeKind::Enemy : ENightNodeKind::Hazard;
	if (Pick == Last && Streak >= FMath::Max(1, MaxStreak))
	{
		Pick = (Last == ENightNodeKind::Enemy) ? ENightNodeKind::Hazard : ENightNodeKind::Enemy;
	}
	if (Pick == Last)
	{
		++Streak;
	}
	else
	{
		Streak = 1;
	}
	return Pick;
}

void UNightTrackGenerator::AppendStep(
	FNightGeneratedCourse& Out,
	FRandomStream& Rng,
	const FNightProcCourseParams& Params,
	FVector& InOutPos,
	float& InOutYawDeg,
	float& InOutArc,
	ENightNodeKind& InOutLastAction,
	int32& InOutStreak,
	bool /*bAsAttackPreferred*/)
{
	const ENightNodeKind Action = PickAction(
		Rng, Params.AttackBias, InOutLastAction, InOutStreak, Params.MaxSameActionStreak);
	InOutLastAction = Action;

	const float Gap = (Action == ENightNodeKind::Enemy) ? Params.KillGapCm : Params.JumpGapCm;
	const float MaxYaw = FMath::Max(0.f, Params.MaxYawDeltaDeg);
	const float RawDelta = Rng.FRandRange(-MaxYaw, MaxYaw);
	const float DYaw = QuantizeYaw01(RawDelta);
	InOutYawDeg = QuantizeYaw01(InOutYawDeg + DYaw);

	const FRotator YawRot(0.f, InOutYawDeg, 0.f);
	const FVector StepForward = YawRot.Vector();
	const FVector FromPos = InOutPos;
	InOutPos = FromPos + StepForward * Gap;
	InOutArc += Gap;

	FNightStoneSpec Stone;
	Stone.bUseWorldPose = true;
	Stone.WorldLocation = InOutPos;
	Stone.YawDeg = InOutYawDeg;
	Stone.TrackDistance = InOutArc;
	Stone.bHasFoe = (Action == ENightNodeKind::Enemy);
	if (Stone.bHasFoe)
	{
		const int32 FoeRoll = Rng.RandRange(1, 5);
		Stone.FoeId = static_cast<EFoeId>(FoeRoll);
		Stone.DropId = EIngredientId::F01_LingGu;
		Stone.DropCount = 1;
	}
	const int32 FromIndex = Out.Stones.Num() - 1;
	Out.Stones.Add(Stone);
	const int32 ToIndex = Out.Stones.Num() - 1;

	FNightBeatSpec Beat;
	Beat.FromStoneIndex = FromIndex;
	Beat.ToStoneIndex = ToIndex;
	Beat.Action = Action;
	Out.Beats.Add(Beat);

	FNightBridgeSpec Bridge;
	Bridge.FromStoneIndex = FromIndex;
	Bridge.ToStoneIndex = ToIndex;
	Bridge.WorldLocation = (FromPos + InOutPos) * 0.5f;
	Bridge.YawDeg = InOutYawDeg;
	Bridge.LengthScale = FMath::Max(0.2f, Gap / 200.f);
	Bridge.MeshVariant = (Rng.FRand() < Params.BridgeMeshAWeight) ? 0 : 1;
	Out.Bridges.Add(Bridge);
}

void UNightTrackGenerator::BuildKeySwapCues(
	FNightGeneratedCourse& Out,
	const FNightProcCourseParams& Params,
	int32 BranchBeats)
{
	Out.KeySwaps.Reset();
	if (Params.KeySwapEveryNNodes <= 0 || Params.KeySwapCountPerPeriod <= 0 || BranchBeats <= 0)
	{
		return;
	}

	const int32 Period = FMath::Max(1, Params.KeySwapEveryNNodes);
	const int32 PerPeriod = FMath::Max(1, Params.KeySwapCountPerPeriod);
	for (int32 Beat = 1; Beat <= BranchBeats; ++Beat)
	{
		if ((Beat % Period) != 0)
		{
			continue;
		}
		for (int32 S = 0; S < PerPeriod; ++S)
		{
			FNightKeySwapCue Cue;
			Cue.TriggerAfterBranchBeats = FMath::Clamp(Beat - S, 1, BranchBeats);
			Cue.WarningSeconds = Params.KeySwapWarningSeconds;
			Cue.SafetyHoldSeconds = Params.KeySwapSafetySeconds;
			Cue.bToggle = true;
			Cue.TargetScheme = ENightControlScheme::Swapped;
			Out.KeySwaps.Add(Cue);
		}
	}
}

FNightGeneratedCourse UNightTrackGenerator::GenerateBaseOnly(
	const FNightProcCourseParams& Params,
	const FVector& Origin,
	const FVector& Forward)
{
	FNightGeneratedCourse Out;
	Out.ResolvedSeed = ResolveSeed(Params.Seed);
	Out.ForkPair = ForkEnvToPair(Params.ForkEnv, Params.ForkPair);

	FRandomStream Rng(Out.ResolvedSeed);
	const FVector Fwd = Forward.GetSafeNormal();
	float YawDeg = QuantizeYaw01(Fwd.Rotation().Yaw);
	FVector Pos = Origin;
	float Arc = 0.f;

	FNightStoneSpec Start;
	Start.bUseWorldPose = true;
	Start.WorldLocation = Pos;
	Start.YawDeg = YawDeg;
	Start.TrackDistance = 0.f;
	Out.Stones.Add(Start);

	const int32 Total = FMath::Max(2, Params.TotalNodes);
	int32 ForkMin = FMath::Clamp(Params.ForkNodeMin, 1, Total - 1);
	int32 ForkMax = FMath::Clamp(Params.ForkNodeMax, ForkMin, Total - 1);
	const int32 ForkStone = Rng.RandRange(ForkMin, ForkMax);
	Out.ForkAfterStoneIndex = ForkStone;

	ENightNodeKind LastAction = ENightNodeKind::None;
	int32 Streak = 0;
	for (int32 StoneIdx = 1; StoneIdx <= ForkStone; ++StoneIdx)
	{
		AppendStep(Out, Rng, Params, Pos, YawDeg, Arc, LastAction, Streak, false);
	}

	Out.BaseBeatCount = Out.Beats.Num();
	return Out;
}

FNightGeneratedCourse UNightTrackGenerator::Generate(
	const FNightProcCourseParams& Params,
	const FVector& Origin,
	const FVector& Forward)
{
	return GenerateBaseOnly(Params, Origin, Forward);
}

void UNightTrackGenerator::AppendBranch(
	FNightGeneratedCourse& InOutCourse,
	ENightRouteId RouteId,
	const FNightProcCourseParams& Params,
	const FVector& /*Origin*/)
{
	if (InOutCourse.Stones.Num() == 0)
	{
		return;
	}

	int32 BranchNodes = Params.BranchANodes;
	if (RouteId == ENightRouteId::B)
	{
		BranchNodes = Params.BranchBNodes;
	}
	else if (RouteId == ENightRouteId::C)
	{
		BranchNodes = Params.BranchCNodes;
	}
	BranchNodes = FMath::Max(1, BranchNodes);

	FRandomStream Rng(InOutCourse.ResolvedSeed ^ (static_cast<int32>(RouteId) * 9973));

	const FNightStoneSpec& Last = InOutCourse.Stones.Last();
	float YawDeg = Last.YawDeg;
	// Branch entry: small yaw kick left/right by route for readable fork.
	if (RouteId == ENightRouteId::A)
	{
		YawDeg = QuantizeYaw01(YawDeg - FMath::Min(12.f, Params.MaxYawDeltaDeg));
	}
	else if (RouteId == ENightRouteId::B)
	{
		YawDeg = QuantizeYaw01(YawDeg + FMath::Min(12.f, Params.MaxYawDeltaDeg));
	}
	else
	{
		YawDeg = QuantizeYaw01(YawDeg + FMath::Min(6.f, Params.MaxYawDeltaDeg) * (Rng.FRand() < 0.5f ? -1.f : 1.f));
	}

	FVector Pos = Last.WorldLocation + FRotator(0.f, YawDeg, 0.f).Vector() * Params.BranchEntryGapCm;
	float Arc = Last.TrackDistance + Params.BranchEntryGapCm;

	FNightStoneSpec BranchStart;
	BranchStart.bUseWorldPose = true;
	BranchStart.WorldLocation = Pos;
	BranchStart.YawDeg = YawDeg;
	BranchStart.TrackDistance = Arc;
	const int32 FromLast = InOutCourse.Stones.Num() - 1;
	InOutCourse.Stones.Add(BranchStart);
	const int32 BranchStartIndex = InOutCourse.Stones.Num() - 1;

	{
		FNightBridgeSpec Bridge;
		Bridge.FromStoneIndex = FromLast;
		Bridge.ToStoneIndex = BranchStartIndex;
		Bridge.WorldLocation = (Last.WorldLocation + Pos) * 0.5f;
		Bridge.YawDeg = YawDeg;
		Bridge.LengthScale = FMath::Max(0.2f, Params.BranchEntryGapCm / 200.f);
		Bridge.MeshVariant = 0;
		InOutCourse.Bridges.Add(Bridge);
	}

	ENightNodeKind LastAction = ENightNodeKind::None;
	int32 Streak = 0;
	const int32 BeatsBefore = InOutCourse.Beats.Num();
	for (int32 i = 0; i < BranchNodes; ++i)
	{
		AppendStep(InOutCourse, Rng, Params, Pos, YawDeg, Arc, LastAction, Streak, true);
	}
	const int32 BranchBeats = InOutCourse.Beats.Num() - BeatsBefore;
	BuildKeySwapCues(InOutCourse, Params, BranchBeats);
}
#pragma endregion K2 moonyfli
