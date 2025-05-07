// Copyright Joseph Kirk 2025

#include "FlowInkSetVariableNode.h"
#include "InkNarrativeSubsystem.h"
#include "FlowSubsystem.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogFlowInkSetVar, Log, All);

UFlowInkSetVariableNode::UFlowInkSetVariableNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	Category = TEXT("Ink");
#endif
	InputPins.Add(FFlowPin(TEXT("In")));
	OutputPins.Add(FFlowPin(TEXT("Out")));
	OutputPins.Add(bSuccessPin.PinName = TEXT("Success")); // Also registers the property pin

	ErrorPin.PinName = TEXT("Error");
#if WITH_EDITOR
	ErrorPin.SetNodeParamName(TEXT("Error")); // Convention for editor display
	ErrorPin.PinFriendlyName = LOCTEXT("ErrorPinFriendlyName", "Error");
	ErrorPin.PinToolTip = LOCTEXT("ErrorPinToolTip", "Triggered if a critical error occurs (e.g., empty variable name, subsystem unavailable).").ToString();
#endif
	OutputPins.Add(ErrorPin);
}

void UFlowInkSetVariableNode::ExecuteInput(const FName& PinName)
{
	bool bResultSet = false;
	if (PinName == TEXT("In"))
	{
		if (InkVariableName.IsEmpty())
		{
			UE_LOG(LogFlowInkSetVar, Error, TEXT("FlowInkSetVariableNode '%s': InkVariableName is empty."), *GetIdentityName());
			SetPropertyValue(bSuccessPin.PinName, false);
			TriggerOutput(ErrorPin.PinName, true);
			return;
		}

		UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		if (!GameInstance)
		{
			UE_LOG(LogFlowInkSetVar, Error, TEXT("FlowInkSetVariableNode '%s': GameInstance not found."), *GetIdentityName());
			SetPropertyValue(bSuccessPin.PinName, false);
			TriggerOutput(ErrorPin.PinName, true);
			return;
		}

		UInkNarrativeSubsystem* InkSubsystem = GameInstance->GetSubsystem<UInkNarrativeSubsystem>();
		if (!InkSubsystem)
		{
			UE_LOG(LogFlowInkSetVar, Error, TEXT("FlowInkSetVariableNode '%s': InkNarrativeSubsystem not found."), *GetIdentityName());
			SetPropertyValue(bSuccessPin.PinName, false);
			TriggerOutput(ErrorPin.PinName, true);
			return;
		}

		// Convert FFlowPropertyVariant to FString. 
		// This relies on FFlowPropertyVariant having a reliable ToString() or GetValue<FString>() method.
		// If FFlowPropertyVariant directly holds basic types, we might be able to be more specific.
		FString ValueAsString = TEXT("");
		const FFlowProperty* ResolvedProperty = ValueToSet.GetPropertyData(this);
		if(ResolvedProperty)
		{
		    // Attempt to get common types. This part is speculative based on FFlowPropertyVariant's potential implementation.
                        // A more direct ValueToSet.ToString(this) or similar would be better if available.
		    if (const FFlowPropertyBool* BoolProp = Cast<FFlowPropertyBool>(ResolvedProperty))
                        {
                            ValueAsString = BoolProp->GetValue() ? TEXT("true") : TEXT("false");
                        }
                        else if (const FFlowPropertyInt* IntProp = Cast<FFlowPropertyInt>(ResolvedProperty))
                        {
                            ValueAsString = FString::FromInt(IntProp->GetValue());
                        }
                        else if (const FFlowPropertyFloat* FloatProp = Cast<FFlowPropertyFloat>(ResolvedProperty))
                        {
                            ValueAsString = FString::SanitizeFloat(FloatProp->GetValue());
                        }
                        else if (const FFlowPropertyString* StringProp = Cast<FFlowPropertyString>(ResolvedProperty))
                        {
                            ValueAsString = StringProp->GetValue();
                        }
                        else if (const FFlowPropertyName* NameProp = Cast<FFlowPropertyName>(ResolvedProperty))
                        {
                             ValueAsString = NameProp->GetValue().ToString();
                        }
                        else
                        {
                            // Fallback or if it's a more complex variant type that has a generic string conversion
				UE_LOG(LogFlowInkSetVar, Warning, TEXT("FlowInkSetVariableNode '%s': ValueToSet has an unsupported FFlowPropertyVariant type for direct conversion. Attempting generic string. This might lead to unexpected behavior or an empty value being set."), *GetIdentityName());
				// If FFlowPropertyVariant had a generic ToString(), it would be used here. For now, it might be empty.
				// Consider if an unsupported type here should trigger the ErrorPin and return.
				// For now, we'll let it proceed and rely on Ink's handling of the potentially empty/incorrect string.
		    }
		}

		bResultSet = InkSubsystem->SetVariableValueFromString(InkVariableName, ValueAsString);
		UE_LOG(LogFlowInkSetVar, Verbose, TEXT("FlowInkSetVariableNode '%s': Set variable '%s' to '%s'. Success: %d"), *GetIdentityName(), *InkVariableName, *ValueAsString, bResultSet);
	}

	SetPropertyValue(bSuccessPin.PinName, bResultSet);
	TriggerOutput(TEXT("Out"), true);
}

#if WITH_EDITOR
FString UFlowInkSetVariableNode::GetNodeDescription() const
{
	return FString::Printf(TEXT("Sets Ink Variable: %s\nTo Value (as string): %s"), 
		*InkVariableName, 
		*ValueToSet.GetExportText(this) // Using GetExportText for a preview; actual conversion happens at runtime.
		);
}

FText UFlowInkSetVariableNode::GetNodeTitle() const
{
	return FText::FromString(TEXT("Set Ink Variable"));
}
#endif
