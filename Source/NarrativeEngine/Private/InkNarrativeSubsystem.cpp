// Copyright Joseph Kirk 2025

#include "InkNarrativeSubsystem.h"
#include "ink/story.h"
#include "ink/runner.h"
#include "InkRuntime.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/Archive.h"
#include "ink/types.h" // For ink::value_type
#include "GameplayMessageSubsystem.h" // Added for UGameplayMessageSubsystem
#include "Kismet/GameplayStatics.h"   // Added for GetGameInstance

DEFINE_LOG_CATEGORY_STATIC(LogInkNarrative, Log, All);

void UInkNarrativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("InkNarrativeSubsystem Initialized"));
}

void UInkNarrativeSubsystem::Deinitialize()
{
	UnloadStory();
	UE_LOG(LogTemp, Log, TEXT("InkNarrativeSubsystem Deinitialized"));
	Super::Deinitialize();
}

void UInkNarrativeSubsystem::UnloadStory()
{
	if (MainRunner)
	{
		delete MainRunner;
		MainRunner = nullptr;
	}
	if (CurrentStory)
	{
		delete CurrentStory;
		CurrentStory = nullptr;
	}
	UE_LOG(LogTemp, Log, TEXT("InkNarrativeSubsystem: Story unloaded."));
}

bool UInkNarrativeSubsystem::LoadStory(const FString& StoryAssetPath)
{
	UnloadStory(); // Ensure any previous story is cleaned up

	UE_LOG(LogTemp, Log, TEXT("InkNarrativeSubsystem: Attempting to load story from asset path: %s"), *StoryAssetPath);

	// InkCPP typically uses FInkRuntime to load story assets.
	// The FString path should be a valid Unreal asset path (e.g., /Game/MyInkStory.MyInkStory)
	UInkStoryAsset* StoryAsset = LoadObject<UInkStoryAsset>(nullptr, *StoryAssetPath);

	if (!StoryAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("InkNarrativeSubsystem: Failed to load UInkStoryAsset from path: %s"), *StoryAssetPath);
		return false;
	}

	// Create story using the InkRuntime singleton provided by InkCPP
	CurrentStory = FInkRuntime::Get().CreateStory(StoryAsset);
	if (!CurrentStory)
	{
		UE_LOG(LogTemp, Error, TEXT("InkNarrativeSubsystem: Failed to create ink story from asset: %s"), *StoryAssetPath);
		return false;
	}

	MainRunner = CurrentStory->CreateRunner();
	if (!MainRunner)
	{
		UE_LOG(LogTemp, Error, TEXT("InkNarrativeSubsystem: Failed to create ink runner from story: %s"), *StoryAssetPath);
		delete CurrentStory;
		CurrentStory = nullptr;
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("InkNarrativeSubsystem: Story '%s' loaded successfully."), *StoryAssetPath);
	return true;
}

