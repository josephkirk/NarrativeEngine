// Copyright Joseph Kirk 2025

#pragma once

#include "FlowInkNodeBase.h"
#include "FlowInkTextNode.generated.h"

/**
 * FlowNode to display text from an Ink story.
 * It fetches the current line from the Ink runner and provides it via an output data pin.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Ink Text"))
class NARRATIVEENGINE_API UFlowInkTextNode : public UFlowInkNodeBase
{
	GENERATED_UCLASS_BODY()

protected:
	// Output pin to trigger when text has been processed/displayed
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	FFlowPin FinishedPin;

	// Output pin that is triggered if an error occurs during execution or no text is available
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	FFlowPin ErrorPin;

	// Runtime fetched text, will be exposed via an Output Data Pin "TextOut"
	UPROPERTY(Transient, VisibleInstanceOnly, Category="Ink", meta=(DisplayName="Current Text"))
	FText FetchedText;

public:
	virtual void ExecuteInput(const FName& PinName) override;

	//~ Begin UFlowNode Interface
	virtual void OnLoad_Implementation() override;
	//~ End UFlowNode Interface

#if WITH_EDITOR
	virtual FString GetNodeDescription() const override;
	virtual FString GetStatusString() const override;
#endif

protected:
	void SetupDataPins(); // To add TextOut pin
};
