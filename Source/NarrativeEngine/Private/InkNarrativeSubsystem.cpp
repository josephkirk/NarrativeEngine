// Copyright Joseph Kirk 2025

#include "InkNarrativeSubsystem.h"
#include "InkAsset.h"       // For UInkAsset
#include "InkChoiceInfo.h"  // For FInkChoiceInfo
#include "story.h"          // For ink::runtime::story
#include "runner.h"         // For ink::runtime::runner
#include "globals.h"        // For ink::runtime::globals
#include "choice.h"         // For ink::runtime::choice
#include "value.h"          // For ink::runtime::value

// For GameplayMessageSubsystem, if BindExternalFunctionWithMessage is to be fully implemented
#include "GameplayMessageSubsystem.h"
#include "Types/InkEventMessageTypes.h" // For FInkExternalFunctionMessage

// Define your log category. Let's use the one you defined: LogInkNarrative
DEFINE_LOG_CATEGORY_STATIC(LogInkNarrative, Log, All);


const FName UInkNarrativeSubsystem::MainStoryRunnerName = FName(TEXT("MainStory"));

void UInkNarrativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Ensure GameplayMessageSubsystem is initialized if this subsystem relies on it.
    // UGameplayMessageSubsystem is a WorldSubsystem, so it's initialized per world.
    // If this is a GameInstanceSubsystem, direct dependency initialization might differ.
    // However, Get() method is often used at point of need.
    UE_LOG(LogInkNarrative, Log, TEXT("InkNarrativeSubsystem Initialized"));
}

void UInkNarrativeSubsystem::Deinitialize()
{
    UE_LOG(LogInkNarrative, Log, TEXT("InkNarrativeSubsystem Deinitializing..."));
    TArray<FName> RunnerNames;
    ActiveRunners.GetKeys(RunnerNames); // Get keys before modifying the map
    for (FName RunnerName : RunnerNames)
    {
        ReleaseRunnerResources(RunnerName);
    }
    // Ensure maps are cleared explicitly after deleting contents
    ActiveRunners.Empty();
    GlobalVariables.Empty();
    LoadedStories.Empty();

    // Clear deprecated members if they point to resources managed above
    CurrentStory = nullptr;
    MainRunner = nullptr;

    Super::Deinitialize();
    UE_LOG(LogInkNarrative, Log, TEXT("InkNarrativeSubsystem Deinitialized"));
}

// --- Deprecated Methods Implementation ---
bool UInkNarrativeSubsystem::LoadStory(const FString& StoryAssetPath)
{
    UE_LOG(LogInkNarrative, Warning, TEXT("LoadStory (by FString path) is deprecated. Please use LoadStoryForRunner with a UInkAsset. Attempting to load for MainStoryRunnerName."));
    UInkAsset* StoryAsset = LoadObject<UInkAsset>(nullptr, *StoryAssetPath);
    if (StoryAsset)
    {
        return LoadStoryForRunner(StoryAsset, MainStoryRunnerName);
    }
    UE_LOG(LogInkNarrative, Error, TEXT("Failed to load UInkAsset from path: %s for MainStoryRunnerName via deprecated LoadStory."), *StoryAssetPath);
    return false;
}

FString UInkNarrativeSubsystem::GetVariableValueAsString(const FString& VariableName, bool& bFound) const
{
    // This const_cast is not ideal but necessary if GetGlobals is non-const and this method must be const.
    // Consider if GetGlobals can be const or if this method's constness can be removed.
    return const_cast<UInkNarrativeSubsystem*>(this)->GetVariable(VariableName, MainStoryRunnerName);
}

bool UInkNarrativeSubsystem::SetVariableValueFromString(const FString& VariableName, const FString& ValueAsString)
{
    // Forward to new SetVariable method for the MainStoryRunnerName
    return SetVariable(VariableName, ValueAsString, MainStoryRunnerName);
}

