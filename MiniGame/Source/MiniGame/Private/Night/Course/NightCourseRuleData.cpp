#include "Night/Course/NightCourseRuleData.h"

#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightCourseAtomRouteData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogNightCourseRule, Log, All);

#pragma region K2 moonyfli
namespace NightCourseRule_Private
{
	static bool ParseAction(const FString& InAction, ENightNodeKind& OutAction)
	{
		const FString Normalized = InAction.TrimStartAndEnd().ToLower();
		if (Normalized == TEXT("jump") || Normalized == TEXT("hazard"))
		{
			OutAction = ENightNodeKind::Hazard;
			return true;
		}
		if (Normalized == TEXT("kill") || Normalized == TEXT("attack") || Normalized == TEXT("enemy"))
		{
			OutAction = ENightNodeKind::Enemy;
			return true;
		}
		return false;
	}

	static FString ActionToString(const ENightNodeKind Action)
	{
		return Action == ENightNodeKind::Enemy ? TEXT("Kill") : TEXT("Jump");
	}

	static bool ParseRouteId(const FString& InRoute, ENightRouteId& OutRoute)
	{
		if (InRoute.Equals(TEXT("A"), ESearchCase::IgnoreCase))
		{
			OutRoute = ENightRouteId::A;
			return true;
		}
		if (InRoute.Equals(TEXT("B"), ESearchCase::IgnoreCase))
		{
			OutRoute = ENightRouteId::B;
			return true;
		}
		if (InRoute.Equals(TEXT("C"), ESearchCase::IgnoreCase))
		{
			OutRoute = ENightRouteId::C;
			return true;
		}
		return false;
	}

	static FString RouteIdToString(const ENightRouteId Route)
	{
		switch (Route)
		{
		case ENightRouteId::A: return TEXT("A");
		case ENightRouteId::B: return TEXT("B");
		case ENightRouteId::C: return TEXT("C");
		default: return TEXT("None");
		}
	}

