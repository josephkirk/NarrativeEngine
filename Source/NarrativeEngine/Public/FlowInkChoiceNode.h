// Copyright Joseph Kirk 2025

#pragma once

#include "FlowInkNodeBase.h"
#include "ink/types.h" // For ink::runtime::choice forward declaration access
#include "FlowInkChoiceNode.generated.h"

// Forward declaration from InkCPP
namespace ink::runtime { class choice; }

USTRUCT(BlueprintType)
struct NARRATIVEENGINE_API FInkChoiceInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ink Choice")
	FText Text;

	// The original index of this choice as provided by the Ink runtime.
	UPROPERTY()
	int32 OriginalInkChoiceIndex;

	// The FName of the dynamically generated output execution pin for this choice.
	UPROPERTY()
	FName OutputPinName;

	FInkChoiceInfo()
		: OriginalInkChoiceIndex(-1)
		, OutputPinName(NAME_None)
	{}

	FInkChoiceInfo(const FText& InText, int32 InOriginalInkChoiceIndex, const FName& InOutputPinName)
		: Text(InText)
		, OriginalInkChoiceIndex(InOriginalInkChoiceIndex)
		, OutputPinName(InOutputPinName)
	{}
};

/**
 * FlowNode to handle a choice point from an Ink story.
 * When executed, it fetches available choices from Ink and dynamically creates output execution pins for each.
 * Activating one of these choice pins tells Ink which choice was selected.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Ink Choice"))
class NARRATIVEENGINE_API UFlowInkChoiceNode : public UFlowInkNodeBase
{
	GENERATED_UCLASS_BODY()

protected:
	// Input pin to trigger fetching choices and setting up dynamic output pins.
	// (This is typically the default "In" pin defined by UFlowNode::DefaultInputPin)

	// Input pin to be triggered after the player has made a selection via UI.
	// The 'SelectedChoiceOriginalIndex' data pin should be set before triggering this.
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode", meta=(ToolTip="Trigger this pin after setting SelectedChoiceOriginalIndex to confirm the player's choice."))
	FFlowPin ConfirmSelectionPin;

	UPROPERTY(EditDefaultsOnly, Category = "FlowNode", meta=(ToolTip="Triggered after choices are fetched and pins are ready."))
	FFlowPin ChoicesAvailablePin;

	// Output pin triggered after an Ink choice has been successfully made and processed by Ink.
	// Connect this to continue the main story flow after a choice.
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode", meta=(ToolTip="Triggered after a choice is made and processed by Ink."))
	FFlowPin ChoiceMadePin;

	// Output pin triggered if an error occurs (e.g. subsystem unavailable, no choices when expected, failed to make choice)
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode", meta=(ToolTip="Triggered on error or if no choices are available."))
	FFlowPin ErrorPin;

	// Input data pin: The original Ink index of the choice selected by the player.
	// This should be set by game logic before triggering the 'ConfirmSelectionPin'.
	UPROPERTY(EditAnywhere, Category = "Ink Choice", meta = (DisplayName = "Selected Choice Index (Ink)", FlowInput))
	FFlowPropertyInt SelectedChoiceOriginalIndex;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Ink")
	TArray<FInkChoiceInfo> AvailableChoices;

public:
	/** Retrieves the list of currently available choices, populated after the 'In' pin is executed and 'ChoicesAvailable' is triggered. */
	UFUNCTION(BlueprintPure, Category = "Ink Choice")
	const TArray<FInkChoiceInfo>& GetAvailableChoices() const { return AvailableChoices; }

	virtual void ExecuteInput(const FName& PinName) override;

	//~ Begin UFlowNode Interface
#if WITH_EDITOR
	virtual FString GetNodeDescription() const override;
	virtual FString GetStatusString() const override;
	virtual void AllocateDefaultPins() override;
	// This function is called when the node is constructed in the editor or when it needs to be rebuilt.
	// We can use it to ensure our dynamic pins are correctly represented if the node is duplicated or reloaded.
	virtual void PostLoad() override;
	// Potentially needed if we want to react to pins being connected/disconnected in editor for choice pins
	// virtual void OnPinConnectionsChanged(const FName& PinName, bool bIsConnection, UEdGraphPin* Pin) override;
#endif
	//~ End UFlowNode Interface

private:
	void FetchAndSetupDynamicChoicePins();
	void ClearDynamicChoicePins();

	// Helper to generate unique pin names for choices
	static FName GetChoicePinNameByIndex(int32 ChoiceIndex);
};
