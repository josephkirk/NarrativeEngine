// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"
#include "QTESubsystem.h" // Include for EQTEType enum
#include "QTEDataAsset.generated.h"

/**
 * Data Asset defining the parameters for a specific type of Quick Time Event.
 */
UCLASS(BlueprintType)
class NARRATIVEENGINE_API UQTEDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// Type of QTE (Timed Press, Mash, Hold)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE")
	EQTEType Type = EQTEType::TimedPress;

	// Total duration the QTE is active if not completed successfully
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE", meta = (ClampMin = "0.1"))
	float Duration = 3.0f;

	// The Input Action required for success
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE")
	TObjectPtr<UInputAction> RequiredAction = nullptr;

	// For Mash type: Number of presses needed for success
    // For Hold type: Required hold duration in seconds (supplements/overrides IMC trigger setting if needed for checks)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE", meta = (EditCondition = "Type == EQTEType::Mash || Type == EQTEType::Hold", EditConditionHides, ClampMin = "0.1"))
	float SuccessThreshold = 5.0f; // Presses for Mash, Seconds for Hold

	// Optional: Input Action for cancelling/failing early (e.g., wrong button press)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE")
	TObjectPtr<UInputAction> FailAction = nullptr;

	// Optional: Text prompt to display to the user during the QTE
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE")
	FText PromptText;

	// The User Widget class to display during the QTE
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE|UI")
	TSubclassOf<UUserWidget> QTEWidgetClass = nullptr;

	// Add any other reusable data: Sound cues, particle effects, etc.
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE|Visuals")
	// TObjectPtr<UParticleSystem> SuccessEffect = nullptr;

	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE|Audio")
	// TObjectPtr<USoundCue> StartSound = nullptr;
};