bool UInkNarrativeSubsystem::GetVariableValueAsBool(const FString& VariableName, bool& bFound, bool& OutValue) const
{
    bFound = false;
    OutValue = false;
    // Use the new GetVariable method
    FString ValueStr = const_cast<UInkNarrativeSubsystem*>(this)->GetVariable(VariableName, MainStoryRunnerName); 
    
    if (!ValueStr.IsEmpty()) // Assuming GetVariable returns empty if not found or error
    {
        bFound = true; // Variable exists if string is not empty (or based on GetVariable's own logic)
        if (ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase))
        {
            OutValue = true;
            return true;
        }
        if (ValueStr.Equals(TEXT("false"), ESearchCase::IgnoreCase))
        {
            OutValue = false;
            return true;
        }
        // Try parsing as number (1 for true, 0 for false)
        if (ValueStr.IsNumeric())
        {
            int32 IntVal = FCString::Atoi(*ValueStr);
            if (IntVal != 0) { OutValue = true; return true;} // Any non-zero is true
            else { OutValue = false; return true;} // Zero is false
        }
    }
    return false; // Could not reliably convert to bool or not found
}


// --- New Multi-Runner Core Methods Implementation ---

bool UInkNarrativeSubsystem::LoadStoryForRunner(UInkAsset* StoryAsset, FName RunnerName)
{
    if (!StoryAsset || StoryAsset->CompiledStory.IsEmpty())
    {
        UE_LOG(LogInkNarrative, Error, TEXT("UInkNarrativeSubsystem::LoadStoryForRunner: Invalid or empty StoryAsset provided for Runner '%s'."), *RunnerName.ToString());
        return false;
    }

    ReleaseRunnerResources(RunnerName); // Clean up existing resources for this runner name first

    try
    {
        // Create a copy of the data for the story to own.
        // UInkAsset::CompiledStory is TArray<uint8>. GetData() gives const uint8*.
        // ink::runtime::story::from_binary expects uint8_t* (non-const) and a bool to indicate if it takes ownership.
        // So, we must make a mutable copy.
        TArray<uint8> StoryDataBuffer = StoryAsset->CompiledStory; // Make a copy
        uint8_t* StoryDataPtr = StoryDataBuffer.GetData(); // Pointer to our copy's data
        size_t StoryDataSize = StoryDataBuffer.Num();

        // The InkCPP story::from_binary documentation should be checked for ownership semantics.
        // If 'true' for owns_data means it will delete[] the pointer, then we must new[] the data.
        // If it means it just holds onto the pointer and expects it to be valid, our TArray copy is fine as long as it lives.
        // Given the common pattern, let's assume owns_data=true means it will delete[].
        
        uint8_t* DataForStoryToOwn = new uint8_t[StoryDataSize];
        FMemory::Memcpy(DataForStoryToOwn, StoryDataPtr, StoryDataSize);

        ink::runtime::story* NewStory = ink::runtime::story::from_binary(DataForStoryToOwn, StoryDataSize, true /*story owns DataForStoryToOwn*/);
        
        if (!NewStory)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("Failed to create story from asset for Runner '%s'."), *RunnerName.ToString());
            delete[] DataForStoryToOwn; // Clean up if story creation failed
            return false;
        }
        LoadedStories.Add(RunnerName, NewStory);

        ink::runtime::globals* NewGlobals = NewStory->new_globals();
        if (!NewGlobals)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("Failed to create globals for Runner '%s'."), *RunnerName.ToString());
            delete NewStory; // This will also delete DataForStoryToOwn
            LoadedStories.Remove(RunnerName);
            return false;
        }
        GlobalVariables.Add(RunnerName, NewGlobals);

        ink::runtime::runner* NewRunner = NewStory->new_runner(NewGlobals);
        if (!NewRunner)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("Failed to create runner for Runner '%s'."), *RunnerName.ToString());
            delete NewGlobals;
            delete NewStory; // Also cleans DataForStoryToOwn
            GlobalVariables.Remove(RunnerName);
            LoadedStories.Remove(RunnerName);
            return false;
        }
        ActiveRunners.Add(RunnerName, NewRunner);

        if (RunnerName == MainStoryRunnerName)
        {
            CurrentStory = NewStory;
            MainRunner = NewRunner;
        }
        
        UE_LOG(LogInkNarrative, Log, TEXT("Successfully loaded story and created runner for '%s'."), *RunnerName.ToString());
        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogInkNarrative, Error, TEXT("Exception while loading story for Runner '%s': %hs"), *RunnerName.ToString(), e.what());
        ReleaseRunnerResources(RunnerName);
        return false;
    }
    catch (...)
    {
        UE_LOG(LogInkNarrative, Error, TEXT("Unknown exception while loading story for Runner '%s'."), *RunnerName.ToString());
        ReleaseRunnerResources(RunnerName);
        return false;
    }
}

