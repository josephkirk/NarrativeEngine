// Copyright Joseph Kirk 2025

#pragma once

#include "FlowInkNodeBase.h"
#include "FlowInkConditionNode.generated.h"

/**
 * FlowNode to evaluate an Ink story condition.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Ink Condition"))
class NARRATIVEENGINE_API UFlowInkConditionNode : public UFlowInkNodeBase
{
	GENERATED_UCLASS_BODY()

protected:
	// The name of the Ink function or variable to evaluate as a condition.
	// Ink functions used as conditions should return a boolean (or 0/1).
	UPROPERTY(EditAnywhere, Category = "Ink")
	FName ConditionName; 

	// Output pin if the condition is true
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	FFlowPin TruePin;

	// Output pin if the condition is false
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	FFlowPin FalsePin;

	// Output pin if an error occurs (e.g. subsystem unavailable, condition name not set, variable not found)
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode", meta=(ToolTip="Triggered on error evaluating condition."))
	FFlowPin ErrorPin;

public:
	virtual void ExecuteInput(const FName& PinName) override;

#if WITH_EDITOR
	virtual FString GetNodeDescription() const override;
	virtual FString GetStatusString() const override;
#endif
};