	static bool ParseEntryObject(
		const TSharedPtr<FJsonObject>& EntryObject,
		const int32 EntryIndex,
		const FString& RouteLabel,
		FNightRuleAtomEntry& OutEntry,
		FString& OutError)
	{
		if (!EntryObject.IsValid())
		{
			OutError = FString::Printf(
				TEXT("%s[%d] is not an object."),
				*RouteLabel,
				EntryIndex);
			return false;
		}

		if (!EntryObject->TryGetStringField(TEXT("atomKey"), OutEntry.AtomKey))
		{
			OutError = FString::Printf(
				TEXT("%s[%d] requires string 'atomKey'."),
				*RouteLabel,
				EntryIndex);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* JsonActions = nullptr;
		if (!EntryObject->TryGetArrayField(TEXT("actions"), JsonActions) || !JsonActions)
		{
			OutError = FString::Printf(
				TEXT("%s[%d] requires array 'actions'."),
				*RouteLabel,
				EntryIndex);
			return false;
		}

		for (int32 ActionIndex = 0; ActionIndex < JsonActions->Num(); ++ActionIndex)
		{
			FString ActionText;
			if (!(*JsonActions)[ActionIndex].IsValid()
				|| !(*JsonActions)[ActionIndex]->TryGetString(ActionText))
			{
				OutError = FString::Printf(
					TEXT("%s[%d].actions[%d] must be a string."),
					*RouteLabel,
					EntryIndex,
					ActionIndex);
				return false;
			}

			ENightNodeKind Action = ENightNodeKind::None;
			if (!ParseAction(ActionText, Action))
			{
				OutError = FString::Printf(
					TEXT("%s[%d].actions[%d] has unsupported action '%s'."),
					*RouteLabel,
					EntryIndex,
					ActionIndex,
					*ActionText);
				return false;
			}
			OutEntry.Actions.Add(Action);
		}
		double JsonWeight = 1.0;
		if (EntryObject->TryGetNumberField(TEXT("weight"), JsonWeight))
		{
			OutEntry.Weight = FMath::RoundToInt(static_cast<float>(JsonWeight));
		}
		return true;
	}

	static bool ParseEntryArray(
		const TArray<TSharedPtr<FJsonValue>>* JsonRoute,
		const FString& RouteLabel,
		TArray<FNightRuleAtomEntry>& OutEntries,
		FString& OutError)
	{
		if (!JsonRoute)
		{
			OutError = FString::Printf(
				TEXT("Rule JSON requires array '%s'."),
				*RouteLabel);
			return false;
		}

		OutEntries.Reset();
		OutEntries.Reserve(JsonRoute->Num());
		for (int32 EntryIndex = 0; EntryIndex < JsonRoute->Num(); ++EntryIndex)
		{
			const TSharedPtr<FJsonObject> EntryObject = (*JsonRoute)[EntryIndex].IsValid()
				? (*JsonRoute)[EntryIndex]->AsObject()
				: nullptr;
			FNightRuleAtomEntry Entry;
			if (!ParseEntryObject(EntryObject, EntryIndex, RouteLabel, Entry, OutError))
			{
				return false;
			}
			OutEntries.Add(MoveTemp(Entry));
		}
		return true;
	}

	static void WriteEntryArray(
		const TArray<FNightRuleAtomEntry>& Entries,
		TArray<TSharedPtr<FJsonValue>>& OutJsonEntries)
	{
		OutJsonEntries.Reset();
		for (const FNightRuleAtomEntry& Entry : Entries)
		{
			TSharedRef<FJsonObject> JsonEntry = MakeShared<FJsonObject>();
			JsonEntry->SetStringField(TEXT("atomKey"), Entry.AtomKey);

			TArray<TSharedPtr<FJsonValue>> JsonActions;
			for (const ENightNodeKind Action : Entry.Actions)
			{
				JsonActions.Add(MakeShared<FJsonValueString>(ActionToString(Action)));
			}
			JsonEntry->SetArrayField(TEXT("actions"), MoveTemp(JsonActions));
			JsonEntry->SetNumberField(TEXT("weight"), Entry.Weight);
			OutJsonEntries.Add(MakeShared<FJsonValueObject>(JsonEntry));
		}
	}
}

bool UNightCourseRuleData::ValidateRule(FString& OutError) const
{
	OutError.Reset();
	if (!bEnabled)
	{
		return true;
	}
	if (BaseRoute.Num() == 0)
	{
		OutError = TEXT("Rule base route is empty.");
		return false;
	}
	if (BaseAtomCount < 0)
	{
		OutError = FString::Printf(
			TEXT("BaseAtomCount %d cannot be negative."),
			BaseAtomCount);
		return false;
	}

	auto ValidateEntries = [this, &OutError](
		const TArray<FNightRuleAtomEntry>& Entries,
		const FString& RouteLabel)
	{
		for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
		{
			const FNightRuleAtomEntry& Entry = Entries[EntryIndex];
			const FString Key = Entry.AtomKey.TrimStartAndEnd();
			if (Entry.Weight <= 0)
			{
				OutError = FString::Printf(
					TEXT("%s entry %d has invalid weight %d; weight must be greater than zero."),
					*RouteLabel,
					EntryIndex,
					Entry.Weight);
				return false;
			}
			if (Key.IsEmpty() && !bAutoSelectAtomKeys)
			{
				OutError = FString::Printf(
					TEXT("%s entry %d has an empty AtomKey while auto-selection is disabled."),
					*RouteLabel,
					EntryIndex);
				return false;
			}

			for (int32 ActionIndex = 0; ActionIndex < Entry.Actions.Num(); ++ActionIndex)
			{
				if (Entry.Actions[ActionIndex] != ENightNodeKind::Enemy
					&& Entry.Actions[ActionIndex] != ENightNodeKind::Hazard)
				{
					OutError = FString::Printf(
						TEXT("%s '%s' action %d is None/unsupported."),
						*RouteLabel,
						Key.IsEmpty() ? TEXT("<auto>") : *Key,
						ActionIndex);
					return false;
				}
			}
		}
		return true;
	};

	if (!ValidateEntries(BaseRoute, TEXT("Base route")))
	{
		return false;
	}

	if (ForkAfterBaseAtomIndex != INDEX_NONE
		&& (ForkAfterBaseAtomIndex <= 0
			|| ForkAfterBaseAtomIndex
				> (BaseAtomCount > 0 ? BaseAtomCount : BaseRoute.Num())))
	{
		OutError = FString::Printf(
			TEXT("ForkAfterBaseAtomIndex %d is outside generated base route length %d."),
			ForkAfterBaseAtomIndex,
			BaseAtomCount > 0 ? BaseAtomCount : BaseRoute.Num());
		return false;
	}

	for (const TPair<ENightRouteId, FNightRuleAtomQueue>& Pair : BranchRoutes)
	{
		if (Pair.Key == ENightRouteId::None)
		{
			OutError = TEXT("BranchRoutes contains the None route.");
			return false;
		}
		if (Pair.Value.Atoms.Num() == 0)
		{
			OutError = FString::Printf(
				TEXT("Branch route %s is empty."),
				*NightCourseRule_Private::RouteIdToString(Pair.Key));
			return false;
		}
		if (Pair.Value.TargetAtomCount < 0)
		{
			OutError = FString::Printf(
				TEXT("Branch route %s has negative TargetAtomCount %d."),
				*NightCourseRule_Private::RouteIdToString(Pair.Key),
				Pair.Value.TargetAtomCount);
			return false;
		}
		if (!ValidateEntries(
			Pair.Value.Atoms,
			FString::Printf(
				TEXT("Branch route %s"),
				*NightCourseRule_Private::RouteIdToString(Pair.Key))))
		{
			return false;
		}
	}

	return true;
}

bool UNightCourseRuleData::ValidateRuleAgainstLibrary(
	const UNightCourseAtomRouteData* AtomLibrary,
	FString& OutError) const
{
	if (!ValidateRule(OutError))
	{
		return false;
	}
	if (!bEnabled)
	{
		return true;
	}
	if (!AtomLibrary)
	{
		OutError = TEXT("An Atom Library is required to validate rule landing-point counts.");
		return false;
	}

	auto ValidateEntries = [this, AtomLibrary, &OutError](
		const TArray<FNightRuleAtomEntry>& Entries,
		const FString& RouteLabel)
	{
		for (const FNightRuleAtomEntry& Entry : Entries)
		{
			const FString Key = Entry.AtomKey.TrimStartAndEnd();
			if (Key.IsEmpty())
			{
				if (!bAutoSelectAtomKeys)
				{
					OutError = FString::Printf(
						TEXT("%s contains an empty AtomKey while auto-selection is disabled."),
						*RouteLabel);
					return false;
				}

				TArray<FString> Candidates;
				AtomLibrary->GetCompatibleAtomKeys(Entry.Actions.Num(), Candidates);
				if (Candidates.Num() == 0)
				{
					OutError = FString::Printf(
						TEXT("%s has no compatible Atom BP candidate for %d actions."),
						*RouteLabel,
						Entry.Actions.Num());
					return false;
				}
				continue;
			}

			const TSoftClassPtr<ANightCourseAtomActor>* AtomClassPtr =
				AtomLibrary->AtomMap.Find(Key);
			if (!AtomClassPtr)
			{
				OutError = FString::Printf(
					TEXT("%s AtomKey '%s' is not registered in the Atom Library."),
					*RouteLabel,
					*Key);
				return false;
			}

			UClass* AtomClass = AtomClassPtr->LoadSynchronous();
			if (!AtomClass || !AtomClass->IsChildOf(ANightCourseAtomActor::StaticClass()))
			{
				OutError = FString::Printf(
					TEXT("%s AtomKey '%s' does not resolve to an Atom BP."),
					*RouteLabel,
					*Key);
				return false;
			}

			const ANightCourseAtomActor* AtomCDO =
				Cast<ANightCourseAtomActor>(AtomClass->GetDefaultObject());
			if (!AtomCDO)
			{
				OutError = FString::Printf(
					TEXT("%s AtomKey '%s' has no valid Atom CDO."),
					*RouteLabel,
					*Key);
				return false;
			}

			const int32 LandingPointCount = AtomCDO->GetLandingPointCount();
			// BP SCS component templates are not instantiated on every CDO in the
			// editor. The Composer performs the authoritative count check on a
			// transient Atom instance; enforce it here whenever the CDO exposes it.
			if (LandingPointCount > 0
				&& Entry.Actions.Num() != LandingPointCount - 1)
			{
				OutError = FString::Printf(
					TEXT("%s '%s' has %d actions but Atom BP has %d landing points (expected %d actions)."),
					*RouteLabel,
					*Key,
					Entry.Actions.Num(),
					LandingPointCount,
					LandingPointCount - 1);
				return false;
			}
		}
		return true;
	};

	if (!ValidateEntries(BaseRoute, TEXT("Base route")))
	{
		return false;
	}
	for (const TPair<ENightRouteId, FNightRuleAtomQueue>& Pair : BranchRoutes)
	{
		if (!ValidateEntries(
			Pair.Value.Atoms,
			FString::Printf(
				TEXT("Branch route %s"),
				*NightCourseRule_Private::RouteIdToString(Pair.Key))))
		{
			return false;
		}
	}
	return true;
}

bool UNightCourseRuleData::HasBranchRoute(const ENightRouteId RouteId) const
{
	const FNightRuleAtomQueue* Queue = BranchRoutes.Find(RouteId);
	return Queue && Queue->Atoms.Num() > 0;
}

int32 UNightCourseRuleData::GetBaseRouteLength() const
{
	return BaseAtomCount > 0 ? BaseAtomCount : BaseRoute.Num();
}

bool UNightCourseRuleData::ImportJson(const FString& JsonText, FString& OutError)
{
	OutError.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Rule JSON is not a valid object.");
		return false;
	}