ink::runtime::runner* UInkNarrativeSubsystem::GetRunner(FName RunnerName)
{
    if (ink::runtime::runner** RunnerPtr = ActiveRunners.Find(RunnerName))
    {
        return *RunnerPtr;
    }
    return nullptr;
}

ink::runtime::globals* UInkNarrativeSubsystem::GetGlobals(FName RunnerName)
{
    if (ink::runtime::globals** GlobalsPtr = GlobalVariables.Find(RunnerName))
    {
        return *GlobalsPtr;
    }
    return nullptr;
}

bool UInkNarrativeSubsystem::IsStoryLoaded(FName RunnerName) const
{
    return ActiveRunners.Contains(RunnerName) && GlobalVariables.Contains(RunnerName) && LoadedStories.Contains(RunnerName);
}

void UInkNarrativeSubsystem::ReleaseRunnerResources(FName RunnerName)
{
    bool bReleasedSomething = false;

    if (ink::runtime::runner* Runner = ActiveRunners.FindRef(RunnerName))
    {
        delete Runner;
        ActiveRunners.Remove(RunnerName);
        bReleasedSomething = true;
        if (RunnerName == MainStoryRunnerName && MainRunner == Runner) MainRunner = nullptr;
    }
    if (ink::runtime::globals* Globals = GlobalVariables.FindRef(RunnerName))
    {
        // Before deleting globals, ensure any observers are cleared if InkCPP doesn't do it automatically.
        // (Currently, observer handles are not stored, so explicit un-observation isn't done here)
        delete Globals;
        GlobalVariables.Remove(RunnerName);
        bReleasedSomething = true;
    }
    if (ink::runtime::story* Story = LoadedStories.FindRef(RunnerName))
    {
        delete Story; // This should also delete the DataForStoryToOwn it owns
        LoadedStories.Remove(RunnerName);
        bReleasedSomething = true;
        if (RunnerName == MainStoryRunnerName && CurrentStory == Story) CurrentStory = nullptr;
    }

    if(bReleasedSomething)
    {
      UE_LOG(LogInkNarrative, Log, TEXT("Released resources for runner '%s'."), *RunnerName.ToString());
    }
}

// --- Ink Interaction Methods (Multi-Runner) Implementation ---

void UInkNarrativeSubsystem::MakeChoice(int32 ChoiceIndex, FName RunnerName)
{
    if (ink::runtime::runner* Runner = GetRunner(RunnerName))
    {
        if (Runner->num_choices() > 0 && ChoiceIndex >= 0 && ChoiceIndex < Runner->num_choices())
        {
            try
            {
                Runner->choose(ChoiceIndex);
            }
            catch (const std::exception& e)
            {
                UE_LOG(LogInkNarrative, Error, TEXT("Exception during MakeChoice on runner '%s': %hs"), *RunnerName.ToString(), e.what());
            }
        }
        else
        {
            UE_LOG(LogInkNarrative, Warning, TEXT("MakeChoice: Invalid choice index %d (num choices: %d) for runner '%s'."), ChoiceIndex, Runner->num_choices(), *RunnerName.ToString());
        }
    }
    else
    {
        UE_LOG(LogInkNarrative, Warning, TEXT("MakeChoice: Runner '%s' not found."), *RunnerName.ToString());
    }
}

