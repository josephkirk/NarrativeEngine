// Copyright Joseph Kirk 2025

#pragma once

#include "CoreMinimal.h"
#include "Nodes/FlowNode.h"
#include "GameplayTagContainer.h"
#include "GameplayMessageSubsystem.h"
#include "Types/InkEventMessageTypes.h" // For FInkExternalFunctionMessage
#include "FlowInkGlobalEventNode.generated.h"

/**
 * A FlowNode that listens for globally broadcasted Ink external function calls via Gameplay Messages.
 * Triggers an output when a matching function call is received on the specified channel.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Ink Global Event"))
class NARRATIVEENGINE_API UFlowInkGlobalEventNode : public UFlowNode
{
	GENERATED_UCLASS_BODY()

protected:
	// The Gameplay Tag channel to listen for Ink external function messages on.
	UPROPERTY(EditAnywhere, Category = "Ink Event", meta = (Categories = "Ink.Event"))
	FGameplayTag EventChannelToListen;

	// Optional: If set, only events with this specific function name will trigger the node.
	// If empty, any function call on the EventChannel will trigger it.
	UPROPERTY(EditAnywhere, Category = "Ink Event")
	FName ExpectedFunctionName;

	// Output pin triggered when a matching Ink external function is called.
	UPROPERTY(EditAnywhere, Category = "Pins")
	FFlowOutputPin FunctionCalledPin;

	// Output data pin for the actual name of the function that was called.
	UPROPERTY(EditAnywhere, Category = "Pins", meta = (DisplayName = "Function Name"))
	FFlowPropertyFName ActualFunctionNamePin;

	// Output data pin for the arguments passed to the function.
	UPROPERTY(EditAnywhere, Category = "Pins", meta = (DisplayName = "Arguments"))
	FFlowPropertyTArrayString ArgumentsPin;

	// Output pin triggered if an error occurs during activation (e.g., invalid channel, subsystem unavailable).
	UPROPERTY(EditAnywhere, Category = "Pins", meta = (DisplayName = "Error", ToolTip = "Triggered if an error occurs during activation."))
	FFlowOutputPin ErrorPin;

private:
	FGameplayMessageListenerHandle MessageListenerHandle;

	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
	
	// Called when a Gameplay Message is received on the subscribed channel.
	void HandleInkEventMessage(FGameplayTag Channel, const FInkExternalFunctionMessage& Message);

public:
#if WITH_EDITOR
	virtual FString GetNodeDescription() const override;
	virtual FText GetNodeTitle() const override;
	virtual EFlowNodeStyle GetNodeStyle() const override;
#endif
};