	double JsonSeed = 0.0;
	if (!Root->TryGetNumberField(TEXT("seed"), JsonSeed))
	{
		OutError = TEXT("Rule JSON requires numeric 'seed'.");
		return false;
	}
	double JsonBaseAtomCount = 0.0;
	const bool bHasBaseAtomCount =
		Root->TryGetNumberField(TEXT("baseAtomCount"), JsonBaseAtomCount);

	TArray<FNightRuleAtomEntry> ImportedBaseRoute;
	const TArray<TSharedPtr<FJsonValue>>* JsonBaseRoute = nullptr;
	if (!Root->TryGetArrayField(TEXT("baseRoute"), JsonBaseRoute)
		|| !NightCourseRule_Private::ParseEntryArray(
			JsonBaseRoute,
			TEXT("baseRoute"),
			ImportedBaseRoute,
			OutError))
	{
		return false;
	}

	TMap<ENightRouteId, FNightRuleAtomQueue> ImportedBranchRoutes;
	const TSharedPtr<FJsonObject>* JsonBranchRoutes = nullptr;
	if (Root->TryGetObjectField(TEXT("branchRoutes"), JsonBranchRoutes)
		&& JsonBranchRoutes
		&& JsonBranchRoutes->IsValid())
	{
		static const TCHAR* RouteNames[] = { TEXT("A"), TEXT("B"), TEXT("C") };
		for (const TCHAR* RouteName : RouteNames)
		{
			const TArray<TSharedPtr<FJsonValue>>* JsonBranch = nullptr;
			const TSharedPtr<FJsonObject>* JsonQueueObject = nullptr;
			const bool bIsLegacyArray =
				(*JsonBranchRoutes)->TryGetArrayField(RouteName, JsonBranch);
			const bool bIsQueueObject =
				!bIsLegacyArray
				&& (*JsonBranchRoutes)->TryGetObjectField(RouteName, JsonQueueObject)
				&& JsonQueueObject
				&& JsonQueueObject->IsValid();
			if (!bIsLegacyArray && !bIsQueueObject)
			{
				continue;
			}

			ENightRouteId RouteId = ENightRouteId::None;
			if (!NightCourseRule_Private::ParseRouteId(RouteName, RouteId))
			{
				OutError = FString::Printf(
					TEXT("branchRoutes contains unsupported route '%s'."),
					RouteName);
				return false;
			}

			FNightRuleAtomQueue Queue;
			if (bIsQueueObject)
			{
				double JsonTargetAtomCount = 0.0;
				if ((*JsonQueueObject)->TryGetNumberField(
					TEXT("targetAtomCount"),
					JsonTargetAtomCount))
				{
					Queue.TargetAtomCount =
						FMath::RoundToInt(static_cast<float>(JsonTargetAtomCount));
				}
				if (!(*JsonQueueObject)->TryGetArrayField(TEXT("atoms"), JsonBranch)
					|| !JsonBranch)
				{
					OutError = FString::Printf(
						TEXT("branchRoutes.%s object requires array 'atoms'."),
						RouteName);
					return false;
				}
			}
			if (!NightCourseRule_Private::ParseEntryArray(
				JsonBranch,
				FString::Printf(TEXT("branchRoutes.%s"), RouteName),
				Queue.Atoms,
				OutError))
			{
				return false;
			}
			ImportedBranchRoutes.Add(RouteId, MoveTemp(Queue));
		}
	}

