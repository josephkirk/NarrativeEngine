// Copyright Joseph Kirk 2025

#include "FlowInkNodeBase.h" // Adjusted include path

UFlowInkNodeBase::UFlowInkNodeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	Category = TEXT("Ink");
#endif
}

#if WITH_EDITOR
FString UFlowInkNodeBase::GetNodeCategory() const
{
	return TEXT("Ink");
}
#endif
