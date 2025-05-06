// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Define a log category for the NarrativeEngine module
DECLARE_LOG_CATEGORY_EXTERN(LogNarrativeEngine, Log, All);

class FNarrativeEngineModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
