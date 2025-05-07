// Copyright Joseph Kirk 2025

#include "FlowInkConditionNode.h"
#include "InkNarrativeSubsystem.h"
#include "Kismet/GameplayStatics.h" // For GetGameInstance

#define LOCTEXT_NAMESPACE "FlowInkConditionNode"

UFlowInkConditionNode::UFlowInkConditionNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	Category = TEXT("Ink");
	NodeStyle = EFlowNodeStyle::Default;
#endif
	InputPins.Add(FFlowPin(TEXT("In")));
	TruePin = FFlowPin(TEXT("True"));
	FalsePin = FFlowPin(TEXT("False"));
	ErrorPin = FFlowPin(TEXT("Error"));

#if WITH_EDITOR
	TruePin.PinFriendlyName = LOCTEXT("TruePinFriendlyName", "True");
	FalsePin.PinFriendlyName = LOCTEXT("FalsePinFriendlyName", "False");
	ErrorPin.PinFriendlyName = LOCTEXT("ErrorPinFriendlyName", "Error");
#endif

	OutputPins.Add(TruePin);
	OutputPins.Add(FalsePin);
	OutputPins.Add(ErrorPin);
}

void UFlowInkConditionNode::ExecuteInput(const FName& PinName)
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): Failed to get GameInstance."), *GetClass()->GetName(), *GetName());
		UE_LOG(LogFlow, Error, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	UInkNarrativeSubsystem* InkSubsystem = GameInstance->GetSubsystem<UInkNarrativeSubsystem>();
	if (!InkSubsystem)
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): InkNarrativeSubsystem not available."), *GetClass()->GetName(), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	if (!InkSubsystem->IsStoryLoaded() || !InkSubsystem->GetRunner())
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): Ink story not loaded or runner not valid."), *GetClass()->GetName(), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	if (ConditionName.IsNone())
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): ConditionName is not set."), *GetClass()->GetName(), *GetName());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	bool bValueFound = false;
	bool bConditionResult = false;
	bool bSuccessfullyInterpreted = InkSubsystem->GetVariableValueAsBool(ConditionName.ToString(), bValueFound, bConditionResult);

	if (!bValueFound)
	{
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): Ink variable '%s' for condition not found."), *GetClass()->GetName(), *GetName(), *ConditionName.ToString());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	if (!bSuccessfullyInterpreted)
	{
		// Variable was found, but its type was not bool, int, or a convertible string.
		const FString ErrorMsg = FString::Printf(TEXT("%s (%s): Ink variable '%s' found, but could not be interpreted as a boolean for condition."), *GetClass()->GetName(), *GetName(), *ConditionName.ToString());
		UE_LOG(LogFlow, Warning, TEXT("%s"), *ErrorMsg); // Subsystem already logs a warning with type info
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	// Successfully found and interpreted the variable as a boolean
	UE_LOG(LogFlow, Log, TEXT("%s (%s): Condition Ink variable '%s' evaluated to %s"), *GetClass()->GetName(), *GetName(), *ConditionName.ToString(), bConditionResult ? TEXT("True") : TEXT("False"));

	if (bConditionResult)
	{
		TriggerOutput(TruePin.PinName, true);
	}
	else
	{
		TriggerOutput(FalsePin.PinName, true);
	}
}

#if WITH_EDITOR
FString UFlowInkConditionNode::GetNodeDescription() const
{
	return FString::Printf(TEXT("Evaluates Ink Condition (Var): %s"), ConditionName.IsNone() ? TEXT("[None]") : *ConditionName.ToString());
}

FString UFlowInkConditionNode::GetStatusString() const
{
	if (!ConditionName.IsNone())
	{
		return ConditionName.ToString();
	}
	return Super::GetStatusString();
}
#endif

#undef LOCTEXT_NAMESPACE
