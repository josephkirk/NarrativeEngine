// Copyright Joseph Kirk 2025

#include "FlowInkObservationNode.h"
#include "InkNarrativeSubsystem.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Types/FlowDataPin.h"
#include "EdGraph/EdGraphSchema_K2.h" // For PinCategory definitions

#define LOCTEXT_NAMESPACE "FlowInkObservationNode"

UFlowInkObservationNode::UFlowInkObservationNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PollingInterval(0.5f)
	, LastKnownValue(TEXT(""))
	, RemainingPollingTime(0.0f)
{
#if WITH_EDITOR
	Category = TEXT("Ink");
	NodeStyle = EFlowNodeStyle::Latent;
	NodeToolTip = LOCTEXT("NodeTooltip", "Observes an Ink variable and triggers when its value changes.");
#endif

	InputPins.Add(FFlowPin(UFlowNode::DefaultInputPin.PinName, LOCTEXT("InPinFriendlyName", "Activate")));
	InputPins.Add(FFlowPin(TEXT("Deactivate"), LOCTEXT("DeactivatePinFriendlyName", "Deactivate")));

	VariableChangedPin.PinName = TEXT("Changed");
	VariableChangedPin.PinToolTip = LOCTEXT("VariableChangedPinTooltip", "Triggered when the observed Ink variable's value changes.").ToString();
#if WITH_EDITOR
	VariableChangedPin.PinFriendlyName = LOCTEXT("VariableChangedPinFriendlyName", "Changed");
#endif
	OutputPins.Add(VariableChangedPin);

	const FName NewValuePinName = TEXT("NewValue");
	FFlowPin NewValueDataPin(NewValuePinName);
	NewValueDataPin.PinToolTip = TEXT("The new string value of the observed variable after it has changed.");
#if WITH_EDITOR
	NewValueDataPin.PinFriendlyName = LOCTEXT("NewValueDataPinFriendlyName", "New Value");
#endif
	NewValueDataPin.PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	OutputDataPins.Add(NewValueDataPin);

	ErrorPin.PinName = TEXT("Error");
#if WITH_EDITOR
	ErrorPin.PinFriendlyName = LOCTEXT("ErrorPinFriendlyName", "Error");
	ErrorPin.PinToolTip = LOCTEXT("ErrorPinToolTip", "Triggered on error during setup or activation (e.g., empty variable name, invalid interval, subsystem unavailable).").ToString();
#endif
	OutputPins.Add(ErrorPin);
}

void UFlowInkObservationNode::ExecuteInput(const FName& PinName)
{
	if (PinName == UFlowNode::DefaultInputPin.PinName)
	{
		UE_LOG(LogFlow, Verbose, TEXT("FlowInkObservationNode (%s): 'Activate' pin executed. Starting observation for variable '%s'."), *GetName(), *InkVariableName);
		StartPolling();
	}
	else if (PinName == TEXT("Deactivate"))
	{
		UE_LOG(LogFlow, Verbose, TEXT("FlowInkObservationNode (%s): 'Deactivate' pin executed. Stopping observation."), *GetName());
		StopPolling();
		// Optionally, trigger a 'Deactivated' output if one were added
	}
}

void UFlowInkObservationNode::OnActivate()
{
    Super::OnActivate();
    // If the node is activated by graph flow (e.g. not through ExecuteInput directly if that path exists),
    // then also start polling. This ensures it starts if the graph becomes active and this node is an initial active node.
    UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Activated. Starting polling for '%s'."), *GetName(), *InkVariableName);
    StartPolling();
}

void UFlowInkObservationNode::OnDeactivate()
{
    Super::OnDeactivate();
    UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Deactivated. Stopping polling for '%s'."), *GetName(), *InkVariableName);
    StopPolling();
}

