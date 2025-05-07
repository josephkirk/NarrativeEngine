// Copyright Joseph Kirk 2025

#pragma once

#include "CoreMinimal.h"
#include "FlowInkNodeBase.h"
#include "Engine/EngineTypes.h" // For FTimerHandle
#include "FlowInkObservationNode.generated.h"

/**
 * FlowNode that observes a specific Ink variable and triggers an output
 * when its value changes. It polls the variable at a configurable interval.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Ink Observe Variable"))
class NARRATIVEENGINE_API UFlowInkObservationNode : public UFlowInkNodeBase
{
	GENERATED_UCLASS_BODY()

protected:
	// The name of the Ink variable to observe (e.g., "player_gold", "chapter_progress").
	UPROPERTY(EditAnywhere, Category = "Ink", meta = (Tooltip = "Name of the Ink variable to observe."))
	FString InkVariableName;

	// How often to check the Ink variable's value, in seconds.
	UPROPERTY(EditAnywhere, Category = "Ink", meta = (ClampMin = "0.1", Tooltip = "Polling interval in seconds.", DefaultForInputFlowPin, FlowPinType = Float))
	float PollingInterval;

	// Output execution pin triggered when the observed Ink variable's value changes.
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode", meta = (ToolTip = "Triggered when the observed variable's value changes."))
	FFlowPin VariableChangedPin;

	// Output data pin providing the new string value of the observed variable when it changes.
	// Note: Actual pin setup for data pins is in the .cpp (e.g., in constructor or AllocateDefaultPins).

	// Output pin triggered if an error occurs during activation/setup (e.g., empty variable name, invalid interval, subsystem unavailable).
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode", meta = (ToolTip = "Triggered on error during setup or activation."))
	FFlowPin ErrorPin;

	// Stores the last known string value of the observed variable for comparison.
	UPROPERTY(SaveGame, Transient)
	FString LastKnownValue;

	// Timer handle for polling the Ink variable.
	UPROPERTY(SaveGame)
	FTimerHandle PollingTimerHandle;

	// Stores remaining time for the timer, used for SaveGame compatibility
	UPROPERTY(SaveGame)
	float RemainingPollingTime;

public:
	//~ Begin UFlowNode Interface
	virtual void ExecuteInput(const FName& PinName) override;
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
	virtual void Cleanup() override; // Important for clearing timers
	virtual void OnSave_Implementation() override;
	virtual void OnLoad_Implementation() override;
#if WITH_EDITOR
	virtual FString GetNodeDescription() const override;
	virtual FString GetStatusString() const override;
	virtual void AllocateDefaultPins() override;
#endif
	//~ End UFlowNode Interface

private:
	// Timer callback function to poll the Ink variable.
	UFUNCTION()
	void PollInkVariable();

	// Helper to start the polling timer
	void StartPolling();

	// Helper to stop the polling timer
	void StopPolling();
};
