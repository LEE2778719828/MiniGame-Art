#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseForkAtomActor.h"
#include "Night/Course/NightCourseRoadsideActor.h"
#include "Night/Course/NightCourseStoneActor.h"
#include "UObject/Package.h"

// Runtime foe classes are resolved from the canonical course map.

#pragma region K2 moonyfli
namespace
{
	bool ValidateRoadsideSet(
		const FNightRoadsideGenerationSettings& Settings,
		const TCHAR* Label,
		FString& OutError)
	{
		OutError.Reset();
		if (!FMath::IsFinite(Settings.SpacingCm)
			|| Settings.SpacingCm < 0.f
			|| !FMath::IsFinite(Settings.LeftBridgeOffsetCm)
			|| Settings.LeftBridgeOffsetCm < 0.f
			|| !FMath::IsFinite(Settings.RightBridgeOffsetCm)
			|| Settings.RightBridgeOffsetCm < 0.f
			|| !FMath::IsFinite(Settings.ZOffsetCm)
			|| !FMath::IsFinite(Settings.RandomYawRangeDeg)
			|| Settings.RandomYawRangeDeg < 0.f)
		{
			OutError = FString::Printf(
				TEXT("%s roadside settings contain an invalid spacing, offset, or yaw value."),
				Label);
			return false;
		}

		bool bHasPositiveWeight = false;
		for (int32 EntryIndex = 0; EntryIndex < Settings.BlueprintPool.Num(); ++EntryIndex)
		{
			const FNightRoadsideBlueprintEntry& Entry = Settings.BlueprintPool[EntryIndex];
			if (Entry.Weight < 0.f || !FMath::IsFinite(Entry.Weight))
			{
				OutError = FString::Printf(
					TEXT("%s roadside Blueprint entry %d has an invalid weight."),
					Label,
					EntryIndex);
				return false;
			}
			if (Entry.Weight <= 0.f)
			{
				continue;
			}
			bHasPositiveWeight = true;
			if (Entry.Blueprint.IsNull())
			{
				OutError = FString::Printf(
					TEXT("%s roadside Blueprint entry %d is empty."),
					Label,
					EntryIndex);
				return false;
			}

			UClass* PropClass = Entry.Blueprint.LoadSynchronous();
			if (!PropClass)
			{
				OutError = FString::Printf(
					TEXT("%s roadside Blueprint entry %d failed to load '%s'."),
					Label,
					EntryIndex,
					*Entry.Blueprint.ToSoftObjectPath().ToString());
				return false;
			}
			if (!PropClass->IsChildOf(ANightRoadsideSegmentActor::StaticClass()))
			{
				OutError = FString::Printf(
					TEXT("%s roadside Blueprint entry %d resolves to '%s', which is not an ANightRoadsideSegmentActor subclass."),
					Label,
					EntryIndex,
					*GetNameSafe(PropClass));
				return false;
			}

			const ANightRoadsideSegmentActor* Defaults =
				PropClass->GetDefaultObject<ANightRoadsideSegmentActor>();
			FVector Start;
			FVector End;
			if (!Defaults
				|| !Defaults->GetRoadsideMarkerLocations(Start, End)
				|| FVector::DistSquared(Start, End) <= FMath::Square(KINDA_SMALL_NUMBER))
			{
				OutError = FString::Printf(
					TEXT("%s roadside Blueprint '%s' must provide distinct StartMarker and EndMarker components."),
					Label,
					*GetNameSafe(PropClass));
				return false;
			}
		}

		if (Settings.bEnabled
			&& Settings.BlueprintPool.Num() > 0
			&& !bHasPositiveWeight)
		{
			OutError = FString::Printf(
				TEXT("%s roadside BlueprintPool must contain a positive-weight entry when enabled."),
				Label);
			return false;
		}
		return true;
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

UClass* UNightG1CourseConfig::ResolveFoeActorClass(
	EFoeId FoeId,
	FString& OutError) const
{
	OutError.Reset();
	if (FoeId == EFoeId::None)
	{
		OutError = TEXT("Foe ID is None.");
		return nullptr;
	}

	const TSoftClassPtr<ANightCourseStoneActor>* FoeClassRef =
		FoeActorMap.Find(FoeId);
	if (!FoeClassRef || FoeClassRef->IsNull())
	{
		OutError = FString::Printf(
			TEXT("FoeActorMap has no Blueprint for FoeId=%d."),
			static_cast<int32>(FoeId));
		return nullptr;
	}

	UClass* FoeClass = FoeClassRef->LoadSynchronous();
	if (!FoeClass)
	{
		OutError = FString::Printf(
			TEXT("FoeActorMap entry for FoeId=%d failed to load class '%s'."),
			static_cast<int32>(FoeId),
			*FoeClassRef->ToSoftObjectPath().ToString());
		return nullptr;
	}
	if (!FoeClass->IsChildOf(ANightCourseStoneActor::StaticClass()))
	{
		OutError = FString::Printf(
			TEXT("FoeActorMap entry for FoeId=%d resolves to '%s', which is not an ANightCourseStoneActor subclass."),
			static_cast<int32>(FoeId),
			*GetNameSafe(FoeClass));
		return nullptr;
	}
	return FoeClass;
}

bool UNightG1CourseConfig::ValidateFoeActorMap(FString& OutError) const
{
	OutError.Reset();
	for (const TPair<EFoeId, TSoftClassPtr<ANightCourseStoneActor>>& Pair : FoeActorMap)
	{
		if (Pair.Key == EFoeId::None)
		{
			OutError = TEXT("FoeActorMap cannot contain an entry for FoeId=None.");
			return false;
		}

		switch (Pair.Key)
		{
		case EFoeId::M01:
		case EFoeId::M02:
		case EFoeId::M03:
		case EFoeId::M04:
		case EFoeId::M05:
			break;
		default:
			OutError = FString::Printf(
				TEXT("FoeActorMap contains unsupported FoeId=%d."),
				static_cast<int32>(Pair.Key));
			return false;
		}

		FString ResolveError;
		if (!ResolveFoeActorClass(Pair.Key, ResolveError))
		{
			OutError = ResolveError;
			return false;
		}
	}
	return true;
}

bool UNightG1CourseConfig::TryGetFoeDropId(
	EFoeId FoeId,
	EIngredientId& OutDropId) const
{
	OutDropId = EIngredientId::None;
	if (FoeId == EFoeId::None)
	{
		return false;
	}
	const EIngredientId* DropId = FoeDropMap.Find(FoeId);
	if (!DropId || *DropId == EIngredientId::None)
	{
		return false;
	}
	OutDropId = *DropId;
	return true;
}

bool UNightG1CourseConfig::ValidateFoeDropMap(FString& OutError) const
{
	OutError.Reset();
	for (const TPair<EFoeId, EIngredientId>& Pair : FoeDropMap)
	{
		if (Pair.Key == EFoeId::None)
		{
			OutError = TEXT("FoeDropMap cannot contain an entry for FoeId=None.");
			return false;
		}
		if (Pair.Value == EIngredientId::None)
		{
			OutError = FString::Printf(
				TEXT("FoeDropMap entry for FoeId=%d cannot use IngredientId=None."),
				static_cast<int32>(Pair.Key));
			return false;
		}
	}

	static const EFoeId RequiredFoes[] = {
		EFoeId::M01,
		EFoeId::M02,
		EFoeId::M03,
		EFoeId::M04,
		EFoeId::M05
	};
	for (const EFoeId FoeId : RequiredFoes)
	{
		EIngredientId DropId = EIngredientId::None;
		if (!TryGetFoeDropId(FoeId, DropId))
		{
			OutError = FString::Printf(
				TEXT("FoeDropMap must bind a non-None ingredient for FoeId=%d."),
				static_cast<int32>(FoeId));
			return false;
		}
	}
	return true;
}

UClass* UNightG1CourseConfig::ResolveForkAtomClass(
	ENightForkPair ForkPair,
	FString& OutError) const
{
	OutError.Reset();
	const TSoftClassPtr<ANightCourseForkAtomActor>* ForkClassRef =
		ForkAtomMap.Find(ForkPair);
	if (!ForkClassRef || ForkClassRef->IsNull())
	{
		OutError = FString::Printf(
			TEXT("ForkAtomMap has no Blueprint for pair %d."),
			static_cast<int32>(ForkPair));
		return nullptr;
	}

	UClass* ForkClass = ForkClassRef->LoadSynchronous();
	if (!ForkClass)
	{
		OutError = FString::Printf(
			TEXT("ForkAtomMap entry for pair %d failed to load class '%s'."),
			static_cast<int32>(ForkPair),
			*ForkClassRef->ToSoftObjectPath().ToString());
		return nullptr;
	}
	if (!ForkClass->IsChildOf(ANightCourseForkAtomActor::StaticClass()))
	{
		OutError = FString::Printf(
			TEXT("ForkAtomMap entry for pair %d resolves to '%s', which is not an ANightCourseForkAtomActor subclass."),
			static_cast<int32>(ForkPair),
			*GetNameSafe(ForkClass));
		return nullptr;
	}
	return ForkClass;
}

bool UNightG1CourseConfig::ValidateForkAtomMap(FString& OutError) const
{
	OutError.Reset();
	for (const TPair<ENightForkPair, TSoftClassPtr<ANightCourseForkAtomActor>>& Pair : ForkAtomMap)
	{
		switch (Pair.Key)
		{
		case ENightForkPair::AB:
		case ENightForkPair::AC:
		case ENightForkPair::BC:
			break;
		default:
			OutError = FString::Printf(
				TEXT("ForkAtomMap contains unsupported ForkPair=%d."),
				static_cast<int32>(Pair.Key));
			return false;
		}

		// Missing pair entries are allowed for migration: the Director keeps
		// the existing logic-only fork until the visual asset is configured.
		if (Pair.Value.IsNull())
		{
			continue;
		}

		FString ResolveError;
		UClass* ForkClass = ResolveForkAtomClass(Pair.Key, ResolveError);
		if (!ForkClass)
		{
			OutError = ResolveError;
			return false;
		}

		const ANightCourseForkAtomActor* Defaults =
			ForkClass->GetDefaultObject<ANightCourseForkAtomActor>();
		if (!Defaults)
		{
			OutError = FString::Printf(
				TEXT("ForkAtomMap entry for pair %d has no valid CDO."),
				static_cast<int32>(Pair.Key));
			return false;
		}

		FString AtomError;
		if (!Defaults->ValidateForkAtom(AtomError))
		{
			OutError = FString::Printf(
				TEXT("ForkAtomMap entry for pair %d is invalid: %s"),
				static_cast<int32>(Pair.Key),
				*AtomError);
			return false;
		}
	}
	return true;
}

bool UNightG1CourseConfig::ValidateRoadsideConfiguration(FString& OutError) const
{
	OutError.Reset();
	if (!FMath::IsFinite(HouseInitialZOffsetCm))
	{
		OutError = TEXT("HouseInitialZOffsetCm must be a finite value.");
		return false;
	}
	if (!ValidateRoadsideSet(HouseRoadside, TEXT("House"), OutError))
	{
		return false;
	}
	if (!ValidateRoadsideSet(PoleRoadside, TEXT("Pole"), OutError))
	{
		return false;
	}
	return true;
}

#pragma endregion K2 moonyfli
