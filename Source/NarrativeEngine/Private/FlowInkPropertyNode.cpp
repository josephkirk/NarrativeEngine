// Copyright Joseph Kirk 2025

#include "FlowInkPropertyNode.h"
#include "Types/FlowDataPin.h"
#include "EdGraph/EdGraphSchema_K2.h"
#include "InkNarrativeSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

// InkCPP Includes
#include "ink/runner.h"
#include "ink/globals.h"
#include "ink/story.h"
#include "ink/value.h" // Public API value

#define LOCTEXT_NAMESPACE "FlowInkPropertyNode"

UFlowInkPropertyNode::UFlowInkPropertyNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Operation(EInkPropertyOperation::Get)
	, VariableType(EFlowInkVariableType::String) // Default to String
{
	InputPins.Add(FFlowPin(TEXT("In")));
	OutPin = FFlowPin(TEXT("Out"));
	ErrorPin = FFlowPin(TEXT("Error")); // Initialize ErrorPin
	OutputPins.Add(OutPin);
	OutputPins.Add(ErrorPin); // Add ErrorPin to OutputPins

	// Initial setup of data pins based on default operation
	// SetupDataPins(); // Called by OnLoad
}

void UFlowInkPropertyNode::OnLoad_Implementation()
{
	Super::OnLoad_Implementation();
	SetupDataPins();
}

