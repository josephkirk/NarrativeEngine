// Copyright Joseph Kirk 2025

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayMessageProcessor.h" // Assuming UBT resolves this from GameplayMessageRuntime module
#include "InkEventMessageTypes.generated.h"

/**
 * Message payload for when an Ink story calls an EXTERNAL function.
 * This can be broadcasted via UGameplayMessageSubsystem.
 */
USTRUCT(BlueprintType)
struct NARRATIVEENGINE_API FInkExternalFunctionMessage : public FGameplayMessageBase
{
	GENERATED_BODY()

public:
	// The name of the external function called from Ink.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Message")
	FName FunctionName;

	// The arguments passed to the external function, converted to strings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Message")
	TArray<FString> Arguments;

	FInkExternalFunctionMessage() = default;

	FInkExternalFunctionMessage(const FName& InFunctionName, const TArray<FString>& InArguments)
		: FunctionName(InFunctionName)
		, Arguments(InArguments)
	{}
};
