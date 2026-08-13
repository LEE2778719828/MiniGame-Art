#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#pragma region K2 moonyfli
class FMiniGameModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
#pragma endregion K2 moonyfli