FString UInkNarrativeSubsystem::GetCurrentText(FName RunnerName)
{
    FString CombinedText = TEXT("");
    if (ink::runtime::runner* Runner = GetRunner(RunnerName))
    {
        try
        {
            while (Runner->can_continue())
            {
                CombinedText += FString(UTF8_TO_TCHAR(Runner->getline().c_str()));
            }
        }
        catch (const std::exception& e)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("Exception during GetCurrentText on runner '%s': %hs"), *RunnerName.ToString(), e.what());
        }
    }
    return CombinedText;
}

TArray<FInkChoiceInfo> UInkNarrativeSubsystem::GetCurrentChoices(FName RunnerName)
{
    TArray<FInkChoiceInfo> ChoicesInfo;
    if (ink::runtime::runner* Runner = GetRunner(RunnerName))
    {
        try
        {
            for (ink::size_t i = 0; i < Runner->num_choices(); ++i)
            {
                ink::runtime::choice Choice = Runner->get_choice(i);
                FInkChoiceInfo Info;
                Info.Text = FString(UTF8_TO_TCHAR(Choice.text()));
                Info.Index = Choice.index(); 
                // For tags:
                // for(const char* tag : Choice.tags()) { Info.Tags.Add(FString(UTF8_TO_TCHAR(tag))); }
                ChoicesInfo.Add(Info);
            }
        }
        catch (const std::exception& e)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("Exception during GetCurrentChoices on runner '%s': %hs"), *RunnerName.ToString(), e.what());
        }
    }
    return ChoicesInfo;
}

// --- Variable Observation (Multi-Runner) Implementation ---

void UInkNarrativeSubsystem::OnInkVariableUpdate(const char* VarName, ink::runtime::value NewInkValue, FName RunnerName)
{
    if (OnInkVariableChanged.IsBound())
    {
        FString VariableNameStr(UTF8_TO_TCHAR(VarName));
        FString ValueStr = ConvertInkValueToString(NewInkValue, FName(VariableNameStr));
        
        // Ensure this broadcast happens on the game thread.
        // If InkCPP callbacks can occur on other threads, this needs to be marshaled.
        // For now, assuming InkCPP callbacks are synchronous with game logic or game-thread safe.
        OnInkVariableChanged.Broadcast(FName(VariableNameStr), ValueStr);
    }
}

void UInkNarrativeSubsystem::ObserveVariable(const FString& VariableName, FName RunnerName)
{
    if (ink::runtime::globals* Globals = GetGlobals(RunnerName))
    {
        try
        {
            // The InkCPP observe method returns a handle (often just a bool or void if it doesn't support specific un-observation).
            // If specific un-observation is needed, this handle would need to be stored.
            Globals->observe(TCHAR_TO_UTF8(*VariableName), 
                [this, RunnerName](const char* VarName, ink::runtime::value NewValue) {
                    if(IsValid(this)) // Essential check for UObject validity in async or long-lived callbacks
                    {
                        this->OnInkVariableUpdate(VarName, NewValue, RunnerName);
                    }
                }
            );
            UE_LOG(LogInkNarrative, Log, TEXT("Observing variable '%s' on runner '%s'"), *VariableName, *RunnerName.ToString());
        }
        catch (const std::exception& e)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("Exception while observing variable '%s' on runner '%s': %hs"), *VariableName, *RunnerName.ToString(), e.what());
        }
    }
    else
    {
        UE_LOG(LogInkNarrative, Warning, TEXT("ObserveVariable: Globals not found for runner '%s' when trying to observe '%s'."), *RunnerName.ToString(), *VariableName);
    }
}

// --- Get/Set Variables (Multi-Runner) Implementation ---

