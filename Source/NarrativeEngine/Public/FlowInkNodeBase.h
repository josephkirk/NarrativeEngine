// Copyright Joseph Kirk 2025

#pragma once

#include "Nodes/FlowNode.h"
#include "FlowInkNodeBase.generated.h"

/**
 * Base class for Flow Nodes that interact with Ink stories.
 */
UCLASS(Abstract, NotBlueprintable)
class NARRATIVEENGINE_API UFlowInkNodeBase : public UFlowNode
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	// UFlowNode interface
	virtual FString GetNodeCategory() const override;
	// End UFlowNode interface
#endif
};
