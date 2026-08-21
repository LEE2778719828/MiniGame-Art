#include "Night/Course/NightG1CourseConfig.h"
#include "UObject/Package.h"

#pragma region K2 moonyfli
void UNightG1CourseConfig::MarkPackageDirtyForEditor()
{
	Modify();
	if (UPackage* Package = GetOutermost())
	{
		Package->SetDirtyFlag(true);
	}
}

#pragma endregion K2 moonyfli