	double JsonForkAfterBaseAtomIndex = INDEX_NONE;
	const bool bHasForkAfterBaseAtomIndex =
		Root->TryGetNumberField(TEXT("forkAfterBaseAtomIndex"), JsonForkAfterBaseAtomIndex);
	bool ImportedAutoSelectAtomKeys = true;
	Root->TryGetBoolField(TEXT("autoSelectAtomKeys"), ImportedAutoSelectAtomKeys);

	UNightCourseRuleData* MutableThis = this;
	const int32 PreviousSeed = MutableThis->Seed;
	const int32 PreviousBaseAtomCount = MutableThis->BaseAtomCount;
	const TArray<FNightRuleAtomEntry> PreviousBaseRoute = MutableThis->BaseRoute;
	const TMap<ENightRouteId, FNightRuleAtomQueue> PreviousBranchRoutes = MutableThis->BranchRoutes;
	const int32 PreviousForkAfterBaseAtomIndex = MutableThis->ForkAfterBaseAtomIndex;
	const bool PreviousAutoSelectAtomKeys = MutableThis->bAutoSelectAtomKeys;
	const FString PreviousEditorJson = MutableThis->EditorJson;
	MutableThis->Modify();
	MutableThis->Seed = static_cast<int32>(JsonSeed);
	MutableThis->BaseAtomCount = bHasBaseAtomCount
		? FMath::RoundToInt(static_cast<float>(JsonBaseAtomCount))
		: 0;
	MutableThis->BaseRoute = MoveTemp(ImportedBaseRoute);
	MutableThis->BranchRoutes = MoveTemp(ImportedBranchRoutes);
	MutableThis->ForkAfterBaseAtomIndex = bHasForkAfterBaseAtomIndex
		? static_cast<int32>(JsonForkAfterBaseAtomIndex)
		: INDEX_NONE;
	MutableThis->bAutoSelectAtomKeys = ImportedAutoSelectAtomKeys;
	MutableThis->EditorJson = JsonText;

