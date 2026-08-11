#include "Night/Course/NightHazardActor.h"

#pragma region K2 moonyfli
ANightHazardActor::ANightHazardActor()
{
	DebugColor = FLinearColor(0.25f, 0.85f, 1.0f);
	SetActorScale3D(FVector(1.8f, 0.45f, 0.45f));
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
