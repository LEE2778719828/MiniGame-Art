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
}

bool UNightCourseRuleData::ValidateRule(FString& OutError) const
{
	OutError.Reset();
	if (!bEnabled)
	{
		return true;
	}
	if (Route.Num() == 0)
	{
		OutError = TEXT("Rule route is empty.");
		return false;
	}
	if (TransitionAction != ENightNodeKind::Hazard)
	{
		OutError = TEXT("TransitionAction must be Jump/Hazard.");
		return false;
	}

	for (int32 EntryIndex = 0; EntryIndex < Route.Num(); ++EntryIndex)
	{
		const FNightRuleAtomEntry& Entry = Route[EntryIndex];
		const FString Key = Entry.AtomKey.TrimStartAndEnd();
		if (Key.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Route entry %d has an empty AtomKey."), EntryIndex);
			return false;
		}

		for (int32 ActionIndex = 0; ActionIndex < Entry.Actions.Num(); ++ActionIndex)
		{
			if (Entry.Actions[ActionIndex] != ENightNodeKind::Enemy
				&& Entry.Actions[ActionIndex] != ENightNodeKind::Hazard)
			{
				OutError = FString::Printf(
					TEXT("Rule '%s' action %d is None/unsupported."),
					*Key,
					ActionIndex);
				return false;
			}
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

	for (const FNightRuleAtomEntry& Entry : Route)
	{
		const TSoftClassPtr<ANightCourseAtomActor>* AtomClassPtr = AtomLibrary->AtomMap.Find(Entry.AtomKey);
		if (!AtomClassPtr)
		{
			OutError = FString::Printf(TEXT("Rule AtomKey '%s' is not registered in the Atom Library."), *Entry.AtomKey);
			return false;
		}

		UClass* AtomClass = AtomClassPtr->LoadSynchronous();
		if (!AtomClass || !AtomClass->IsChildOf(ANightCourseAtomActor::StaticClass()))
		{
			OutError = FString::Printf(TEXT("Rule AtomKey '%s' does not resolve to an Atom BP."), *Entry.AtomKey);
			return false;
		}

		const ANightCourseAtomActor* AtomCDO = Cast<ANightCourseAtomActor>(AtomClass->GetDefaultObject());
		if (!AtomCDO)
		{
			OutError = FString::Printf(TEXT("Rule AtomKey '%s' has no valid Atom CDO."), *Entry.AtomKey);
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
				TEXT("Rule '%s' has %d actions but Atom BP has %d landing points (expected %d actions)."),
				*Entry.AtomKey,
				Entry.Actions.Num(),
				LandingPointCount,
				LandingPointCount - 1);
			return false;
		}
	}
	return true;
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

	ENightNodeKind ImportedTransitionAction = ENightNodeKind::Hazard;
	FString TransitionText;
	if (Root->TryGetStringField(TEXT("transitionAction"), TransitionText)
		&& (!NightCourseRule_Private::ParseAction(TransitionText, ImportedTransitionAction)
			|| ImportedTransitionAction != ENightNodeKind::Hazard))
	{
		OutError = TEXT("Rule JSON transitionAction must be Jump.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonRoute = nullptr;
	if (!Root->TryGetArrayField(TEXT("route"), JsonRoute) || !JsonRoute)
	{
		OutError = TEXT("Rule JSON requires array 'route'.");
		return false;
	}

	TArray<FNightRuleAtomEntry> ImportedRoute;
	ImportedRoute.Reserve(JsonRoute->Num());
	for (int32 EntryIndex = 0; EntryIndex < JsonRoute->Num(); ++EntryIndex)
	{
		const TSharedPtr<FJsonObject> EntryObject = (*JsonRoute)[EntryIndex].IsValid()
			? (*JsonRoute)[EntryIndex]->AsObject()
			: nullptr;
		if (!EntryObject.IsValid())
		{
			OutError = FString::Printf(TEXT("route[%d] is not an object."), EntryIndex);
			return false;
		}

		FNightRuleAtomEntry Entry;
		if (!EntryObject->TryGetStringField(TEXT("atomKey"), Entry.AtomKey))
		{
			OutError = FString::Printf(TEXT("route[%d] requires string 'atomKey'."), EntryIndex);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* JsonActions = nullptr;
		if (!EntryObject->TryGetArrayField(TEXT("actions"), JsonActions) || !JsonActions)
		{
			OutError = FString::Printf(TEXT("route[%d] requires array 'actions'."), EntryIndex);
			return false;
		}

		for (int32 ActionIndex = 0; ActionIndex < JsonActions->Num(); ++ActionIndex)
		{
			FString ActionText;
			if (!(*JsonActions)[ActionIndex].IsValid()
				|| !(*JsonActions)[ActionIndex]->TryGetString(ActionText))
			{
				OutError = FString::Printf(TEXT("route[%d].actions[%d] must be a string."), EntryIndex, ActionIndex);
				return false;
			}

			ENightNodeKind Action = ENightNodeKind::None;
			if (!NightCourseRule_Private::ParseAction(ActionText, Action))
			{
				OutError = FString::Printf(
					TEXT("route[%d].actions[%d] has unsupported action '%s'."),
					EntryIndex,
					ActionIndex,
					*ActionText);
				return false;
			}
			Entry.Actions.Add(Action);
		}
		ImportedRoute.Add(MoveTemp(Entry));
	}

	UNightCourseRuleData* MutableThis = this;
	const int32 PreviousSeed = MutableThis->Seed;
	const TArray<FNightRuleAtomEntry> PreviousRoute = MutableThis->Route;
	const ENightNodeKind PreviousTransitionAction = MutableThis->TransitionAction;
	const FString PreviousEditorJson = MutableThis->EditorJson;
	MutableThis->Modify();
	MutableThis->Seed = static_cast<int32>(JsonSeed);
	MutableThis->Route = MoveTemp(ImportedRoute);
	MutableThis->TransitionAction = ImportedTransitionAction;
	MutableThis->EditorJson = JsonText;

	if (!ValidateRule(OutError))
	{
		MutableThis->Seed = PreviousSeed;
		MutableThis->Route = PreviousRoute;
		MutableThis->TransitionAction = PreviousTransitionAction;
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
	Root->SetStringField(TEXT("transitionAction"), TEXT("Jump"));

	TArray<TSharedPtr<FJsonValue>> JsonRoute;
	for (const FNightRuleAtomEntry& Entry : Route)
	{
		TSharedRef<FJsonObject> JsonEntry = MakeShared<FJsonObject>();
		JsonEntry->SetStringField(TEXT("atomKey"), Entry.AtomKey);

		TArray<TSharedPtr<FJsonValue>> JsonActions;
		for (const ENightNodeKind Action : Entry.Actions)
		{
			JsonActions.Add(MakeShared<FJsonValueString>(NightCourseRule_Private::ActionToString(Action)));
		}
		JsonEntry->SetArrayField(TEXT("actions"), MoveTemp(JsonActions));
		JsonRoute.Add(MakeShared<FJsonValueObject>(JsonEntry));
	}
	Root->SetArrayField(TEXT("route"), MoveTemp(JsonRoute));

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
