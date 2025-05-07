// Copyright Joseph Kirk 2025

#include "FlowInkTagEventNode.h"
#include "InkNarrativeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Types/FlowDataPin.h"
#include "EdGraph/EdGraphSchema_K2.h" // For PinCategory definitions

// InkCPP Includes
#include "ink/runner.h"

#define LOCTEXT_NAMESPACE "FlowInkTagEventNode"

UFlowInkTagEventNode::UFlowInkTagEventNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsSubscribedToTagEvents(false)
{
#if WITH_EDITOR
	Category = TEXT("Ink");
	NodeStyle = EFlowNodeStyle::Event;
	NodeToolTip = LOCTEXT("NodeTooltip", "Listens for specific tags from the Ink story and triggers an output when a match occurs.");
#endif

	// Input pin to activate the listener (though OnActivate is the primary mechanism)
	InputPins.Add(FFlowPin(UFlowNode::DefaultInputPin.PinName, LOCTEXT("InPinFriendlyName", "Activate"))); 

	TagMatchedPin.PinName = TEXT("TagMatched");
	TagMatchedPin.PinToolTip = LOCTEXT("TagMatchedPinTooltip", "Triggered when a matching Ink tag is encountered.").ToString();
#if WITH_EDITOR
	TagMatchedPin.PinFriendlyName = LOCTEXT("TagMatchedPinFriendlyName", "Tag Matched");
#endif
	OutputPins.Add(TagMatchedPin);

	// Setup for the output data pin "MatchedTagString"
	const FName MatchedTagPinName = TEXT("MatchedTag");
	FFlowPin MatchedTagDataPin(MatchedTagPinName);
	MatchedTagDataPin.PinToolTip = TEXT("The string value of the tag that was matched.");
#if WITH_EDITOR
	MatchedTagDataPin.PinFriendlyName = LOCTEXT("MatchedTagDataPinFriendlyName", "Tag Value");
#endif
	MatchedTagDataPin.PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	OutputDataPins.Add(MatchedTagDataPin);

	ErrorPin.PinName = TEXT("Error");
#if WITH_EDITOR
	ErrorPin.PinFriendlyName = LOCTEXT("ErrorPinFriendlyName", "Error");
	ErrorPin.PinToolTip = LOCTEXT("ErrorPinToolTip", "Triggered on error during setup or activation (e.g., subsystem unavailable, unable to subscribe to Ink events).").ToString();
#endif
	OutputPins.Add(ErrorPin);
}

void UFlowInkTagEventNode::ExecuteInput(const FName& PinName)
{
	// The primary activation is via OnActivate. ExecuteInput can serve as a re-arm or explicit check if needed.
	// For now, it just ensures the node attempts to subscribe if it hasn't already.
	if (PinName == UFlowNode::DefaultInputPin.PinName)
	{
		UE_LOG(LogFlow, Verbose, TEXT("FlowInkTagEventNode (%s): 'In' pin executed. Ensuring subscription."), *GetName());
		TrySubscribeToTagEvents();
		// Note: This node doesn't 'finish' in the typical sense after ExecuteInput unless the tag is immediately found.
		// It remains active, listening for tags.
	}
}

void UFlowInkTagEventNode::OnActivate()
{
	Super::OnActivate();
	UE_LOG(LogFlow, Log, TEXT("FlowInkTagEventNode (%s): Activated. Attempting to subscribe to Ink tag events."), *GetName());
	TrySubscribeToTagEvents();
}

void UFlowInkTagEventNode::OnDeactivate()
{
	Super::OnDeactivate();
	UE_LOG(LogFlow, Log, TEXT("FlowInkTagEventNode (%s): Deactivated. Attempting to unsubscribe from Ink tag events."), *GetName());
	TryUnsubscribeFromTagEvents();
	MatchedTagValue = TEXT(""); // Clear last matched tag
}