bool UInkNarrativeSubsystem::SetVariable(const FString& VariableName, const FString& Value, FName RunnerName)
{
    if (ink::runtime::globals* Globals = GetGlobals(RunnerName))
    {
        try
        {
            ink::runtime::value InkVal;
            // Type inference from FString.
            if (Value.Equals(TEXT("true"), ESearchCase::IgnoreCase))
            {
                InkVal = ink::runtime::value(true);
            }
            else if (Value.Equals(TEXT("false"), ESearchCase::IgnoreCase))
            {
                InkVal = ink::runtime::value(false);
            }
            else if (Value.IsNumeric() && !Value.Contains(TEXT("."))) 
            {
                InkVal = ink::runtime::value(FCString::Atoi(*Value));
            }
            else if (Value.IsNumeric()) 
            {
                InkVal = ink::runtime::value(FCString::Atof(*Value));
            }
            else 
            {
                InkVal = ink::runtime::value(TCHAR_TO_UTF8(*Value));
            }
            
            bool bSuccess = Globals->set_var(TCHAR_TO_UTF8(*VariableName), InkVal);
            if(!bSuccess)
            {
                 UE_LOG(LogInkNarrative, Warning, TEXT("SetVariable: Ink globals->set_var failed for '%s' on runner '%s'. Variable might not exist or type mismatch."), *VariableName, *RunnerName.ToString());
            }
            return bSuccess;
        }
        catch (const std::exception& e)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("Exception while setting variable '%s' on runner '%s': %hs"), *VariableName, *RunnerName.ToString(), e.what());
            return false;
        }
    }
    UE_LOG(LogInkNarrative, Warning, TEXT("SetVariable: Globals not found for runner '%s' when trying to set '%s'."), *RunnerName.ToString(), *VariableName);
    return false;
}

FString UInkNarrativeSubsystem::GetVariable(const FString& VariableName, FName RunnerName)
{
    if (ink::runtime::globals* Globals = GetGlobals(RunnerName))
    {
        try
        {
            TOptional<ink::runtime::value> InkValOpt = Globals->get_var(TCHAR_TO_UTF8(*VariableName));
            if (InkValOpt.IsSet())
            {
                return ConvertInkValueToString(InkValOpt.GetValue(), FName(VariableName));
            }
            // else: Variable not found, do not log warning spam, just return empty.
        }
        catch (const std::exception& e)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("Exception while getting variable '%s' on runner '%s': %hs"), *VariableName, *RunnerName.ToString(), e.what());
        }
    }
    // else: Globals not found, do not log warning spam.
    return FString(); 
}

