// Copyright Joseph Kirk 2025

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/InkEventMessageTypes.h" // Added for FInkExternalFunctionMessage
#include "GameplayMessageSubsystem.h" // Added for UGameplayMessageSubsystem
#include "InkChoiceInfo.h" // Added for FInkChoiceInfo
#include "InkNarrativeSubsystem.generated.h"

// Forward declarations for InkCPP types
namespace ink { namespace runtime { class story; class runner; class globals; class value; class choice; } }
class UInkAsset; // Forward declare UInkAsset

/**
 * Manages the Ink story runtime, including loading stories and providing access to the story and runner.
 */
UCLASS()
class NARRATIVEENGINE_API UInkNarrativeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Default FName for the main story runner.
	static const FName MainStoryRunnerName;

	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/**
	 * Loads an Ink story from the given compiled JSON file path.
	 * @param StoryAssetPath The path to the compiled Ink JSON asset (e.g., /Game/InkStories/MyStory.MyStory_Ink)
	 * @return True if the story was loaded successfully, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ink Narrative", meta=(DeprecatedFunction, DeprecationMessage="Use LoadStoryForRunner with UInkAsset instead."))
	bool LoadStory(const FString& StoryAssetPath);

	/**
	 * Loads the given Ink story asset and prepares it for execution on the specified runner.
	 * If a story is already loaded on that runner, it will be replaced.
	 * @param StoryAsset The UInkAsset to load.
	 * @param RunnerName The FName identifier for the runner this story will be associated with. Defaults to MainStoryRunnerName.
	 * @return True if the story was successfully loaded, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ink Narrative Subsystem")
	bool LoadStoryForRunner(UInkAsset* StoryAsset, FName RunnerName = MainStoryRunnerName);

	/** Gets the currently loaded Ink story instance. DEPRECATED: Use GetRunner and GetGlobals for specific runner data. */
	UFUNCTION(BlueprintPure, Category = "Ink Narrative", meta=(DeprecatedFunction, DeprecationMessage="Use GetRunner and GetGlobals for specific runner data. This refers to the MainStory runner's story if loaded."))
	ink::runtime::story* GetStory() const { return CurrentStory; }

	/**
	 * Retrieves an active Ink runner by its name.
	 * @param RunnerName The name of the runner to retrieve. Defaults to MainStoryRunnerName.
	 * @return Pointer to the ink::runtime::runner, or nullptr if not found or not valid.
	 */
	ink::runtime::runner* GetRunner(FName RunnerName = MainStoryRunnerName);
	
	/**
	 * Retrieves the global variables store for a given runner.
	 * @param RunnerName The name of the runner whose globals are to be retrieved. Defaults to MainStoryRunnerName.
	 * @return Pointer to the ink::runtime::globals, or nullptr if not found or not valid.
	 */
	ink::runtime::globals* GetGlobals(FName RunnerName = MainStoryRunnerName);

	/** Gets the main Ink runner instance. DEPRECATED: Use GetRunner(MainStoryRunnerName). */
	UFUNCTION(BlueprintPure, Category = "Ink Narrative", meta=(DeprecatedFunction, DeprecationMessage="Use GetRunner with MainStoryRunnerName instead."))
	ink::runtime::runner* GetMainRunner() const { return MainRunner; }

	/** Checks if a story is currently loaded on the specified runner. */
	UFUNCTION(BlueprintPure, Category = "Ink Narrative")
	bool IsStoryLoaded(FName RunnerName = MainStoryRunnerName) const;

	/**
	 * Gets the value of an Ink variable as a string. DEPRECATED: Use GetVariable.
	 * @param VariableName The name of the Ink variable (e.g., "player_gold").
	 * @param bFound Set to true if the variable was found, false otherwise.
	 * @return The string representation of the variable's value, or an empty string if not found or if the story/runner is not loaded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ink Narrative", meta=(DeprecatedFunction, DeprecationMessage="Use GetVariable instead."))
	FString GetVariableValueAsString(const FString& VariableName, bool& bFound) const;

	/**
	 * Binds an Ink external function to broadcast a GameplayMessage when called.
	 * The message will be of type FInkExternalFunctionMessage and broadcast on the specified channel tag.
	 * @param FunctionName The name of the external function in the Ink story.
	 * @param MessageChannelTag The GameplayTag channel to broadcast the message on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ink Narrative", meta=(AdvancedDisplay="MessageChannelTag"))
	void BindExternalFunctionWithMessage(const FString& FunctionName, FGameplayTag MessageChannelTag = FGameplayTag::RequestGameplayTag(FName("Ink.Event.ExternalFunctionCall")));

	/**
	 * Sets the value of an Ink variable from a string. DEPRECATED: Use SetVariable.
	 * The subsystem will attempt to convert the string to the appropriate Ink type.
	 * @param VariableName The name of the Ink variable (e.g., "player_gold").
	 * @param ValueAsString The new value for the variable, as a string.
	 * @return True if the variable was set successfully, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ink Narrative", meta=(DeprecatedFunction, DeprecationMessage="Use SetVariable instead."))
	bool SetVariableValueFromString(const FString& VariableName, const FString& ValueAsString);

	// Gets the value of an Ink variable, attempting to interpret it as a boolean. DEPRECATED: Use GetVariable and parse manually or use specific GetBoolVariable nodes if created.
	// Returns the variable's boolean value in OutValue.
	// Sets bFound to true if the variable exists, false otherwise.
	// Returns true if the variable exists AND could be interpreted as a boolean, false otherwise.
	UFUNCTION(BlueprintCallable, Category = "Ink Narrative", meta=(DeprecatedFunction, DeprecationMessage="Use GetVariable and parse manually or use specific GetBoolVariable nodes."))
	bool GetVariableValueAsBool(const FString& VariableName, bool& bFound, bool& OutValue) const;

	// --- New Multi-Runner Compatible Methods ---

	UFUNCTION(BlueprintCallable, Category = "Ink Narrative Subsystem")
	void MakeChoice(int32 ChoiceIndex, FName RunnerName = MainStoryRunnerName);

	UFUNCTION(BlueprintPure, Category = "Ink Narrative Subsystem")
	FString GetCurrentText(FName RunnerName = MainStoryRunnerName);

	UFUNCTION(BlueprintPure, Category = "Ink Narrative Subsystem")
	TArray<FInkChoiceInfo> GetCurrentChoices(FName RunnerName = MainStoryRunnerName);
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInkVariableChanged, FName, VariableName, const FString&, NewValue);

	UPROPERTY(BlueprintAssignable, Category = "Ink Narrative Subsystem")
	FOnInkVariableChanged OnInkVariableChanged;

	UFUNCTION(BlueprintCallable, Category = "Ink Narrative Subsystem")
	void ObserveVariable(const FString& VariableName, FName RunnerName = MainStoryRunnerName);

	/** Sets an Ink variable's value, attempting to parse the FString to the correct type. */
	UFUNCTION(BlueprintCallable, Category = "Ink Narrative Subsystem")
	bool SetVariable(const FString& VariableName, const FString& Value, FName RunnerName = MainStoryRunnerName);
	
	/** Gets an Ink variable's value as FString. */
	UFUNCTION(BlueprintPure, Category = "Ink Narrative Subsystem")
	FString GetVariable(const FString& VariableName, FName RunnerName = MainStoryRunnerName);

	/**
	 * Converts an ink::runtime::value to an FString for logging or display.
	 * @param InkValue The ink value to convert.
	 * @param PropertyNameForLogging Optional FName of the property, used for logging if conversion is complex.
	 * @return FString representation of the InkValue.
	 */
	FString ConvertInkValueToString(const ink::runtime::value& InkValue, const FName& PropertyNameForLogging = NAME_None) const;

protected:
	// Called to clean up a specific runner and its associated story and globals.
	void ReleaseRunnerResources(FName RunnerName);

	// Callback for InkCPP variable observation
	void OnInkVariableUpdate(const char* VarName, ink::runtime::value NewInkValue, FName RunnerName);

private:
	// Map of active Ink stories, keyed by runner name.
	// UPROPERTY() // Not strictly needed for raw pointers, but good if these were UObject wrappers
	TMap<FName, ink::runtime::story*> LoadedStories;

	// Map of Ink global variable stores, keyed by runner name.
	TMap<FName, ink::runtime::globals*> GlobalVariables;

	// Map of active Ink runners, keyed by runner name.
	TMap<FName, ink::runtime::runner*> ActiveRunners;

	// The current Ink story instance
	ink::runtime::story* CurrentStory = nullptr;

	// The main runner for the current story
	ink::runtime::runner* MainRunner = nullptr;

	// Helper to clean up story and runner - DEPRECATED
	// void UnloadStory(); // Removed, ReleaseRunnerResources handles this per runner
};
