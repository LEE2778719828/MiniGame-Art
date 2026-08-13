#include "Runner/RunnerTrackData.h"

#pragma region K2 moonyfli
int32 URunnerTrackData::FindNextEventIndex(float CurrentDistance) const
{
	int32 BestIndex = INDEX_NONE;
	float BestDistance = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		const float EventDistance = Events[Index].Distance;
		if (EventDistance + KINDA_SMALL_NUMBER < CurrentDistance)
		{
			continue;
		}

		if (EventDistance < BestDistance)
		{
			BestDistance = EventDistance;
			BestIndex = Index;
		}
	}

	return BestIndex;
}
#pragma endregion K2 moonyfli