FString UInkNarrativeSubsystem::GetVariableValueAsString(const FString& VariableName, bool& bFound) const
{
	bFound = false;
	if (!IsStoryLoaded() || !MainRunner)
	{
		UE_LOG(LogInkNarrative, Warning, TEXT("UInkNarrativeSubsystem::GetVariableValueAsString: Story not loaded or runner not available for variable '%s'."), *VariableName);
		return FString();
	}

	try
	{
		// Convert FString to const char* for InkCPP API
		std::string VarNameAnsi = TCHAR_TO_UTF8(*VariableName);
		ink::optional<ink::value_type> optionalValue = MainRunner->get_variable(VarNameAnsi.c_str());

		if (optionalValue.has_value())
		{
			bFound = true;
			const ink::value_type& val = optionalValue.get();

			switch (val.type)
			{
				case ink::value_type::type::Bool:
					return val.v_bool ? TEXT("true") : TEXT("false");
				case ink::value_type::type::Int32:
					return FString::FromInt(val.v_int32);
				case ink::value_type::type::Uint32:
					return FString::Printf(TEXT("%u"), val.v_uint32);
				case ink::value_type::type::Float:
					return FString::SanitizeFloat(val.v_float);
				case ink::value_type::type::String:
					// Ensure the string is null-terminated before converting, though v_string should be.
					return FString(UTF8_TO_TCHAR(val.v_string));
				case ink::value_type::type::DivertTarget:
					// For divert targets (paths), you might want to return the path as a string.
					// Assuming path_string is available or similar, or convert path to string.
					// This part might need adjustment based on how ink::path is structured and how you want to represent it.
					// For now, let's just indicate it's a divert target.
					UE_LOG(LogInkNarrative, Verbose, TEXT("Variable '%s' is a DivertTarget. Returning path string if available or placeholder."), *VariableName);
					// Example: return FString(UTF8_TO_TCHAR(val.v_path.ToString().c_str())); // If path has a ToString()
					return TEXT("[DivertTarget]"); // Placeholder
				case ink::value_type::type::VariablePointer:
					UE_LOG(LogInkNarrative, Verbose, TEXT("Variable '%s' is a VariablePointer. Value is its name: %s"), *VariableName, UTF8_TO_TCHAR(val.v_variable_pointer.name));
					return FString::Printf(TEXT("[VariablePointer: %s]"), UTF8_TO_TCHAR(val.v_variable_pointer.name));
				// case ink::value_type::type::List: // List types would require more complex string conversion
				default:
					UE_LOG(LogInkNarrative, Warning, TEXT("UInkNarrativeSubsystem::GetVariableValueAsString: Unhandled Ink variable type (%d) for '%s'."), static_cast<int>(val.type),*VariableName);
					return TEXT("[UnhandledType]");
			}
		}
		else
		{
			UE_LOG(LogInkNarrative, Verbose, TEXT("UInkNarrativeSubsystem::GetVariableValueAsString: Variable '%s' not found in Ink story."), *VariableName);
			return FString(); // Or perhaps a specific string indicating not found, e.g., "[NotFound]"
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogInkNarrative, Error, TEXT("UInkNarrativeSubsystem::GetVariableValueAsString: Exception while getting variable '%s': %s"), *VariableName, ANSI_TO_TCHAR(e.what()));
		return FString();
	}
	catch (...)
	{
		UE_LOG(LogInkNarrative, Error, TEXT("UInkNarrativeSubsystem::GetVariableValueAsString: Unknown exception while getting variable '%s'."), *VariableName);
		return FString();
	}
}

