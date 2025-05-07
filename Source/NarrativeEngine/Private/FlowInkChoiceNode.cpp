// Copyright Joseph Kirk 2025

#include "FlowInkChoiceNode.h"
#include "InkNarrativeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Types/FlowDataPin.h" // Though not used for data pins here, often included with FlowNode work

// InkCPP Includes
#include "ink/runner.h"
#include "ink/story.h"
#include "ink/choice.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphNode.h" // Required for ReconstructNode
#include "FlowGraph.h" // Required for GetGraph
#endif

#define LOCTEXT_NAMESPACE "FlowInkChoiceNode"

UFlowInkChoiceNode::UFlowInkChoiceNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	Category = TEXT("Ink");
	NodeStyle = EFlowNodeStyle::Default;
#endif
	InputPins.Add(FFlowPin(UFlowNode::DefaultInputPin.PinName, LOCTEXT("InPinFriendlyName", "In")));

	ConfirmSelectionPin.PinName = TEXT("ConfirmSelection");
	ConfirmSelectionPin.PinToolTip = LOCTEXT("ConfirmSelectionPinTooltip", "Trigger this pin after setting SelectedChoiceOriginalIndex to confirm the player's choice.").ToString();
#if WITH_EDITOR
	ConfirmSelectionPin.PinFriendlyName = LOCTEXT("ConfirmSelectionPinFriendlyName", "Confirm Selection");
#endif
	InputPins.Add(ConfirmSelectionPin);

	// SelectedChoiceOriginalIndex is an FFlowProperty, its PinName for property system might be set via Setup() or implicitly.
	// For now, relying on UPROPERTY meta(FlowInput) to register it as a data input.
	// If direct access like GetPropertyValue(TEXT("SelectedChoiceOriginalIndex")) is needed, ensure PinName is discoverable.
	// We will use SelectedChoiceOriginalIndex.GetValue(this) directly.

	ChoicesAvailablePin.PinName = TEXT("ChoicesAvailable");
	ChoicesAvailablePin.PinToolTip = LOCTEXT("ChoicesAvailablePinTooltip", "Triggered after choices are fetched and dynamic pins are (re)created. Connect to UI logic to display choices.").ToString();
#if WITH_EDITOR
	ChoicesAvailablePin.PinFriendlyName = LOCTEXT("ChoicesAvailablePinFriendlyName", "Choices Available");
#endif

	ChoiceMadePin.PinName = TEXT("ChoiceMade");
	ChoiceMadePin.PinToolTip = LOCTEXT("ChoiceMadePinTooltip", "Triggered after an Ink choice has been selected and processed by Ink.").ToString();
#if WITH_EDITOR
	ChoiceMadePin.PinFriendlyName = LOCTEXT("ChoiceMadePinFriendlyName", "Choice Made");
#endif

	ErrorPin.PinName = TEXT("Error");
	ErrorPin.PinToolTip = LOCTEXT("ErrorPinTooltip", "Triggered on error, if no choices are available when expected, or if making a choice fails.").ToString();
#if WITH_EDITOR
	ErrorPin.PinFriendlyName = LOCTEXT("ErrorPinFriendlyName", "Error");
#endif

	// OutputPins are initially empty of dynamic choices; they get added by AllocateDefaultPins and FetchAndSetupDynamicChoicePins
	// Add static pins here to ensure they are always part of the collection from the start.
	OutputPins.Add(ChoicesAvailablePin);
	OutputPins.Add(ChoiceMadePin);
	OutputPins.Add(ErrorPin);
}

