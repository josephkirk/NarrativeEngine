// Copyright Epic Games, Inc. All Rights Reserved.

#include "NarrativeEngine.h"

#define LOCTEXT_NAMESPACE "FNarrativeEngineModule"

// Define the implementation for the log category declared in the header
DEFINE_LOG_CATEGORY(LogNarrativeEngine);

void FNarrativeEngineModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogNarrativeEngine, Log, TEXT("NarrativeEngine Module Started"));
}

void FNarrativeEngineModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UE_LOG(LogNarrativeEngine, Log, TEXT("NarrativeEngine Module Shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNarrativeEngineModule, NarrativeEngine)