void UFlowInkTagEventNode::TrySubscribeToTagEvents()
{
	if (bIsSubscribedToTagEvents) return;

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s: Failed to get GameInstance for tag subscription."), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	UInkNarrativeSubsystem* InkSubsystem = GameInstance->GetSubsystem<UInkNarrativeSubsystem>();
	if (!InkSubsystem || !InkSubsystem->IsStoryLoaded())
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s: InkNarrativeSubsystem not available or story not loaded for tag subscription."), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	ink::runtime::runner* Runner = InkSubsystem->GetRunner();
	if (Runner)
	{
		Runner->tag_event.bind(this, &UFlowInkTagEventNode::HandleInkTagEvent);
		bIsSubscribedToTagEvents = true;
		UE_LOG(LogFlow, Log, TEXT("FlowInkTagEventNode (%s): Successfully subscribed to Ink tag events."), *GetName());
	}
	else
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s: Ink Runner not valid for tag subscription."), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
	}
}

void UFlowInkTagEventNode::TryUnsubscribeFromTagEvents()
{
	if (!bIsSubscribedToTagEvents) return;

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	// Check game instance and subsystem primarily to avoid issues during shutdown or if runner became invalid
	if (GameInstance)
	{
		UInkNarrativeSubsystem* InkSubsystem = GameInstance->GetSubsystem<UInkNarrativeSubsystem>();
		if (InkSubsystem && InkSubsystem->GetRunner()) // Check if runner is still valid
		{
			ink::runtime::runner* Runner = InkSubsystem->GetRunner();
			if(Runner) Runner->tag_event.unbind(); // Unbind if runner still exists
		}
	}
	bIsSubscribedToTagEvents = false;
	UE_LOG(LogFlow, Log, TEXT("FlowInkTagEventNode (%s): Unsubscribed from Ink tag events."), *GetName());
}

void UFlowInkTagEventNode::HandleInkTagEvent(const char* Tag)
{
	if (!IsActive() || !Tag)
	{
		return; // Node not active or tag is null
	}

	FString IncomingTag = ANSI_TO_TCHAR(Tag);
	UE_LOG(LogFlow, Verbose, TEXT("FlowInkTagEventNode (%s): Received Ink tag: '%s'. Listening for: '%s'"), *GetName(), *IncomingTag, *TagToListenFor);

	if (TagToListenFor.IsEmpty() || TagToListenFor.Equals(IncomingTag, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogFlow, Log, TEXT("FlowInkTagEventNode (%s): Matched tag: '%s'. Triggering output."), *GetName(), *IncomingTag);
		MatchedTagValue = IncomingTag;
		SetPropertyValue<FString>(TEXT("MatchedTag"), MatchedTagValue); 
		TriggerOutput(TagMatchedPin.PinName, true);
		// This node might remain active to catch subsequent tags unless designed otherwise.
	}
}

#if WITH_EDITOR
void UFlowInkTagEventNode::AllocateDefaultPins()
{
    Super::AllocateDefaultPins();
    // Ensure static pins are present (handled by constructor adding them to respective arrays)
    // But we need to make sure they are in the OutputPins for UFlowNode logic if they were only in OutputDataPins etc.
    if (!ContainsOutputPin(TagMatchedPin.PinName)) OutputPins.Add(TagMatchedPin);

    // For data pins, they are usually managed by their specific arrays (InputDataPins, OutputDataPins)
    // If UFlowNode base class or editor utilities expect them in a combined list for some operations,
    // ensure they are correctly registered. Here, it's correctly in OutputDataPins.
}

FString UFlowInkTagEventNode::GetNodeDescription() const
{
	if (TagToListenFor.IsEmpty())
	{
		return LOCTEXT("DescListenAnyTag", "Triggers when ANY Ink tag is encountered.").ToString();
	}
	return FText::Format(LOCTEXT("DescListenSpecificTagFmt", "Triggers when the Ink tag '{0}' is encountered."), FText::FromString(TagToListenFor)).ToString();
}

FString UFlowInkTagEventNode::GetStatusString() const
{
	if (!MatchedTagValue.IsEmpty())
    {
        return FText::Format(LOCTEXT("StatusLastMatchedFmt", "Last matched: {0}"), FText::FromString(MatchedTagValue)).ToString();
    }
	if (TagToListenFor.IsEmpty())
	{
		return LOCTEXT("StatusListeningAny", "Listening for any tag").ToString();
	}
	return FText::Format(LOCTEXT("StatusListeningSpecificFmt", "Listening for: {0}"), FText::FromString(TagToListenFor)).ToString();
}
#endif

#undef LOCTEXT_NAMESPACE