#if WITH_EDITOR
void UFlowInkChoiceNode::AllocateDefaultPins()
{
	Super::AllocateDefaultPins(); // Handles DefaultInputPin if base does it, or ensures it's there

	// Ensure ConfirmSelectionPin is present in InputPins
	if (!ContainsInputPin(ConfirmSelectionPin.PinName)) InputPins.Add(ConfirmSelectionPin);

	// Ensure static output pins are present if not already added by constructor/loading
	if (!ContainsOutputPin(ChoicesAvailablePin.PinName)) OutputPins.Add(ChoicesAvailablePin);
	if (!ContainsOutputPin(ChoiceMadePin.PinName)) OutputPins.Add(ChoiceMadePin);
	if (!ContainsOutputPin(ErrorPin.PinName)) OutputPins.Add(ErrorPin); // Add ErrorPin here too

	// Reconstruct dynamic pins based on AvailableChoices (e.g. after copy-paste or load)
	// This ensures the visual representation matches the state.
	TArray<FFlowPin> CurrentDynamicPins;
	for (const FInkChoiceInfo& ChoiceInfo : AvailableChoices)
	{
		FFlowPin ChoicePin(ChoiceInfo.OutputPinName);
		ChoicePin.PinToolTip = FText::Format(LOCTEXT("DynamicChoicePinTooltipFormat", "Select choice: {0}"), ChoiceInfo.Text).ToString();
		ChoicePin.PinFriendlyName = ChoiceInfo.Text;
		CurrentDynamicPins.Add(ChoicePin);
	}

	// Remove old dynamic pins that are no longer in AvailableChoices
	for (int32 i = OutputPins.Num() - 1; i >= 0; --i)
	{
		const FFlowPin& Pin = OutputPins[i];
		bool bIsStaticPin = Pin.PinName == ChoicesAvailablePin.PinName || 
						  Pin.PinName == ChoiceMadePin.PinName || 
						  Pin.PinName == ErrorPin.PinName || 
						  Pin.PinName == UFlowNode::DefaultInputPin.PinName ||
						  Pin.PinName == ConfirmSelectionPin.PinName; // ConfirmSelectionPin is an input, not output, but good to be robust if checking all pins.
		if (!bIsStaticPin && !AvailableChoices.FindByPredicate([&](const FInkChoiceInfo& Info){ return Info.OutputPinName == Pin.PinName; }))
		{
			OutputPins.RemoveAt(i);
		}
	}

	// Add new dynamic pins from AvailableChoices
	for (const FFlowPin& DynPinToAdd : CurrentDynamicPins)
	{
		if (!ContainsOutputPin(DynPinToAdd.PinName))
		{
			OutputPins.Add(DynPinToAdd);
		}
	}
}

void UFlowInkChoiceNode::PostLoad()
{
	Super::PostLoad();
	// On load, AvailableChoices will be empty (it's transient).
	// Ensure static pins are there; AllocateDefaultPins should handle this robustly.
	// Call ClearDynamicChoicePins to ensure a clean state for dynamic pins before potential AllocateDefaultPins or ReconstructNode calls.
	ClearDynamicChoicePins(false); // Don't notify graph yet, AllocateDefaultPins and ReconstructNode will do it
}

#endif

FName UFlowInkChoiceNode::GetChoicePinNameByIndex(int32 VisualIndex)
{
	return FName(*FString::Printf(TEXT("Choice_%d"), VisualIndex));
}

void UFlowInkChoiceNode::ClearDynamicChoicePins(bool bNotifyGraph)
{
	bool bPinsChanged = false;
	for (int32 i = OutputPins.Num() - 1; i >= 0; --i)
	{
		const FName PinName = OutputPins[i].PinName;
		// Check if it's NOT one of the static pins
		if (PinName != ChoicesAvailablePin.PinName && 
			PinName != ChoiceMadePin.PinName && 
			PinName != ErrorPin.PinName && 
			PinName != UFlowNode::DefaultInputPin.PinName && // DefaultInputPin is an input pin
			PinName != ConfirmSelectionPin.PinName) // ConfirmSelectionPin is an input pin
		{
			// Assuming other pins in OutputPins array are dynamic choice pins
			OutputPins.RemoveAt(i);
			bPinsChanged = true;
		}
	}
	AvailableChoices.Empty();

	if (bPinsChanged && bNotifyGraph)
	{
#if WITH_EDITOR
		if (UEdGraphNode* GraphNode = GetGraphNode())
		{
			GraphNode->ReconstructNode();
		}
		// Notify the graph owner if a graph node isn't directly available or for broader updates
		if (UFlowGraph* FG = GetGraph())
		{
			FG->NotifyGraphChanged(); 
		}
#endif
	}
}

