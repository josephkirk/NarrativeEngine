// Copyright Joseph Kirk 2025

#pragma once

#include "CoreMinimal.h"
#include "Nodes/FlowNode.h"
#include "FlowPropertyVariant.h" // Assuming FFlowPropertyVariant is available for generic input
#include "FlowInkSetVariableNode.generated.h"

/**
 * A FlowNode that sets the value of a specified Ink variable.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Set Ink Variable"))
class NARRATIVEENGINE_API UFlowInkSetVariableNode : public UFlowNode
{
	GENERATED_UCLASS_BODY()

protected:
	// The name of the Ink variable to set (e.g., "player_gold").
	UPROPERTY(EditAnywhere, Category = "Ink")
	FString InkVariableName;

	// The new value to set the Ink variable to. 
	// The node will attempt to convert this to an appropriate Ink type (bool, int, float, string).
	UPROPERTY(EditAnywhere, Category = "Ink", meta = (DisplayName = "New Value"))
	FFlowPropertyVariant ValueToSet;

	// Output pin to indicate if the variable was successfully set.
	UPROPERTY(EditAnywhere, Category = "Pins", meta = (DisplayName = "Success"))
	FFlowPropertyBool bSuccessPin;

	// Output pin triggered if a critical error occurs during setup (e.g., empty variable name, subsystem unavailable).
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	FFlowPin ErrorPin;

	virtual void ExecuteInput(const FName& PinName) override;

public:
#if WITH_EDITOR
	virtual FString GetNodeDescription() const override;
	virtual FText GetNodeTitle() const override;
#endif
};
