// Copyright Joseph Kirk 2025

#pragma once

#include "FlowInkNodeBase.h"
#include "FlowInkPropertyNode.generated.h"

UENUM(BlueprintType)
enum class EFlowInkVariableType : uint8
{
	Bool	UMETA(DisplayName = "Boolean"),
	Int		UMETA(DisplayName = "Integer"),
	Float	UMETA(DisplayName = "Float"),
	String	UMETA(DisplayName = "String")
};

UENUM(BlueprintType)
enum class EInkPropertyOperation : uint8
{
	Get UMETA(DisplayName = "Get Property"),
	Set UMETA(DisplayName = "Set Property")
};

/**
 * FlowNode to get or set an Ink story property (variable).
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Ink Property"))
class NARRATIVEENGINE_API UFlowInkPropertyNode : public UFlowInkNodeBase
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Ink")
	FName PropertyName;

	UPROPERTY(EditAnywhere, Category = "Ink", meta = (ReconstructNode = "True")) // ReconstructNode to update pins
	EInkPropertyOperation Operation;

	// The expected type of the Ink variable. This determines the type of the 'Value' data pin.
	UPROPERTY(EditAnywhere, Category = "Ink", meta = (ReconstructNode = "True"))
	EFlowInkVariableType VariableType;

	// Output pin that is triggered after the operation
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	FFlowPin OutPin;

	// Output pin that is triggered if an error occurs during execution
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	FFlowPin ErrorPin;

public:
	virtual void ExecuteInput(const FName& PinName) override;

	//~ Begin UFlowNode Interface
	virtual void OnLoad_Implementation() override;
	//~ End UFlowNode Interface

#if WITH_EDITOR
	virtual FString GetNodeDescription() const override;
	virtual FString GetStatusString() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	void SetupDataPins();
};