void UFlowInkChoiceNode::FetchAndSetupDynamicChoicePins()
{
	ClearDynamicChoicePins(false); // Clear previous, don't notify graph yet as we'll do it after adding new ones

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		const FString ErrorMsg = FString::Printf(TEXT("FlowInkChoiceNode (%s): Failed to get GameInstance."), *GetName());
		UE_LOG(LogFlow, Error, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	UInkNarrativeSubsystem* InkSubsystem = GameInstance->GetSubsystem<UInkNarrativeSubsystem>();
	if (!InkSubsystem || !InkSubsystem->IsStoryLoaded())
	{
		const FString ErrorMsg = FString::Printf(TEXT("FlowInkChoiceNode (%s): InkNarrativeSubsystem not available or story not loaded."), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	ink::runtime::runner* Runner = InkSubsystem->GetRunner();
	if (!Runner)
	{
		const FString ErrorMsg = FString::Printf(TEXT("FlowInkChoiceNode (%s): Ink Runner is not valid."), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	std::vector<ink::runtime::choice*> CurrentInkChoices = Runner->get_current_choices();
	UE_LOG(LogFlow, Log, TEXT("FlowInkChoiceNode (%s): Fetched %d choices from Ink."), *GetName(), CurrentInkChoices.size());

	if (CurrentInkChoices.empty())
	{
		const FString WarnMsg = FString::Printf(TEXT("FlowInkChoiceNode (%s): No choices available from Ink at this point."), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *WarnMsg); // This might be a valid state, but for a "Choice" node, it usually implies an issue or end of choice block.
		LogError(WarnMsg); // Treat as an error for this node's purpose.
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	bool bPinsChanged = false;
	for (size_t i = 0; i < CurrentInkChoices.size(); ++i)
	{
		ink::runtime::choice* InkChoice = CurrentInkChoices[i];
		if (!InkChoice) continue;

		FText ChoiceText = FText::FromString(ANSI_TO_TCHAR(InkChoice->text()));
		FName ChoicePinName = GetChoicePinNameByIndex(static_cast<int32>(i)); // Visual index for pin naming
		
		AvailableChoices.Emplace(ChoiceText, InkChoice->index(), ChoicePinName);

		FFlowPin NewChoicePin(ChoicePinName);
#if WITH_EDITOR
		NewChoicePin.PinFriendlyName = ChoiceText;
#endif
		NewChoicePin.PinToolTip = FText::Format(LOCTEXT("DynamicChoicePinTooltipFormat", "Select choice: {0}"), ChoiceText).ToString();
		
		if (!ContainsOutputPin(ChoicePinName))
        {
		    OutputPins.Add(NewChoicePin);
            bPinsChanged = true;
        }
	}

#if WITH_EDITOR
	// Notify editor to rebuild the node's visual representation
	// Only call ReconstructNode if pins actually changed to avoid unnecessary rebuilds.
	if (bPinsChanged || OutputPins.Num() != (AvailableChoices.Num() + 3)) // +3 for ChoicesAvailable, ChoiceMade, and Error
	{
		if (UEdGraphNode* GraphNode = GetGraphNode())
		{
			GraphNode->ReconstructNode();
		}
		// Notify the graph owner if a graph node isn't directly available or for broader updates
		if (UFlowGraph* FG = GetGraph())
		{
			FG->NotifyGraphChanged(); 
		}
	}
#endif

	// If we reached here, choices were fetched and pins set up (or confirmed to be the same).
	TriggerOutput(ChoicesAvailablePin.PinName, true);
}

void UFlowInkChoiceNode::ExecuteInput(const FName& PinName)
{
	UE_LOG(LogFlow, Log, TEXT("FlowInkChoiceNode (%s) ExecuteInput: %s"), *GetName(), *PinName.ToString());

	if (PinName == UFlowNode::DefaultInputPin.PinName)
	{
		FetchAndSetupDynamicChoicePins();
		// FetchAndSetupDynamicChoicePins now handles its own error triggering if no choices are found.
		// It also calls ReconstructNode, so ChoicesAvailablePin should be triggered if successful.
		// We only trigger ChoicesAvailable if no error was already triggered by FetchAndSetup.
		// A bit tricky as TriggerOutput does not return success. Consider if Fetch should return a bool.
		// For now, assume if AvailableChoices is populated, it was successful and no error was triggered from Fetch.
		if (!AvailableChoices.IsEmpty())
		{
			TriggerOutput(ChoicesAvailablePin.PinName, true);
		}
		// If AvailableChoices is empty, FetchAndSetupDynamicChoicePins should have already triggered ErrorPin.
	}
	else if (PinName == ConfirmSelectionPin.PinName)
	{
		UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		if (!GameInstance)
		{
			const FString ErrorMsg = FString::Printf(TEXT("FlowInkChoiceNode (%s): Failed to get GameInstance for ConfirmSelection."), *GetName());
			UE_LOG(LogFlow, Error, TEXT("%s"), *ErrorMsg);
			LogError(ErrorMsg);
			TriggerOutput(ErrorPin.PinName, true);
			return;
		}

		UInkNarrativeSubsystem* InkSubsystem = GameInstance->GetSubsystem<UInkNarrativeSubsystem>();
		if (!InkSubsystem || !InkSubsystem->IsStoryLoaded() || !InkSubsystem->GetRunner())
		{
			const FString ErrorMsg = FString::Printf(TEXT("FlowInkChoiceNode (%s): InkNarrativeSubsystem not available, story not loaded, or runner invalid for ConfirmSelection."), *GetName());
			UE_LOG(LogFlow, Error, TEXT("%s"), *ErrorMsg);
			LogError(ErrorMsg);
			TriggerOutput(ErrorPin.PinName, true);
			return;
		}

		int32 ChoiceIdxToMake = -1;
		FFlowProperty* ResolvedProperty = SelectedChoiceOriginalIndex.GetPropertyData(this);
		if (const FFlowPropertyInt* IntProp = Cast<FFlowPropertyInt>(ResolvedProperty))
		{
			ChoiceIdxToMake = IntProp->GetValue();
		}
		else
		{
		    // This case should ideally not happen if the pin is correctly connected and set.
		    // GetValue() on FFlowPropertyInt itself might also be an option if GetPropertyData is not needed.
		    // ChoiceIdxToMake = SelectedChoiceOriginalIndex.GetValue(); // Assuming FFlowPropertyInt has a direct GetValue()
		    UE_LOG(LogFlow, Warning, TEXT("FlowInkChoiceNode (%s): SelectedChoiceOriginalIndex FFlowPropertyInt could not be resolved or read correctly. Using default -1."), *GetName());
		}
		
		UE_LOG(LogFlow, Log, TEXT("FlowInkChoiceNode (%s): ConfirmSelection triggered. Attempting to make choice with original Ink index: %d"), *GetName(), ChoiceIdxToMake);

		bool bFoundChoice = false;
		for (const FInkChoiceInfo& Info : AvailableChoices)
		{
			if (Info.OriginalInkChoiceIndex == ChoiceIdxToMake)
			{
				bFoundChoice = true;
				break;
			}
		}

		if (!bFoundChoice || ChoiceIdxToMake < 0)
		{
			const FString ErrorMsg = FString::Printf(TEXT("FlowInkChoiceNode (%s): Selected choice index %d is invalid or not among current available choices."), *GetName(), ChoiceIdxToMake);
			UE_LOG(LogFlow, Error, TEXT("%s"), *ErrorMsg);
			LogError(ErrorMsg);
			TriggerOutput(ErrorPin.PinName, true);
			return;
		}

		if (InkSubsystem->MakeChoice(ChoiceIdxToMake))
		{
			UE_LOG(LogFlow, Log, TEXT("FlowInkChoiceNode (%s): Successfully made choice with index %d."), *GetName(), ChoiceIdxToMake);
			TriggerOutput(ChoiceMadePin.PinName, true);
		}
		else
		{
			const FString ErrorMsg = FString::Printf(TEXT("FlowInkChoiceNode (%s): InkNarrativeSubsystem->MakeChoice(%d) failed."), *GetName(), ChoiceIdxToMake);
			UE_LOG(LogFlow, Error, TEXT("%s"), *ErrorMsg);
			LogError(ErrorMsg);
			TriggerOutput(ErrorPin.PinName, true);
		}
	}
	// Removed the old 'else' block that tried to process dynamic output pins as inputs.
	// Dynamic output pins are now purely informational for the editor.
}

#if WITH_EDITOR
FString UFlowInkChoiceNode::GetNodeDescription() const
{
	if (AvailableChoices.Num() > 0)
	{
		FString Desc = LOCTEXT("ChoicesAvailableDesc", "Presents the following Ink choices:\n").ToString();
		for (int32 i = 0; i < AvailableChoices.Num(); ++i)
		{
			Desc += FString::Printf(TEXT("- %s (%s)\n"), *AvailableChoices[i].Text.ToString(), *AvailableChoices[i].OutputPinName.ToString());
		}
		return Desc;
	}
	return LOCTEXT("NoChoicesYetDesc", "Fetches and presents Ink choices when 'In' is triggered.").ToString();
}

FString UFlowInkChoiceNode::GetStatusString() const
{
	if (AvailableChoices.Num() > 0)
	{
		return FText::Format(LOCTEXT("StatusChoicesFormat", "{0} choices available"), FText::AsNumber(AvailableChoices.Num())).ToString();
	}
	return LOCTEXT("StatusNoChoices", "(No choices fetched yet)").ToString();
}
#endif

#undef LOCTEXT_NAMESPACE
