// Copyright Joseph Kirk 2025

#include "FlowInkTextNode.h"
#include "Types/FlowDataPin.h"
#include "EdGraph/EdGraphSchema_K2.h"
#include "InkNarrativeSubsystem.h"
#include "Kismet/GameplayStatics.h"

// InkCPP Includes
#include "ink/runner.h"

#define LOCTEXT_NAMESPACE "FlowInkTextNode"

UFlowInkTextNode::UFlowInkTextNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputPins.Add(FFlowPin(TEXT("In"))); // Default Input Pin
	FinishedPin = FFlowPin(TEXT("Finished"));
	ErrorPin = FFlowPin(TEXT("Error")); // Initialize ErrorPin
	OutputPins.Add(FinishedPin);
	OutputPins.Add(ErrorPin); // Add ErrorPin to OutputPins

	// SetupDataPins(); // Called by OnLoad
}

void UFlowInkTextNode::OnLoad_Implementation()
{
	Super::OnLoad_Implementation();
	SetupDataPins();
}

void UFlowInkTextNode::SetupDataPins()
{
	OutputDataPins.Empty(); // Clear any pre-existing, just in case

	const FName TextOutPinName = TEXT("TextOut");
	FFlowPin TextOutDataPin(TextOutPinName);
	TextOutDataPin.PinToolTip = TEXT("The line of text fetched from the Ink story.");
#if WITH_EDITOR
	TextOutDataPin.PinFriendlyName = LOCTEXT("TextOutPinFriendlyName", "Text");
#endif
	TextOutDataPin.PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	OutputDataPins.Add(TextOutDataPin);
}

void UFlowInkTextNode::ExecuteInput(const FName& PinName)
{
	FetchedText = FText::GetEmpty(); // Clear previous text
	bool bSuccess = false;

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		const FString ErrorMsg = FString::Printf(TEXT("FlowInkTextNode (%s): Failed to get GameInstance."), *GetName());
		UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		// SetPropertyValue must be called before triggering output pin if it's to be used by connected nodes
		SetPropertyValue<FText>(TEXT("TextOut"), FetchedText);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	UInkNarrativeSubsystem* InkSubsystem = GameInstance->GetSubsystem<UInkNarrativeSubsystem>();
	if (!InkSubsystem || !InkSubsystem->IsStoryLoaded())
	{
		const FString ErrorMsg = FString::Printf(TEXT("FlowInkTextNode (%s): InkNarrativeSubsystem not available or story not loaded."), *GetName());
		UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		SetPropertyValue<FText>(TEXT("TextOut"), FetchedText);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	ink::runtime::runner* Runner = InkSubsystem->GetRunner();
	if (!Runner)
	{
		const FString ErrorMsg = FString::Printf(TEXT("FlowInkTextNode (%s): Ink Runner is not valid."), *GetName());
		UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMsg);
		LogError(ErrorMsg);
		SetPropertyValue<FText>(TEXT("TextOut"), FetchedText);
		TriggerOutput(ErrorPin.PinName, true);
		return;
	}

	// Check if the story can continue
	if (Runner->can_continue())
	{
		const char* InkLineAnsi = Runner->getline();
		if (InkLineAnsi && InkLineAnsi[0] != '\0') // Check for non-null and non-empty string
		{
			FetchedText = FText::FromString(ANSI_TO_TCHAR(InkLineAnsi));
			UE_LOG(LogTemp, Log, TEXT("FlowInkTextNode (%s): Fetched Ink Text: '%s'"), *GetName(), *FetchedText.ToString());
			bSuccess = true;
		}
		else
		{
			const FString WarnMsg = FString::Printf(TEXT("FlowInkTextNode (%s): can_continue was true, but getline() returned null or empty. Story might be waiting for choice or at different content type."), *GetName());
			UE_LOG(LogTemp, Warning, TEXT("%s"), *WarnMsg);
			// Depending on strictness, this could be an error or just mean "no text line here"
			// For a node named "Ink Text", we expect text. So, treat as non-success.
			LogError(WarnMsg); // Log as error as this node expects text
			// FetchedText is already empty
		}
	}
	else
	{
		const FString LogMsg = FString::Printf(TEXT("FlowInkTextNode (%s): Ink story cannot continue (or has ended). No text fetched."), *GetName());
		UE_LOG(LogTemp, Log, TEXT("%s"), *LogMsg);
		LogError(LogMsg); // Log as an error for this node as it expects to fetch text
		// FetchedText is already empty
	}

	SetPropertyValue<FText>(TEXT("TextOut"), FetchedText);

	if (bSuccess)
	{
		TriggerOutput(FinishedPin.PinName, true);
	}
	else
	{
		TriggerOutput(ErrorPin.PinName, true);
	}
}

#if WITH_EDITOR
FString UFlowInkTextNode::GetNodeDescription() const
{
	return FetchedText.IsEmpty() ? LOCTEXT("NodeDescNotRun", "Displays Ink story text (runtime)").ToString() : FetchedText.ToString();
}

FString UFlowInkTextNode::GetStatusString() const
{
	if (!FetchedText.IsEmpty())
	{
		return FString::Printf(TEXT("%s..."), *FetchedText.ToString().Left(20));
	}
	return LOCTEXT("StatusNotRun", "(No text yet)").ToString();
}
#endif

#undef LOCTEXT_NAMESPACE