bool UInkNarrativeSubsystem::GetVariableValueAsBool(const FString& VariableName, bool& bFound, bool& OutValue) const
{
	bFound = false;
	OutValue = false;

	if (!MainRunner)
	{
		UE_LOG(LogInkNarrative, Warning, TEXT("GetVariableValueAsBool: MainRunner is not initialized."));
		return false;
	}

	try
	{
		ink::optional<ink::runtime::value> InkValueOpt = MainRunner->get_variable(TCHAR_TO_UTF8(*VariableName));

		if (!InkValueOpt.has_value())
		{
			UE_LOG(LogInkNarrative, Log, TEXT("GetVariableValueAsBool: Variable '%s' not found in Ink story."), *VariableName);
			// bFound remains false
			return false; 
		}

		bFound = true;
		const ink::runtime::value& InkValue = InkValueOpt.value();

		switch (InkValue.type)
		{
			case ink::runtime::value::Type::Bool:
				OutValue = InkValue.get<ink::runtime::value::Type::Bool>();
				return true;
			case ink::runtime::value::Type::Int:
				OutValue = (InkValue.get<ink::runtime::value::Type::Int>() != 0);
				return true;
			case ink::runtime::value::Type::String:
				{
					FString StringVal = UTF8_TO_TCHAR(InkValue.get<ink::runtime::value::Type::String>());
					if (StringVal.Equals(TEXT("true"), ESearchCase::IgnoreCase) || StringVal.Equals(TEXT("1"), ESearchCase::IgnoreCase))
					{
						OutValue = true;
						return true;
					}
					if (StringVal.Equals(TEXT("false"), ESearchCase::IgnoreCase) || StringVal.Equals(TEXT("0"), ESearchCase::IgnoreCase))
					{
						OutValue = false;
						return true;
					}
					UE_LOG(LogInkNarrative, Warning, TEXT("GetVariableValueAsBool: Variable '%s' is a string ('%s') that could not be interpreted as boolean."), *VariableName, *StringVal);
					return false; // Could not convert string to bool
				}
			default:
				UE_LOG(LogInkNarrative, Warning, TEXT("GetVariableValueAsBool: Variable '%s' is of an unsupported type for boolean conversion. Type: %d"), *VariableName, static_cast<int>(InkValue.type));
				return false; // Unsupported type for bool conversion
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogInkNarrative, Error, TEXT("GetVariableValueAsBool: Exception while accessing variable '%s': %s"), *VariableName, UTF8_TO_TCHAR(e.what()));
		// bFound might be true if exception happened after finding, but value is indeterminate
		// To be safe, let's ensure bFound reflects the initial check's outcome if InkValueOpt was valid, or reset it.
		// However, if we are here, InkValueOpt was likely problematic or the get<> threw.
		// For simplicity, if an exception occurs, we treat it as 'not successfully retrieved as bool'.
		bFound = false; // Or keep as is if we want to differentiate 'found but bad type' from 'exception'
		OutValue = false;
		return false;
	}
	catch (...)
	{
		UE_LOG(LogInkNarrative, Error, TEXT("GetVariableValueAsBool: Unknown exception while accessing variable '%s'."), *VariableName);
		bFound = false;
		OutValue = false;
		return false;
	}
}

void UInkNarrativeSubsystem::BindExternalFunctionWithMessage(const FString& FunctionName, FGameplayTag MessageChannelTag)
{
	if (!IsStoryLoaded() || !CurrentStory)
	{
		UE_LOG(LogInkNarrative, Warning, TEXT("UInkNarrativeSubsystem::BindExternalFunctionWithMessage: Story not loaded. Cannot bind function '%s'."), *FunctionName);
		return;
	}

	std::string FuncNameAnsi = TCHAR_TO_UTF8(*FunctionName);

	CurrentStory->bind_external_function(FuncNameAnsi.c_str(), 
		[this, FunctionNameCapture = FunctionName, ChannelTagCapture = MessageChannelTag](ink::runtime::runner_async&, size_t NumArgs, const ink::optional<ink::value_type>* Args) {
		
			UGameInstance* GameInstance = GetGameInstance();
			if (!GameInstance)
			{
				UE_LOG(LogInkNarrative, Error, TEXT("BindExternalFunctionWithMessage: Failed to get GameInstance for function '%s'."), *FunctionNameCapture);
				return;
			}

			UGameplayMessageSubsystem* MessageSubsystem = GameInstance->GetSubsystem<UGameplayMessageSubsystem>();
			if (!MessageSubsystem)
			{
				UE_LOG(LogInkNarrative, Error, TEXT("BindExternalFunctionWithMessage: Failed to get UGameplayMessageSubsystem for function '%s'."), *FunctionNameCapture);
				return;
			}

			FInkExternalFunctionMessage Message;
			Message.FunctionName = FName(*FunctionNameCapture);

			for (size_t i = 0; i < NumArgs; ++i)
			{
				if (Args[i].has_value())
				{
					const ink::value_type& val = Args[i].get();
					FString ArgString;
					switch (val.type)
					{
						case ink::value_type::type::Bool:
							ArgString = val.v_bool ? TEXT("true") : TEXT("false");
							break;
						case ink::value_type::type::Int32:
							ArgString = FString::FromInt(val.v_int32);
							break;
						case ink::value_type::type::Uint32:
							ArgString = FString::Printf(TEXT("%u"), val.v_uint32);
							break;
						case ink::value_type::type::Float:
							ArgString = FString::SanitizeFloat(val.v_float);
							break;
						case ink::value_type::type::String:
							ArgString = FString(UTF8_TO_TCHAR(val.v_string));
							break;
						case ink::value_type::type::DivertTarget:
							// Assuming path has a string representation; InkCPP might need extension or specific handling here.
							// For now, using a placeholder format. Consider how ink::path should be stringified.
							// ArgString = FString::Printf(TEXT("[Divert:%s]"), UTF8_TO_TCHAR(val.v_path.ToString().c_str())); // Example if path has ToString()
							ArgString = TEXT("[DivertTarget]");
							break;
						case ink::value_type::type::VariablePointer:
							ArgString = FString::Printf(TEXT("[VariablePointer:%s]"), UTF8_TO_TCHAR(val.v_variable_pointer.name));
							break;
						default:
							ArgString = TEXT("[UnhandledArgumentType]");
							UE_LOG(LogInkNarrative, Warning, TEXT("BindExternalFunctionWithMessage: Unhandled argument type (%d) for function '%s', arg %d."), static_cast<int>(val.type), *FunctionNameCapture, i);
							break;
					}
					Message.Arguments.Add(ArgString);
				}
				else
				{
					Message.Arguments.Add(TEXT("[InvalidOptionalArgument]"));
					UE_LOG(LogInkNarrative, Warning, TEXT("BindExternalFunctionWithMessage: Invalid optional argument for function '%s', arg %d."), *FunctionNameCapture, i);
				}
			}

			UE_LOG(LogInkNarrative, Log, TEXT("Broadcasting Ink external function call: '%s' on channel '%s' with %d arguments."), *FunctionNameCapture, *ChannelTagCapture.ToString(), Message.Arguments.Num());
			MessageSubsystem->BroadcastMessage(ChannelTagCapture, Message);
		}
	);

	UE_LOG(LogInkNarrative, Log, TEXT("UInkNarrativeSubsystem: Bound external function '%s' to broadcast on GameplayMessage channel '%s'."), *FunctionName, *MessageChannelTag.ToString());
}

