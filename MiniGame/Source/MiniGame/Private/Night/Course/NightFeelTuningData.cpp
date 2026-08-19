#include "Night/Course/NightFeelTuningData.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCoursePawn.h"

#pragma region K2 moonyfli
void UNightFeelTuningData::ApplyTo(UNightFeelStubComponent& Feel, ANightCoursePawn* Pawn) const
{
	Feel.JumpWindowMs = JumpWindowMs;
	Feel.AttackWindowMs = AttackWindowMs;
	Feel.bGraceWaitsForAnim = bGraceWaitsForAnim;
	Feel.bUseTutorialWindows = bUseTutorialWindows;
	Feel.TutorialJumpWindowMs = TutorialJumpWindowMs;
	Feel.TutorialAttackWindowMs = TutorialAttackWindowMs;

	Feel.EarlyAcceptMs = EarlyAcceptMs;
	Feel.CatchUpPlayRate = CatchUpPlayRate;
	Feel.MaxCatchUpCompressMs = MaxCatchUpCompressMs;
	Feel.MaxBufferedInputs = MaxBufferedInputs;

	Feel.BreathDecayPerSecond = BreathDecayPerSecond;
	Feel.bEnableBreathDecay = bEnableBreathDecay;
	Feel.SoulPenaltyHazard = SoulPenaltyHazard;
	Feel.SoulPenaltyEnemy = SoulPenaltyEnemy;
	Feel.HitInvulnMs = HitInvulnMs;

	Feel.bLogJudge = bLogJudge;
	Feel.bLogHudLines = bLogHudLines;

	// Soul 本身不套：起始魂归 R2 的 Config，Host 在接线时写入。

	if (Pawn)
	{
		Pawn->JumpAnimRate = JumpAnimRate;
		Pawn->AttackAnimRate = AttackAnimRate;
		Pawn->bAnimDrivenAdvance = bAnimDrivenAdvance;
		Pawn->JumpAnchorMs = JumpAnchorMs;
		Pawn->AttackAnchorMs = AttackAnchorMs;
	}
}

void UNightFeelTuningData::CaptureFrom(const UNightFeelStubComponent& Feel, const ANightCoursePawn* Pawn)
{
	JumpWindowMs = Feel.JumpWindowMs;
	AttackWindowMs = Feel.AttackWindowMs;
	bGraceWaitsForAnim = Feel.bGraceWaitsForAnim;
	bUseTutorialWindows = Feel.bUseTutorialWindows;
	TutorialJumpWindowMs = Feel.TutorialJumpWindowMs;
	TutorialAttackWindowMs = Feel.TutorialAttackWindowMs;

	EarlyAcceptMs = Feel.EarlyAcceptMs;
	CatchUpPlayRate = Feel.CatchUpPlayRate;
	MaxCatchUpCompressMs = Feel.MaxCatchUpCompressMs;
	MaxBufferedInputs = Feel.MaxBufferedInputs;

	BreathDecayPerSecond = Feel.BreathDecayPerSecond;
	bEnableBreathDecay = Feel.bEnableBreathDecay;
	SoulPenaltyHazard = Feel.SoulPenaltyHazard;
	SoulPenaltyEnemy = Feel.SoulPenaltyEnemy;
	HitInvulnMs = Feel.HitInvulnMs;

	bLogJudge = Feel.bLogJudge;
	bLogHudLines = Feel.bLogHudLines;

	if (Pawn)
	{
		JumpAnimRate = Pawn->JumpAnimRate;
		AttackAnimRate = Pawn->AttackAnimRate;
		bAnimDrivenAdvance = Pawn->bAnimDrivenAdvance;
		JumpAnchorMs = Pawn->JumpAnchorMs;
		AttackAnchorMs = Pawn->AttackAnchorMs;
	}
}
#pragma endregion K2 moonyfli
