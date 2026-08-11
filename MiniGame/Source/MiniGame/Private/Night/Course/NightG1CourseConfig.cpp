#include "Night/Course/NightG1CourseConfig.h"

#pragma region K2 moonyfli
void UNightG1CourseConfig::BuildNodeSpecs(TArray<FNightTrackNodeSpec>& OutSpecs) const
{
	OutSpecs.Reset();
	OutSpecs.Reserve(NodeCount);

	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		FNightTrackNodeSpec Spec;
		if (PatternOverride.IsValidIndex(Index))
		{
			Spec.Kind = PatternOverride[Index];
		}
		else
		{
			Spec.Kind = (Index % 2 == 0) ? ENightNodeKind::Enemy : ENightNodeKind::Hazard;
		}

		Spec.TrackDistance = FirstNodeDistance + NodeSpacing * static_cast<float>(Index);
		Spec.JudgeTime = static_cast<float>(Index); // retained for debug labels only
		Spec.FoeId = static_cast<EFoeId>(1 + (Index % 5));
		Spec.DropId = DefaultDropId;
		Spec.DropCount = DefaultDropCount;
		Spec.ArtTag = (Spec.Kind == ENightNodeKind::Enemy)
			? FName(*FString::Printf(TEXT("Foe_%d"), Index))
			: FName(*FString::Printf(TEXT("Hazard_%d"), Index));
		OutSpecs.Add(Spec);
	}
}
#pragma endregion K2 moonyfli