bool UInkNarrativeSubsystem::SetVariableValueFromString(const FString& VariableName, const FString& ValueAsString)
{
	if (!IsStoryLoaded() || !MainRunner)
	{
		UE_LOG(LogInkNarrative, Warning, TEXT("UInkNarrativeSubsystem::SetVariableValueFromString: Story not loaded. Cannot set variable '%s'."), *VariableName);
		return false;
	}

	try
	{
		std::string VarNameAnsi = TCHAR_TO_UTF8(*VariableName);
		ink::value InkValue;

		// Attempt to infer type from string content
		if (ValueAsString.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			InkValue = ink::value(true);
		}
		else if (ValueAsString.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			InkValue = ink::value(false);
		}
		else if (ValueAsString.IsNumeric() && !ValueAsString.Contains(TEXT(".")) && !ValueAsString.Contains(TEXT(",")))
		{
			int32 IntVal;
			if (FDefaultValueHelper::ParseInt(ValueAsString, IntVal))
			{
				InkValue = ink::value(IntVal);
			}
			else
			{
				// Fallback to string if parsing as int fails despite IsNumeric (e.g. very large number for int32)
				UE_LOG(LogInkNarrative, Warning, TEXT("SetVariableValueFromString: Failed to parse '%s' as int32 for variable '%s', setting as string."), *ValueAsString, *VariableName);
				std::string ValueAnsi = TCHAR_TO_UTF8(*ValueAsString);
				InkValue = ink::value(ValueAnsi.c_str());
			}
		}
		else if (ValueAsString.IsNumeric()) // For floats (contains . or , based on locale, IsNumeric handles this)
		{
			float FloatVal;
			// FDefaultValueHelper::ParseFloat might be too strict with FText. Using FString::SanitizeFloat and FCString::Atof
			std::string SanitizedValueAnsi = TCHAR_TO_UTF8(*ValueAsString.Replace(TEXT(","), TEXT("."))); // Normalize comma to dot for atof
			FloatVal = FCString::Atof(UTF8_TO_TCHAR(SanitizedValueAnsi.c_str()));
			// A more robust way would be to check if TTypeFromString<float>::FromString handles it well.
			InkValue = ink::value(FloatVal);
			// A check if conversion was truly successful might be needed if FCString::Atof has poor error reporting for bad strings
		}
		else
		{
			// Default to string
			std::string ValueAnsi = TCHAR_TO_UTF8(*ValueAsString);
			InkValue = ink::value(ValueAnsi.c_str());
		}

		MainRunner->assign(VarNameAnsi.c_str(), InkValue);
		UE_LOG(LogInkNarrative, Log, TEXT("UInkNarrativeSubsystem: Set variable '%s' to '%s'. (Interpreted as type: %d)"), *VariableName, *ValueAsString, static_cast<int>(InkValue.type));
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogInkNarrative, Error, TEXT("UInkNarrativeSubsystem::SetVariableValueFromString: Exception while setting variable '%s': %s"), *VariableName, ANSI_TO_TCHAR(e.what()));
		return false;
	}
	catch (...)
	{
		UE_LOG(LogInkNarrative, Error, TEXT("UInkNarrativeSubsystem::SetVariableValueFromString: Unknown exception while setting variable '%s'."), *VariableName);
		return false;
	}
}

