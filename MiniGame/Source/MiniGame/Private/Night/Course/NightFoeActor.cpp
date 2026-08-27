#include "Night/Course/NightFoeActor.h"
#include "Night/Course/NightCoursePawn.h" //add by K2
#include "Kismet/GameplayStatics.h" //add by K2

#pragma region K2 moonyfli
ANightFoeActor::ANightFoeActor()
{
	DebugColor = FLinearColor(0.75f, 0.55f, 0.35f);
	KillGapCm = 140.f;
	JumpGapCm = 420.f;
}

void ANightFoeActor::OnResolved_Implementation(ENightJudgeOutcome Outcome)
{
	if (Outcome == ENightJudgeOutcome::Success)
	{
		if (ANightCoursePawn* Hero = Cast<ANightCoursePawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Hero->PlayAttackVFX(GetActorLocation());
		}
		PlaySlashVFX();
		PlayDropBurst(Spec.DropId, Spec.DropCount);
	}
	Super::OnResolved_Implementation(Outcome);
}
#pragma endregion K2 moonyfli
