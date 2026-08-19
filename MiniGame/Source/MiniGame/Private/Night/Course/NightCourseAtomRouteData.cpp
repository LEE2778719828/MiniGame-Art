#include "Night/Course/NightCourseAtomRouteData.h"
#include "Night/Course/NightCourseAtomActor.h"
#include "UObject/Package.h"

#pragma region K2 moonyfli
bool UNightCourseAtomRouteData::ValidateRoute(FString& OutError) const
{
	OutError.Reset();
	if (!bEnabled)
	{
		return true;
	}
	if (TransitionJumpGapCm <= 0.f)
	{
		OutError = TEXT("TransitionJumpGapCm must be greater than zero.");
		return false;
	}
	if (AtomMap.Num() == 0)
	{
		OutError = TEXT("AtomMap is empty.");
		return false;
	}
	if (AtomSequence.Num() == 0)
	{
		OutError = TEXT("AtomSequence is empty.");
		return false;
	}

	TSet<FString> SeenKeys;
	for (const FString& Key : AtomSequence)
	{
		if (Key.IsEmpty())
		{
			OutError = TEXT("AtomSequence contains an empty key.");
			return false;
		}
		if (SeenKeys.Contains(Key))
		{
			OutError = FString::Printf(
				TEXT("AtomSequence contains duplicate key '%s'."),
				*Key);
			return false;
		}
		SeenKeys.Add(Key);

		const TSoftClassPtr<ANightCourseAtomActor>* AtomClass = AtomMap.Find(Key);
		if (!AtomClass || AtomClass->IsNull())
		{
			OutError = FString::Printf(
				TEXT("AtomSequence key '%s' is missing from AtomMap or has no BP class."),
				*Key);
			return false;
		}
	}

	for (const TPair<FString, TSoftClassPtr<ANightCourseAtomActor>>& Entry : AtomMap)
	{
		if (Entry.Key.IsEmpty() || Entry.Value.IsNull())
		{
			OutError = TEXT("AtomMap contains an empty key or null BP class.");
			return false;
		}
	}

	return true;
}

void UNightCourseAtomRouteData::MarkPackageDirtyForEditor()
{
	Modify();
	if (UPackage* Package = GetOutermost())
	{
		Package->SetDirtyFlag(true);
	}
}
#pragma endregion K2 moonyfli