	if (!ValidateRule(OutError))
	{
		MutableThis->Seed = PreviousSeed;
		MutableThis->BaseAtomCount = PreviousBaseAtomCount;
		MutableThis->BaseRoute = PreviousBaseRoute;
		MutableThis->BranchRoutes = PreviousBranchRoutes;
		MutableThis->ForkAfterBaseAtomIndex = PreviousForkAfterBaseAtomIndex;
		MutableThis->bAutoSelectAtomKeys = PreviousAutoSelectAtomKeys;
		MutableThis->EditorJson = PreviousEditorJson;
		return false;
	}
	MarkPackageDirtyForEditor();
	return true;
}

bool UNightCourseRuleData::ExportJson(FString& OutJson, FString& OutError) const
{
	OutJson.Reset();
	OutError.Reset();
	if (!ValidateRule(OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("seed"), Seed);
	Root->SetBoolField(TEXT("autoSelectAtomKeys"), bAutoSelectAtomKeys);
	Root->SetNumberField(TEXT("baseAtomCount"), BaseAtomCount);

	TArray<TSharedPtr<FJsonValue>> JsonBaseRoute;
	NightCourseRule_Private::WriteEntryArray(BaseRoute, JsonBaseRoute);
	Root->SetArrayField(TEXT("baseRoute"), MoveTemp(JsonBaseRoute));

	if (BranchRoutes.Num() > 0)
	{
		TSharedRef<FJsonObject> JsonBranches = MakeShared<FJsonObject>();
		for (const TPair<ENightRouteId, FNightRuleAtomQueue>& Pair : BranchRoutes)
		{
			TArray<TSharedPtr<FJsonValue>> JsonBranch;
			NightCourseRule_Private::WriteEntryArray(Pair.Value.Atoms, JsonBranch);
			TSharedRef<FJsonObject> JsonQueue = MakeShared<FJsonObject>();
			JsonQueue->SetNumberField(TEXT("targetAtomCount"), Pair.Value.TargetAtomCount);
			JsonQueue->SetArrayField(TEXT("atoms"), MoveTemp(JsonBranch));
			JsonBranches->SetObjectField(
				NightCourseRule_Private::RouteIdToString(Pair.Key),
				JsonQueue);
		}
		Root->SetObjectField(TEXT("branchRoutes"), JsonBranches);
	}
	if (ForkAfterBaseAtomIndex != INDEX_NONE)
	{
		Root->SetNumberField(TEXT("forkAfterBaseAtomIndex"), ForkAfterBaseAtomIndex);
	}

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutError = TEXT("Failed to serialize rule JSON.");
		return false;
	}
	return true;
}

void UNightCourseRuleData::ImportEditorJson()
{
	FString Error;
	if (!ImportJson(EditorJson, Error))
	{
		UE_LOG(LogNightCourseRule, Error, TEXT("Import rule JSON failed for '%s': %s"), *GetName(), *Error);
	}
	else
	{
		UE_LOG(LogNightCourseRule, Display, TEXT("Imported rule JSON into '%s'."), *GetName());
	}
}

void UNightCourseRuleData::ExportEditorJson()
{
	FString ExportedJson;
	FString Error;
	if (!ExportJson(ExportedJson, Error))
	{
		UE_LOG(LogNightCourseRule, Error, TEXT("Export rule JSON failed for '%s': %s"), *GetName(), *Error);
		return;
	}
	Modify();
	EditorJson = MoveTemp(ExportedJson);
	MarkPackageDirtyForEditor();
	UE_LOG(LogNightCourseRule, Display, TEXT("Exported rule JSON into EditorJson on '%s'."), *GetName());
}

void UNightCourseRuleData::MarkPackageDirtyForEditor()
{
	Modify();
	MarkPackageDirty();
}
#pragma endregion K2 moonyfli