// --- External Function Binding Implementation ---
void UInkNarrativeSubsystem::BindExternalFunctionWithMessage(const FString& FunctionName, FGameplayTag MessageChannelTag)
{
    // Bind to the MainStoryRunnerName by default as per current .h declaration.
    // For multi-runner binding, this function would need a RunnerName parameter.
    if (ink::runtime::globals* Globals = GetGlobals(MainStoryRunnerName))
    {
        // It's important that 'this' (UInkNarrativeSubsystem) outlives the binding or that bindings are cleared in Deinitialize.
        // UGameplayMessageSubsystem::Get requires a WorldContextObject. 'this' (a GameInstanceSubsystem) can provide it via GetWorld().
        UWorld* World = GetWorld();
        if (!World)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("BindExternalFunctionWithMessage: Cannot get World Context for GameplayMessageSubsystem. Function '%s' not bound."), *FunctionName);
            return;
        }
        UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);

        try
        {
            // The lambda captures 'this', FunctionName, MessageChannelTag, and a reference to MessageSubsystem.
            // Ensure these are valid for the lifetime of the callback.
            Globals->bind_external_function(TCHAR_TO_UTF8(*FunctionName),
                [this, FnName = FunctionName, MsgChanTag = MessageChannelTag, &MessageSubsystem](ink::runtime::runner& CallerRunner, ink::runtime::expected_args& Args)
                {
                    if(!IsValid(this) || !IsValid(&MessageSubsystem)) return; // Essential validity checks

                    FInkExternalFunctionMessage Message;
                    Message.FunctionName = FName(FnName);
                    
                    // Attempt to find which of our managed runners matches the CallerRunner from Ink.
                    Message.RunnerName = MainStoryRunnerName; // Default if not found
                    for(auto const& [Name, RunnerPtr] : ActiveRunners)
                    {
                        if(RunnerPtr == &CallerRunner)
                        {
                            Message.RunnerName = Name;
                            break;
                        }
                    }

                    for (ink::size_t i = 0; i < Args.num_args(); ++i)
                    {
                        // Args.get_var<T> can throw if type mismatches or index out of bounds.
                        // A try-catch here might be good, or ensure Args provides safe access.
                        Message.Arguments.Add(ConvertInkValueToString(Args.get_var<ink::runtime::value>(i)));
                    }
                    
                    MessageSubsystem.BroadcastMessage(MsgChanTag, Message);
                });
            UE_LOG(LogInkNarrative, Log, TEXT("Bound external function '%s' to message on channel '%s' for MainStoryRunner."), *FunctionName, *MessageChannelTag.ToString());
        }
        catch (const std::exception& e)
        {
            UE_LOG(LogInkNarrative, Error, TEXT("Exception binding external function '%s' for MainStoryRunner: %hs"), *FunctionName, e.what());
        }
    }
    else
    {
        UE_LOG(LogInkNarrative, Warning, TEXT("BindExternalFunctionWithMessage: Globals for MainStoryRunner not found when trying to bind '%s'."), *FunctionName);
    }
}


// --- Utility Implementation ---

FString UInkNarrativeSubsystem::ConvertInkValueToString(const ink::runtime::value& InkValue, const FName& PropertyNameForLogging) const
{
    // This is based on ink::runtime::value from InkCPP v3
    switch (InkValue.type())
    {
        case ink::runtime::value_type::Bool:
            return InkValue.get<ink::runtime::value_type::Bool>() ? TEXT("true") : TEXT("false");
        case ink::runtime::value_type::Int32:
            return FString::FromInt(InkValue.get<ink::runtime::value_type::Int32>());
        case ink::runtime::value_type::String:
            return FString(UTF8_TO_TCHAR(InkValue.get<ink::runtime::value_type::String>()));
        case ink::runtime::value_type::Float:
            return FString::SanitizeFloat(InkValue.get<ink::runtime::value_type::Float>());
        case ink::runtime::value_type::List: // InkCPP v3 uses "list_interface"
             // For lists, you might iterate through them or get a string representation if available.
             // Example: return FString(InkValue.get<ink::runtime::value_type::List>()->to_string().c_str());
             // This needs specific InkCPP list handling.
             return TEXT("[Ink List]"); 
        case ink::runtime::value_type::DivertTarget: // InkCPP v3 uses "divert"
             return FString::Printf(TEXT("[DivertPath:%s]"), UTF8_TO_TCHAR(InkValue.get<ink::runtime::value_type::DivertTarget>()->path_string()));
        case ink::runtime::value_type::None: // InkCPP v3 might call this something else or handle nulls differently
            return TEXT("[Ink None]");
        case ink::runtime::value_type::Void:
            return TEXT("[Ink Void]"); // Typically for function returns, not stored variables.
        default:
            // This path should ideally not be hit if all types are handled.
            // InkCPP v3 might have slightly different type enums or names.
            UE_LOG(LogInkNarrative, Warning, TEXT("ConvertInkValueToString: Unhandled ink value type for '%s'. Type Enum: %d"), 
                PropertyNameForLogging.IsNone() ? TEXT("UnknownProperty") : *PropertyNameForLogging.ToString(), 
                static_cast<int>(InkValue.type()));
            return FString(TEXT("[Unhandled Ink Type]"));
    }
}