void UFlowInkObservationNode::StartPolling()
{
	if (InkVariableName.IsEmpty())
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s: InkVariableName is empty. Cannot start polling."), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	if (PollingInterval <= 0.0f)
    {
        const FString ErrorMsg = FString::Printf(TEXT("%s: PollingInterval is zero or negative (%.2f). Polling will not start."), *GetName(), PollingInterval);
        UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
        LogError(ErrorMsg);
        TriggerOutput(ErrorPin.PinName, true);
        return;
    }

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s: Failed to get GameInstance."), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	UInkNarrativeSubsystem* InkSubsystem = GameInstance->GetSubsystem<UInkNarrativeSubsystem>();
	if (!InkSubsystem || !InkSubsystem->IsStoryLoaded())
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s: InkNarrativeSubsystem not available or story not loaded."), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	// Fetch initial value
	bool bFound = false;
	LastKnownValue = InkSubsystem->GetVariableValueAsString(InkVariableName, bFound);
	if (!bFound)
	{
		UE_LOG(LogFlow, Warning, TEXT("FlowInkObservationNode (%s): Variable '%s' not found in Ink story during initial fetch. Polling will continue but might not trigger if variable never appears."), *GetName(), *InkVariableName);
		// LastKnownValue will be empty, which is fine. If it appears later, it will be a change.
	}
	else
	{
		UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Initial value for '%s' is '%s'."), *GetName(), *InkVariableName, *LastKnownValue);
	}

	// Start the timer
	UWorld* World = GetWorld();
	if (World)
	{
		if (!PollingTimerHandle.IsValid()) // Only start if not already running
        {
		    World->GetTimerManager().SetTimer(PollingTimerHandle, this, &UFlowInkObservationNode::PollInkVariable, PollingInterval, true, PollingInterval); // Initial delay same as interval
		    UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Started polling timer for '%s' every %.2f seconds."), *GetName(), *InkVariableName, PollingInterval);
        }
	}
}

void UFlowInkObservationNode::StopPolling()
{
	UWorld* World = GetWorld();
	if (World && PollingTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(PollingTimerHandle);
		UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Stopped polling timer for '%s'."), *GetName(), *InkVariableName);
	}
	PollingTimerHandle.Invalidate(); // Ensure it's marked as invalid
    RemainingPollingTime = 0.0f; // Reset remaining time when explicitly stopped
}