FString UInkNarrativeSubsystem::ConvertInkValueToString(const ink::runtime::value& InkValue, const FName& PropertyNameForLogging) const
{
	switch (InkValue.type)
	{
		case ink::runtime::value_type::boolean:
			return InkValue.get<ink::runtime::value_type::boolean>() ? TEXT("true") : TEXT("false");
		case ink::runtime::value_type::int32:
			return FString::FromInt(InkValue.get<ink::runtime::value_type::int32>());
		case ink::runtime::value_type::float32:
			return FString::SanitizeFloat(InkValue.get<ink::runtime::value_type::float32>());
		case ink::runtime::value_type::string:
			return FString(UTF8_TO_TCHAR(InkValue.get<ink::runtime::value_type::string>()));
		case ink::runtime::value_type::divert_target:
			{
				// Divert targets are paths. For now, just indicate it's a divert target.
				// Potentially, convert the path to a string if a useful representation exists.
				// const char* path_str = InkValue.get<ink::runtime::value_type::divert_target>().path_string(); // Example if such a method existed
				UE_LOG(LogInkNarrative, Verbose, TEXT("ConvertInkValueToString: Property '%s' is a DivertTarget. Returning placeholder."), *PropertyNameForLogging.ToString());
				return TEXT("[DivertTarget]");
			}
		case ink::runtime::value_type::variable_pointer:
			{
				const char* var_ptr_name = InkValue.get<ink::runtime::value_type::variable_pointer>().name;
				UE_LOG(LogInkNarrative, Verbose, TEXT("ConvertInkValueToString: Property '%s' is a VariablePointer. Name: %s"), *PropertyNameForLogging.ToString(), UTF8_TO_TCHAR(var_ptr_name));
				return FString::Printf(TEXT("[VariablePointer: %s]"), UTF8_TO_TCHAR(var_ptr_name));
			}
		// TODO: Add ink::runtime::value_type::list when list support is implemented
		// case ink::runtime::value_type::list:
		// return TEXT("[InkList]"); // Placeholder for list
		default:
			UE_LOG(LogInkNarrative, Warning, TEXT("ConvertInkValueToString: Unhandled ink::runtime::value_type (%d) for property '%s'."), static_cast<int>(InkValue.type), *PropertyNameForLogging.ToString());
			return TEXT("[UnhandledRuntimeValueType]");
	}
}

void UInkNarrativeSubsystem::BindExternalFunctions(ink::runtime::story* StoryToBind)
{
{{ ... }}
