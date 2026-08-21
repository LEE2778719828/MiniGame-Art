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

void UNightCourseAtomRouteData::GetCompatibleAtomKeys(
	const int32 RequiredActionCount,
	TArray<FString>& OutKeys,
	TArray<FString>* OutRejectionReasons) const
{
	OutKeys.Reset();
	if (OutRejectionReasons)
	{
		OutRejectionReasons->Reset();
	}
	if (RequiredActionCount < 0)
	{
		return;
	}

	TArray<FString> ExactMatches;
	TArray<FString> UnknownCountMatches;
	for (const TPair<FString, TSoftClassPtr<ANightCourseAtomActor>>& Entry : AtomMap)
	{
		if (Entry.Key.IsEmpty() || Entry.Value.IsNull())
		{
			if (OutRejectionReasons)
			{
				OutRejectionReasons->Add(
					FString::Printf(
						TEXT("key '%s' is empty or has no BP class"),
						*Entry.Key));
			}
			continue;
		}

		UClass* AtomClass = Entry.Value.LoadSynchronous();
		if (!AtomClass || !AtomClass->IsChildOf(ANightCourseAtomActor::StaticClass()))
		{
			if (OutRejectionReasons)
			{
				OutRejectionReasons->Add(
					FString::Printf(
						TEXT("key '%s' does not resolve to an Atom BP"),
						*Entry.Key));
			}
			continue;
		}

		const ANightCourseAtomActor* AtomCDO =
			Cast<ANightCourseAtomActor>(AtomClass->GetDefaultObject());
		if (!AtomCDO)
		{
			if (OutRejectionReasons)
			{
				OutRejectionReasons->Add(
					FString::Printf(TEXT("key '%s' has no valid Atom CDO"), *Entry.Key));
			}
			continue;
		}

		const int32 LandingPointCount = AtomCDO->GetLandingPointCount();
		if (LandingPointCount <= 0)
		{
			// Blueprint SCS templates can be unavailable on an editor CDO. Keep
			// the valid class as a candidate and let the transient Composer
			// instance perform the authoritative count check.
			UnknownCountMatches.Add(Entry.Key);
		}
		else if (LandingPointCount == RequiredActionCount + 1)
		{
			ExactMatches.Add(Entry.Key);
		}
		else if (OutRejectionReasons)
		{
			OutRejectionReasons->Add(
				FString::Printf(
					TEXT("key '%s' has %d landing points; expected %d"),
					*Entry.Key,
					LandingPointCount,
					RequiredActionCount + 1));
		}
	}

	ExactMatches.Sort();
	UnknownCountMatches.Sort();
	OutKeys = ExactMatches.Num() > 0 ? MoveTemp(ExactMatches) : MoveTemp(UnknownCountMatches);
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
