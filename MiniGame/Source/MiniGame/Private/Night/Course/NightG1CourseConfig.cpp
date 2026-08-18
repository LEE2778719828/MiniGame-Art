#include "Night/Course/NightG1CourseConfig.h"
#include "UObject/Package.h"

#pragma region K2 moonyfli
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

void UNightG1CourseConfig::MarkPackageDirtyForEditor()
{
	Modify();
	if (UPackage* Package = GetOutermost())
	{
		Package->SetDirtyFlag(true);
	}
}

#pragma endregion K2 moonyfli