void UFlowInkObservationNode::PollInkVariable()
{
	if (InkVariableName.IsEmpty() || !IsActive())
	{
		StopPolling(); // Stop if variable name is cleared or node deactivated
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UInkNarrativeSubsystem* InkSubsystem = GameInstance ? GameInstance->GetSubsystem<UInkNarrativeSubsystem>() : nullptr;

	if (!InkSubsystem || !InkSubsystem->IsStoryLoaded())
	{
		UE_LOG(LogFlow, Verbose, TEXT("FlowInkObservationNode (%s): Ink system not ready during poll. Skipping."), *GetName());
		return;
	}

	bool bFound = false;
	FString CurrentValue = InkSubsystem->GetVariableValueAsString(InkVariableName, bFound);

	if (!bFound)
	{
		// Variable might have been removed or story changed. If it was previously found, this is a change.
		if (!LastKnownValue.IsEmpty() || LastKnownValue != TEXT("[NotFound]")) // Consider a specific internal string for 'not found'
        {
            UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Variable '%s' was previously '%s' but is now not found. Considering it changed."), *GetName(), *InkVariableName, *LastKnownValue);
            LastKnownValue = TEXT("[NotFound]"); // Or some other internal marker for 'not found'
            SetPropertyValue<FString>(TEXT("NewValue"), LastKnownValue); 
		    TriggerOutput(VariableChangedPin.PinName, true);
        }
		return;
	}

	if (CurrentValue != LastKnownValue)
	{
		UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Variable '%s' changed from '%s' to '%s'. Triggering output."), *GetName(), *InkVariableName, *LastKnownValue, *CurrentValue);
		LastKnownValue = CurrentValue;
		SetPropertyValue<FString>(TEXT("NewValue"), LastKnownValue); 
		TriggerOutput(VariableChangedPin.PinName, true);
	}
}

void UFlowInkObservationNode::Cleanup()
{
	Super::Cleanup();
	StopPolling();
}

void UFlowInkObservationNode::OnSave_Implementation()
{
    Super::OnSave_Implementation();
	UWorld* World = GetWorld();
	if (World && PollingTimerHandle.IsValid())
	{
		RemainingPollingTime = World->GetTimerManager().GetTimerRemaining(PollingTimerHandle);
	}
    else
    {
        RemainingPollingTime = 0.0f;
    }
}

void UFlowInkObservationNode::OnLoad_Implementation()
{
    Super::OnLoad_Implementation();
	if (RemainingPollingTime > 0.0f && !InkVariableName.IsEmpty())
	{
		UWorld* World = GetWorld();
		if (World)
		{
            // Fetch initial value again on load before restarting timer, as LastKnownValue is what we saved.
            // This ensures that if the variable changed WHILE the game was saved/closed, we catch it immediately on load.
            UGameInstance* GameInstance = World->GetGameInstance();
            UInkNarrativeSubsystem* InkSubsystem = GameInstance ? GameInstance->GetSubsystem<UInkNarrativeSubsystem>() : nullptr;
            if(InkSubsystem && InkSubsystem->IsStoryLoaded())
            {
                bool bFoundCurrent = false;
                FString CurrentValueOnLoad = InkSubsystem->GetVariableValueAsString(InkVariableName, bFoundCurrent);
                if (bFoundCurrent && CurrentValueOnLoad != LastKnownValue)
                {
                    UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Variable '%s' changed during save/load. From '%s' to '%s'. Triggering."), *GetName(), *InkVariableName, *LastKnownValue, *CurrentValueOnLoad);
                    LastKnownValue = CurrentValueOnLoad;
                    SetPropertyValue<FString>(TEXT("NewValue"), LastKnownValue);
                    // Defer triggering output to avoid issues during load sequence? Or trigger immediately?
                    // For now, let's trigger. If it causes issues, can defer to next tick.
		            TriggerOutput(VariableChangedPin.PinName, true);
                }
                else if (!bFoundCurrent && (LastKnownValue.IsEmpty() || LastKnownValue != TEXT("[NotFound]")))
                {
                     UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Variable '%s' was '%s' but not found on load. Considering changed."), *GetName(), *InkVariableName, *LastKnownValue);
                    LastKnownValue = TEXT("[NotFound]"); 
                    SetPropertyValue<FString>(TEXT("NewValue"), LastKnownValue);
                    TriggerOutput(VariableChangedPin.PinName, true);
                }
            }

			World->GetTimerManager().SetTimer(PollingTimerHandle, this, &UFlowInkObservationNode::PollInkVariable, PollingInterval, true, RemainingPollingTime);
            UE_LOG(LogFlow, Log, TEXT("FlowInkObservationNode (%s): Restored polling timer for '%s', remaining: %.2f sec, interval: %.2f sec"), *GetName(), *InkVariableName, RemainingPollingTime, PollingInterval);
		}
	}
    RemainingPollingTime = 0.0f; // Clear after attempting to restore
}

#if WITH_EDITOR
void UFlowInkObservationNode::AllocateDefaultPins()
{
    Super::AllocateDefaultPins();
    if (!ContainsOutputPin(VariableChangedPin.PinName)) OutputPins.Add(VariableChangedPin);
    // Data pins are managed by OutputDataPins array in constructor
}

FString UFlowInkObservationNode::GetNodeDescription() const
{
	if (InkVariableName.IsEmpty())
	{
		return LOCTEXT("DescObserveNoVar", "Observes an Ink variable. VARIABLE NOT SET.").ToString();
	}
	return FText::Format(LOCTEXT("DescObserveVarFmt", "Observes Ink variable: '{0}' every {1}s"), FText::FromString(InkVariableName), FText::AsNumber(PollingInterval)).ToString();
}

FString UFlowInkObservationNode::GetStatusString() const
{
    if (InkVariableName.IsEmpty())
    {
        return LOCTEXT("StatusNoVariableSet", "No variable set").ToString();
    }
	if (PollingTimerHandle.IsValid())
	{
		return FText::Format(LOCTEXT("StatusPollingFmt", "Polling '{0}'. Last: '{1}'"), FText::FromString(InkVariableName), FText::FromString(LastKnownValue)).ToString();
	}
	return FText::Format(LOCTEXT("StatusIdleFmt", "Idle. Observes: '{0}'"), FText::FromString(InkVariableName)).ToString();
}
#endif

#undef LOCTEXT_NAMESPACE
