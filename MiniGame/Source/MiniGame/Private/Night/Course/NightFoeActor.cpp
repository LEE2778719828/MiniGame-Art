#include "Night/Course/NightFoeActor.h"

#pragma region K2 moonyfli
ANightFoeActor::ANightFoeActor()
{
	DebugColor = FLinearColor(1.0f, 0.25f, 0.2f);
	SetActorScale3D(FVector(0.9f, 0.9f, 1.6f));
}

void ANightFoeActor::OnResolved_Implementation(ENightJudgeOutcome Outcome)
{
	if (Outcome == ENightJudgeOutcome::Success)
	{
		PlaySlashVFX();
		PlayDropBurst(Spec.DropId, Spec.DropCount);
	}
	Super::OnResolved_Implementation(Outcome);
}
#pragma endregion K2 moonyfli
