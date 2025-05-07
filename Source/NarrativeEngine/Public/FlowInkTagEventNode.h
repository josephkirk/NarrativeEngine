// Copyright Joseph Kirk 2025

#pragma once

#include "FlowInkNodeBase.h"
#include "FlowInkTagEventNode.generated.h"

/**
 * FlowNode that listens for specific tags emitted by the Ink story.
 * When a monitored tag is encountered, it triggers an output execution pin
 * and provides the tag as an output data pin.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Ink Tag Event"))
class NARRATIVEENGINE_API UFlowInkTagEventNode : public UFlowInkNodeBase
{
	GENERATED_UCLASS_BODY()

protected:
	// The specific tag to listen for. If empty, the node will trigger for any tag encountered.
	UPROPERTY(EditAnywhere, Category = "Ink", meta = (Tooltip = "The specific tag to listen for. If empty, triggers for any tag."))
	FString TagToListenFor;

	// Output execution pin triggered when the specified tag (or any tag, if TagToListenFor is empty) is matched.
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode", meta = (ToolTip = "Triggered when a matching tag is encountered."))
	FFlowPin TagMatchedPin;

	// Output data pin providing the string value of the tag that was matched.
	// This will be of type FString.
	// Note: Actual pin setup for data pins is in the .cpp (e.g., in constructor or AllocateDefaultPins).

	// Output pin triggered if an error occurs during activation/setup (e.g., subsystem unavailable, unable to subscribe to Ink events).
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode", meta = (ToolTip = "Triggered on error during setup or activation."))
	FFlowPin ErrorPin;

	// The tag that was last matched by this node.
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Ink", meta=(DisplayName="Last Matched Tag"))
	FString MatchedTagValue;

public:
	//~ Begin UFlowNode Interface
	virtual void ExecuteInput(const FName& PinName) override;
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
#if WITH_EDITOR
	virtual FString GetNodeDescription() const override;
	virtual FString GetStatusString() const override;
	virtual void AllocateDefaultPins() override;
#endif
	//~ End UFlowNode Interface

private:
	// Handler for the Ink runner's tag event
	void HandleInkTagEvent(const char* Tag);

	// Helper to (un)subscribe to Ink tag events
	void TrySubscribeToTagEvents();
	void TryUnsubscribeFromTagEvents();

	// Store the delegate handle if needed for unbinding, or use a flag
	bool bIsSubscribedToTagEvents;
};
