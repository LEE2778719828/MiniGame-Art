#include "Night/Course/NightHazardActor.h"

#pragma region K2 moonyfli
ANightHazardActor::ANightHazardActor()
{
	DebugColor = FLinearColor(0.35f, 0.65f, 0.95f);
	JumpGapCm = 420.f;
	KillGapCm = 140.f;
}

void ANightHazardActor::OnResolved_Implementation(ENightJudgeOutcome Outcome)
{
	if (Outcome == ENightJudgeOutcome::Success)
	{
		PlayClearVFX();
	}
	else
	{
		PlayImpactVFX();
	}
	Super::OnResolved_Implementation(Outcome);
}
#pragma endregion K2 moonyfli
