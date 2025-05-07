// Copyright Joseph Kirk 2025

#include "FlowInkGlobalEventNode.h"
#include "GameplayMessageSubsystem.h"
#include "FlowSubsystem.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogFlowInkGlobalEvent, Log, All);
#define LOCTEXT_NAMESPACE "FlowInkGlobalEventNode"

UFlowInkGlobalEventNode::UFlowInkGlobalEventNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	Category = TEXT("Ink");
	NodeStyle = EFlowNodeStyle::Event;
#endif
	FunctionCalledPin.PinName = TEXT("Called");
	ActualFunctionNamePin.PinName = TEXT("FunctionName");
	ArgumentsPin.PinName = TEXT("Arguments");
	ErrorPin.PinName = TEXT("Error");

#if WITH_EDITOR
	FunctionCalledPin.PinFriendlyName = LOCTEXT("FunctionCalledPinFriendlyName", "Called");
	ActualFunctionNamePin.PinFriendlyName = LOCTEXT("ActualFunctionNamePinFriendlyName", "Function Name");
	ArgumentsPin.PinFriendlyName = LOCTEXT("ArgumentsPinFriendlyName", "Arguments");
	ErrorPin.PinFriendlyName = LOCTEXT("ErrorPinFriendlyName", "Error");
	ErrorPin.PinToolTip = LOCTEXT("ErrorPinToolTip", "Triggered if an error occurs during activation (e.g., invalid channel, subsystem unavailable).");
#endif

	OutputPins.Add(FunctionCalledPin);
	OutputPins.Add(ActualFunctionNamePin);
	OutputPins.Add(ArgumentsPin);
	OutputPins.Add(ErrorPin);
}

void UFlowInkGlobalEventNode::OnActivate()
{
	Super::OnActivate();

	if (!EventChannelToListen.IsValid())
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): EventChannelToListen is not valid. Node will not receive messages."), *GetIdentityName(), *GetName());
		UE_LOG(LogFlowInkGlobalEvent, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): Failed to get World."), *GetIdentityName(), *GetName());
		UE_LOG(LogFlowInkGlobalEvent, Error, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	UGameplayMessageSubsystem* MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	if (!MessageSubsystem)
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): Failed to get GameplayMessageSubsystem."), *GetIdentityName(), *GetName());
		UE_LOG(LogFlowInkGlobalEvent, Error, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	MessageListenerHandle = MessageSubsystem->RegisterListener(EventChannelToListen, this, &UFlowInkGlobalEventNode::HandleInkEventMessage);

	if (MessageListenerHandle.IsValid())
	{
		UE_LOG(LogFlowInkGlobalEvent, Verbose, TEXT("%s (%s): Successfully registered listener for channel '%s'."), *GetIdentityName(), *GetName(), *EventChannelToListen.ToString());
	}
	else
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): Failed to register listener for channel '%s'."), *GetIdentityName(), *GetName(), *EventChannelToListen.ToString());
		UE_LOG(LogFlowInkGlobalEvent, Error, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
	}
}

void UFlowInkGlobalEventNode::OnDeactivate()
{
	UWorld* World = GetWorld();
	if (World && MessageListenerHandle.IsValid())
	{
		UGameplayMessageSubsystem* MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		if (MessageSubsystem)
		{
			MessageSubsystem->UnregisterListener(MessageListenerHandle);
			UE_LOG(LogFlowInkGlobalEvent, Verbose, TEXT("%s (%s): Unregistered listener for channel '%s'."), *GetIdentityName(), *GetName(), *EventChannelToListen.ToString());
		}
	}
	MessageListenerHandle.Clear();
	Super::OnDeactivate();
}

void UFlowInkGlobalEventNode::HandleInkEventMessage(FGameplayTag Channel, const FInkExternalFunctionMessage& Message)
{
	UE_LOG(LogFlowInkGlobalEvent, Verbose, TEXT("%s (%s): Received message on channel '%s'. FunctionName: '%s'. Expected: '%s'"), 
		*GetIdentityName(), *GetName(), *Channel.ToString(), *Message.FunctionName.ToString(), *ExpectedFunctionName.ToString());

	if (!ExpectedFunctionName.IsNone() && Message.FunctionName != ExpectedFunctionName)
	{
		// Not the specific function we're waiting for, if one is specified.
		return;
	}

	SetPropertyValue(ActualFunctionNamePin.PinName, Message.FunctionName);
	SetPropertyValue(ArgumentsPin.PinName, Message.Arguments);

	TriggerOutput(FunctionCalledPin.PinName, true);
}

#if WITH_EDITOR
FString UFlowInkGlobalEventNode::GetNodeDescription() const
{
	return FString::Printf(TEXT("Listens for Ink event on channel: %s\nExpected Function: %s"), 
		*EventChannelToListen.ToString(), 
		ExpectedFunctionName.IsNone() ? TEXT("Any") : *ExpectedFunctionName.ToString());
}

FText UFlowInkGlobalEventNode::GetNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Ink Global Event");
}

EFlowNodeStyle UFlowInkGlobalEventNode::GetNodeStyle() const
{
	return EFlowNodeStyle::Event;
}
#endif

#undef LOCTEXT_NAMESPACE