void UFlowInkPropertyNode::ExecuteInput(const FName& PinName)
{
	if (PropertyName.IsNone())
	{
		const FString ErrorMsg = TEXT("PropertyName is not set.");
		UE_LOG(LogTemp, Warning, TEXT("FlowInkPropertyNode: %s"), *ErrorMsg);
		LogError(ErrorMsg); 
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		const FString ErrorMsg = TEXT("Failed to get GameInstance.");
		UE_LOG(LogTemp, Error, TEXT("FlowInkPropertyNode: %s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	UInkNarrativeSubsystem* InkSubsystem = GameInstance->GetSubsystem<UInkNarrativeSubsystem>();
	if (!InkSubsystem || !InkSubsystem->IsStoryLoaded())
	{
		const FString ErrorMsg = FString::Printf(TEXT("InkNarrativeSubsystem not available or story not loaded for property '%s'."), *PropertyName.ToString());
		UE_LOG(LogTemp, Warning, TEXT("FlowInkPropertyNode: %s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	ink::runtime::runner* Runner = InkSubsystem->GetRunner();
	if (!Runner)
	{
		const FString ErrorMsg = FString::Printf(TEXT("Ink Runner is not valid for property '%s'."), *PropertyName.ToString());
		UE_LOG(LogTemp, Warning, TEXT("FlowInkPropertyNode: %s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	ink::runtime::globals Globals = Runner->get_globals();
	if (!Globals)
	{
		const FString ErrorMsg = FString::Printf(TEXT("Ink Globals is not valid for property '%s'."), *PropertyName.ToString());
		UE_LOG(LogTemp, Warning, TEXT("FlowInkPropertyNode: %s"), *ErrorMsg);
		LogError(ErrorMsg);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	const std::string InkPropertyNameStd = TCHAR_TO_UTF8(*PropertyName.ToString());
	bool bOperationSuccess = false;

	if (Operation == EInkPropertyOperation::Get)
	{
		ink::optional<ink::runtime::value> InkValueOpt = Globals->get_variable(InkPropertyNameStd.c_str());
		if (InkValueOpt.has_value())
		{
			ink::runtime::value InkValue = InkValueOpt.value();
			bool bTypeMatchedOrConverted = false;
			switch (VariableType)
			{
				case EFlowInkVariableType::Bool:
					if (InkValue.type == ink::runtime::value_type::boolean) {
						SetPropertyValue<bool>(TEXT("Value"), InkValue.get<ink::runtime::value_type::boolean>());
						bTypeMatchedOrConverted = true;
					}
					break;
				case EFlowInkVariableType::Int:
					if (InkValue.type == ink::runtime::value_type::int32) {
						SetPropertyValue<int32>(TEXT("Value"), InkValue.get<ink::runtime::value_type::int32>());
						bTypeMatchedOrConverted = true;
					}
					else if (InkValue.type == ink::runtime::value_type::float32) {
						SetPropertyValue<int32>(TEXT("Value"), static_cast<int32>(InkValue.get<ink::runtime::value_type::float32>()));
						bTypeMatchedOrConverted = true;
						UE_LOG(LogTemp, Log, TEXT("FlowInkPropertyNode: GET Property '%s'. Converted Float to Int."), *PropertyName.ToString());
					}
					break;
				case EFlowInkVariableType::Float:
					if (InkValue.type == ink::runtime::value_type::float32) {
						SetPropertyValue<float>(TEXT("Value"), InkValue.get<ink::runtime::value_type::float32>());
						bTypeMatchedOrConverted = true;
					}
					else if (InkValue.type == ink::runtime::value_type::int32) {
						SetPropertyValue<float>(TEXT("Value"), static_cast<float>(InkValue.get<ink::runtime::value_type::int32>()));
						bTypeMatchedOrConverted = true;
						UE_LOG(LogTemp, Log, TEXT("FlowInkPropertyNode: GET Property '%s'. Converted Int to Float."), *PropertyName.ToString());
					}
					break;
				case EFlowInkVariableType::String:
					if (InkValue.type == ink::runtime::value_type::string) {
						SetPropertyValue<FString>(TEXT("Value"), FString(UTF8_TO_TCHAR(InkValue.get<ink::runtime::value_type::string>())));
						bTypeMatchedOrConverted = true;
					}
					else 
					{
						FString ConvertedString = InkSubsystem->ConvertInkValueToString(InkValue, PropertyName);
						SetPropertyValue<FString>(TEXT("Value"), ConvertedString);
						bTypeMatchedOrConverted = true; // Conversion is considered a success for setting pin
						UE_LOG(LogTemp, Log, TEXT("FlowInkPropertyNode: GET Property '%s'. Expected String, got InkType %d. Converted to '%s'."), *PropertyName.ToString(), static_cast<int>(InkValue.type), *ConvertedString);
					}
					break;
			}
			if (bTypeMatchedOrConverted) {
				UE_LOG(LogTemp, Log, TEXT("FlowInkPropertyNode: GET Property '%s'. Value set on output pin."), *PropertyName.ToString());
				bOperationSuccess = true;
			}
			else {
				const FString ErrorMsg = FString::Printf(TEXT("GET Property '%s'. Type mismatch: Expected %s, got InkType %d. No conversion applied."), *PropertyName.ToString(), *UEnum::GetValueAsString(VariableType), static_cast<int>(InkValue.type));
				UE_LOG(LogTemp, Warning, TEXT("FlowInkPropertyNode: %s"), *ErrorMsg);
				LogError(ErrorMsg);
				// Still output default for type if direct match/conversion failed but var existed
				switch (VariableType)
				{
					case EFlowInkVariableType::Bool: SetPropertyValue<bool>(TEXT("Value"), false); break;
					case EFlowInkVariableType::Int: SetPropertyValue<int32>(TEXT("Value"), 0); break;
					case EFlowInkVariableType::Float: SetPropertyValue<float>(TEXT("Value"), 0.0f); break;
					case EFlowInkVariableType::String: SetPropertyValue<FString>(TEXT("Value"), FString()); break;
				}
				// bOperationSuccess remains false, will trigger ErrorPin
			}
		}
		else
		{
			const FString ErrorMsg = FString::Printf(TEXT("GET Property '%s'. Variable not found in Ink story. Outputting default."), *PropertyName.ToString());
			UE_LOG(LogTemp, Warning, TEXT("FlowInkPropertyNode: %s"), *ErrorMsg);
			LogError(ErrorMsg); 
			// Set default values for output pins if variable not found
			switch (VariableType)
			{
				case EFlowInkVariableType::Bool: SetPropertyValue<bool>(TEXT("Value"), false); break;
				case EFlowInkVariableType::Int: SetPropertyValue<int32>(TEXT("Value"), 0); break;
				case EFlowInkVariableType::Float: SetPropertyValue<float>(TEXT("Value"), 0.0f); break;
				case EFlowInkVariableType::String: SetPropertyValue<FString>(TEXT("Value"), FString()); break;
			}
			// bOperationSuccess remains false, will trigger ErrorPin
		}
	}
	else // Set Operation
	{
		ink::runtime::value InkValueToSet;
		bool bValuePreparedForSet = false;

		switch (VariableType)
		{
			case EFlowInkVariableType::Bool:
				InkValueToSet = ink::runtime::value(GetPropertyValue<bool>(TEXT("Value")));
				bValuePreparedForSet = true;
				break;
			case EFlowInkVariableType::Int:
				InkValueToSet = ink::runtime::value(GetPropertyValue<int32>(TEXT("Value")));
				bValuePreparedForSet = true;
				break;
			case EFlowInkVariableType::Float:
				InkValueToSet = ink::runtime::value(GetPropertyValue<float>(TEXT("Value")));
				bValuePreparedForSet = true;
				break;
			case EFlowInkVariableType::String:
				{
					FString StringVal = GetPropertyValue<FString>(TEXT("Value"));
					InkValueToSet = ink::runtime::value(TCHAR_TO_UTF8(*StringVal));
					bValuePreparedForSet = true;
				}
				break;
		}

		if (bValuePreparedForSet)
		{
			if (Globals->set_variable(InkPropertyNameStd.c_str(), InkValueToSet))
			{
				UE_LOG(LogTemp, Log, TEXT("FlowInkPropertyNode: SET Property '%s' successfully."), *PropertyName.ToString());
				bOperationSuccess = true;
			}
			else
			{
				const FString ErrorMsg = FString::Printf(TEXT("SET Property '%s' FAILED. Variable might not exist or type mismatch with input type %s."), *PropertyName.ToString(), *UEnum::GetValueAsString(VariableType));
				UE_LOG(LogTemp, Warning, TEXT("FlowInkPropertyNode: %s"), *ErrorMsg);
				LogError(ErrorMsg);
			}
		}
		else
		{
			const FString ErrorMsg = FString::Printf(TEXT("SET Property '%s'. Could not prepare value for setting due to unhandled VariableType %s."), *PropertyName.ToString(), *UEnum::GetValueAsString(VariableType));
			UE_LOG(LogTemp, Error, TEXT("FlowInkPropertyNode: %s"), *ErrorMsg);
			LogError(ErrorMsg);
		}
	}

	if (bOperationSuccess)
	{
		TriggerOutput(OutPin.PinName, true);
	}
	else
	{
		TriggerOutput(ErrorPin.PinName, true);
	}
}

#if WITH_EDITOR
FString UFlowInkPropertyNode::GetNodeDescription() const
{
	FString OpStr = (Operation == EInkPropertyOperation::Get) ? TEXT("Get") : TEXT("Set");
	return FString::Printf(TEXT("%s Ink Property: %s"), *OpStr, PropertyName.IsNone() ? TEXT("[None]") : *PropertyName.ToString());
}

FString UFlowInkPropertyNode::GetStatusString() const
{
	if (!PropertyName.IsNone())
	{
		FString OpStr = (Operation == EInkPropertyOperation::Get) ? TEXT("Get") : TEXT("Set");
		FString PinName = (Operation == EInkPropertyOperation::Get) ? TEXT("ValueOut") : TEXT("ValueIn");
		return FString::Printf(TEXT("%s: %s (%s)"), *OpStr, *PropertyName.ToString(), *PinName);
	}
	return Super::GetStatusString();
}

void UFlowInkPropertyNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyNameChanged = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyNameChanged == GET_MEMBER_NAME_CHECKED(UFlowInkPropertyNode, Operation) ||
		PropertyNameChanged == GET_MEMBER_NAME_CHECKED(UFlowInkPropertyNode, VariableType))
	{
		SetupDataPins();
		// Ensure the node is reconstructed in the editor to reflect pin changes
		if (GetGraphNode()) 
		{
			GetGraphNode()->ReconstructNode();
		}
	}
}
#endif

void UFlowInkPropertyNode::SetupDataPins()
{
	UE_LOG(LogTemp, Log, TEXT("FlowInkPropertyNode (%s): SetupDataPins called. Operation: %s"), *GetName(), Operation == EInkPropertyOperation::Get ? TEXT("Get") : TEXT("Set") );

	// Clear existing data pins to ensure a clean state
	InputDataPins.Empty(); 
	OutputDataPins.Empty();

	const FName ValuePinName = TEXT("Value");
	const FText ValuePinFriendlyName = (Operation == EInkPropertyOperation::Get) ? LOCTEXT("ValueOutPinName", "Value Out") : LOCTEXT("ValueInPinName", "Value In");
	const FString ValuePinTooltipBase = (Operation == EInkPropertyOperation::Get) ? TEXT("The value retrieved from the Ink property: ") : TEXT("The value to set for the Ink property: ");

	FEdGraphPinType PinType;
	FString TypeDesc;

	switch (VariableType)
	{
		case EFlowInkVariableType::Bool:
			PinType = FEdGraphPinType(UEdGraphSchema_K2::PC_Boolean, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
			TypeDesc = TEXT("Bool");
			break;
		case EFlowInkVariableType::Int:
			PinType = FEdGraphPinType(UEdGraphSchema_K2::PC_Int, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
			TypeDesc = TEXT("Int");
			break;
		case EFlowInkVariableType::Float:
			PinType = FEdGraphPinType(UEdGraphSchema_K2::PC_Real, FName(TEXT("float")), nullptr, EPinContainerType::None, false, FEdGraphTerminalType()); // Using PC_Real with float subcategory
			TypeDesc = TEXT("Float");
			break;
		case EFlowInkVariableType::String:
		default:
			PinType = FEdGraphPinType(UEdGraphSchema_K2::PC_String, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
			TypeDesc = TEXT("String");
			break;
	}
	
	FString FinalTooltip = ValuePinTooltipBase + PropertyName.ToString() + TEXT(" (Type: ") + TypeDesc + TEXT(")");

	if (Operation == EInkPropertyOperation::Get)
	{
		// For 'Get', we provide an OUTPUT data pin
		FFlowPin NewOutputDataPin(ValuePinName);
		NewOutputDataPin.PinToolTip = FinalTooltip;
#if WITH_EDITOR
		NewOutputDataPin.PinFriendlyName = ValuePinFriendlyName;
		NewOutputDataPin.PinType = PinType;
#endif
		OutputDataPins.Add(NewOutputDataPin);
	}
	else // Set
	{
		// For 'Set', we provide an INPUT data pin
		FFlowPin NewInputDataPin(ValuePinName);
		NewInputDataPin.PinToolTip = FinalTooltip;
#if WITH_EDITOR
		NewInputDataPin.PinFriendlyName = ValuePinFriendlyName;
		NewInputDataPin.PinType = PinType;
#endif
		InputDataPins.Add(NewInputDataPin);
	}
}

#undef LOCTEXT_NAMESPACE
