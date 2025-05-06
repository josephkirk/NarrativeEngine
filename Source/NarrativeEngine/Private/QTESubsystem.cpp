// Copyright Epic Games, Inc. All Rights Reserved.


#include "QTESubsystem.h"
#include "QTEDataAsset.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/CoreDelegates.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeEngine/Public/NarrativeEngineLog.h"
#include "Blueprint/UserWidget.h"         // <<< Added include
#include "GameFramework/PlayerController.h" // <<< Added include
#include "NarrativeEngine.h" // <<< Added include for LogNarrativeEngine

UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem(const UObject* WorldContextObject)
{
    UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
    if (!GameInstance) return nullptr;

    if (const UWorld* World = GameInstance->GetWorld())
    {
        if (const APlayerController* PC = World->GetFirstPlayerController())
        {
            if (const ULocalPlayer* LP = PC->GetLocalPlayer())
            {
                return LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
            }
        }
    }
    return nullptr;
}

void UQTESubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogNarrativeEngine, Log, TEXT("QTE Subsystem Initialized"));
    UniqueIDCounter = 0;
}

void UQTESubsystem::Deinitialize()
{
    UE_LOG(LogNarrativeEngine, Log, TEXT("QTE Subsystem Deinitialized"));
    if (bIsQTEActive)
    {
        EndQTE(false); 
    }
    Super::Deinitialize();
}

FName UQTESubsystem::StartQTE(const UQTEDataAsset* QTEDataAsset, FName OptionalInstanceID)
{
    if (bIsQTEActive)
    {
        UE_LOG(LogNarrativeEngine, Warning, TEXT("StartQTE called while another QTE (ID: %s) is already active. Ignoring."), *ActiveQTEInfo.InstanceID.ToString());
        return NAME_None;
    }

    if (!QTEDataAsset)
    {
        UE_LOG(LogNarrativeEngine, Error, TEXT("StartQTE called with null QTEDataAsset."));
        return NAME_None;
    }

    if (!QTEDataAsset->RequiredAction)
    {
        UE_LOG(LogNarrativeEngine, Error, TEXT("StartQTE called, but QTEDataAsset '%s' has no RequiredAction specified."), *QTEDataAsset->GetName());
        return NAME_None;
    }

    if (!QTE_InputMappingContext)
    {
        UE_LOG(LogNarrativeEngine, Error, TEXT("StartQTE called but QTE_InputMappingContext is not set in QTESubsystem defaults."));
        return NAME_None;
    }

    InputSubsystem = GetEnhancedInputSubsystem(this);
    if (!InputSubsystem)
    {
        UE_LOG(LogNarrativeEngine, Error, TEXT("StartQTE failed: Could not get EnhancedInputLocalPlayerSubsystem."));
        return NAME_None;
    }

    FName InstanceID = OptionalInstanceID;
    if (InstanceID == NAME_None)
    {
        InstanceID = FName(*FString::Printf(TEXT("QTEInst_%llu"), ++UniqueIDCounter));
    }

    ActiveQTEInfo = FActiveQTEInfo(QTEDataAsset, InstanceID);
    bIsQTEActive = true;
    MashCount = 0;
    HoldStartTime = 0.0f;
    InputBindingHandles.Empty();

    UE_LOG(LogNarrativeEngine, Log, TEXT("Starting QTE Instance: %s (Asset: %s, Type: %d, Duration: %.2f)"), 
        *ActiveQTEInfo.InstanceID.ToString(), 
        *ActiveQTEInfo.QTEData->GetName(),
        static_cast<int32>(ActiveQTEInfo.GetType()), 
        ActiveQTEInfo.GetDuration());

    InputSubsystem->AddMappingContext(QTE_InputMappingContext, 1);

    if (UEnhancedInputComponent* PlayerInputComponent = Cast<UEnhancedInputComponent>(InputSubsystem->GetOuter()->InputComponent))
    {
        switch (ActiveQTEInfo.GetType())
        {
            case EQTEType::TimedPress:
            case EQTEType::Mash:
                InputBindingHandles.Add(PlayerInputComponent->BindAction(ActiveQTEInfo.GetRequiredAction(), ETriggerEvent::Triggered, this, &UQTESubsystem::HandleQTEInputTriggered, ActiveQTEInfo.InstanceID).GetHandle());
                break;
            case EQTEType::Hold:
                InputBindingHandles.Add(PlayerInputComponent->BindAction(ActiveQTEInfo.GetRequiredAction(), ETriggerEvent::Started, this, &UQTESubsystem::HandleQTEInputStarted, ActiveQTEInfo.InstanceID).GetHandle());
                InputBindingHandles.Add(PlayerInputComponent->BindAction(ActiveQTEInfo.GetRequiredAction(), ETriggerEvent::Completed, this, &UQTESubsystem::HandleQTEInputCompleted, ActiveQTEInfo.InstanceID).GetHandle());
                InputBindingHandles.Add(PlayerInputComponent->BindAction(ActiveQTEInfo.GetRequiredAction(), ETriggerEvent::Canceled, this, &UQTESubsystem::HandleQTEInputCanceled, ActiveQTEInfo.InstanceID).GetHandle());
                break;
        }

        if(ActiveQTEInfo.GetFailAction())
        {
            InputBindingHandles.Add(PlayerInputComponent->BindAction(ActiveQTEInfo.GetFailAction(), ETriggerEvent::Triggered, this, &UQTESubsystem::HandleQTEFailInputTriggered, ActiveQTEInfo.InstanceID).GetHandle());
        }
    }
    else
    {
        UE_LOG(LogNarrativeEngine, Error, TEXT("StartQTE failed: Could not get EnhancedInputComponent to bind actions for QTE %s."), *ActiveQTEInfo.InstanceID.ToString());
        InputSubsystem->RemoveMappingContext(QTE_InputMappingContext);
        bIsQTEActive = false;
        return NAME_None;
    }

    UWorld* World = GetWorld(); 
    if (World)
    {
        World->GetTimerManager().SetTimer(QTEDurationTimerHandle, this, &UQTESubsystem::TimeoutQTE, ActiveQTEInfo.GetDuration(), false);
    }
    else
    {
        UE_LOG(LogNarrativeEngine, Error, TEXT("StartQTE failed: Could not get World to set timer for QTE %s."), *ActiveQTEInfo.InstanceID.ToString());
        EndQTE(false); 
        return NAME_None;
    }

    OnQTEStarted.Broadcast(ActiveQTEInfo.InstanceID, ActiveQTEInfo.QTEData);

    // --- Create and Show Widget --- //
    if (TSubclassOf<UUserWidget> WidgetClass = ActiveQTEInfo.GetWidgetClass())
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController(); // Assumes single local player
        if (PC)
        {
            UUserWidget* NewWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
            if (NewWidget)
            {
                ActiveQTEWidgets.Add(InstanceID, NewWidget);
                NewWidget->AddToViewport();
                UE_LOG(LogNarrativeEngine, Log, TEXT("Added QTE widget for instance %s"), *InstanceID.ToString());
            }
            else
            {
                UE_LOG(LogNarrativeEngine, Warning, TEXT("Failed to create QTE widget for instance %s from class %s"), *InstanceID.ToString(), *WidgetClass->GetName());
            }
        }
        else
        {
            UE_LOG(LogNarrativeEngine, Warning, TEXT("Could not get PlayerController to create QTE widget for instance %s"), *InstanceID.ToString());
        }
    }
    // --- End Widget Handling --- //

    return ActiveQTEInfo.InstanceID;
}

bool UQTESubsystem::CancelActiveQTE()
{
    if (bIsQTEActive)
    {
        UE_LOG(LogNarrativeEngine, Log, TEXT("Explicitly cancelling active QTE: %s"), *ActiveQTEInfo.InstanceID.ToString());
        EndQTE(false); 
        return true;
    }
    return false;
}

void UQTESubsystem::EndQTE(bool bSuccess)
{
    if (!bIsQTEActive)
    {
        return; 
    }

    FName EndedInstanceID = ActiveQTEInfo.InstanceID;
    UE_LOG(LogNarrativeEngine, Log, TEXT("Ending QTE Instance: %s (Asset: %s, Success: %s)"), 
        *EndedInstanceID.ToString(), 
        ActiveQTEInfo.QTEData ? *ActiveQTEInfo.QTEData->GetName() : TEXT("null"), 
        bSuccess ? TEXT("True") : TEXT("False"));

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(QTEDurationTimerHandle);
    }

    if (!InputSubsystem)
    {
        InputSubsystem = GetEnhancedInputSubsystem(this);
    }

    if (InputSubsystem)
    {
        if (UEnhancedInputComponent* PlayerInputComponent = Cast<UEnhancedInputComponent>(InputSubsystem->GetOuter()->InputComponent))
        {
            for (uint32 Handle : InputBindingHandles)
            {
                PlayerInputComponent->RemoveBindingByHandle(Handle);
            }
        }
        InputBindingHandles.Empty(); 

        InputSubsystem->RemoveMappingContext(QTE_InputMappingContext);
    }
    else
    {
        UE_LOG(LogNarrativeEngine, Warning, TEXT("EndQTE: Could not get EnhancedInputLocalPlayerSubsystem to unbind actions or remove context for QTE %s."), *EndedInstanceID.ToString());
    }

    bIsQTEActive = false;
    ActiveQTEInfo = FActiveQTEInfo(); 
    MashCount = 0;
    HoldStartTime = 0.0f;

    // --- Remove Widget --- //
    if (TObjectPtr<UUserWidget>* FoundWidgetPtr = ActiveQTEWidgets.Find(EndedInstanceID))
    {
        if (UUserWidget* WidgetToRemove = *FoundWidgetPtr)
        {
            WidgetToRemove->RemoveFromParent();
            UE_LOG(LogNarrativeEngine, Log, TEXT("Removed QTE widget for instance %s"), *EndedInstanceID.ToString());
        }
        ActiveQTEWidgets.Remove(EndedInstanceID);
    }
    // --- End Widget Handling --- //

    if (bSuccess)
    {
        OnQTESucceeded.Broadcast(EndedInstanceID);
    }
    else
    {
        OnQTEFailed.Broadcast(EndedInstanceID);
    }
}

void UQTESubsystem::TimeoutQTE()
{
    if (!bIsQTEActive)
    {
        return; 
    }

    UE_LOG(LogNarrativeEngine, Log, TEXT("QTE Timed Out: %s (Asset: %s)"), 
        *ActiveQTEInfo.InstanceID.ToString(), 
        ActiveQTEInfo.QTEData ? *ActiveQTEInfo.QTEData->GetName() : TEXT("null"));
    EndQTE(false); 
}

void UQTESubsystem::HandleQTEInputTriggered(const FInputActionInstance& Instance, FName InstanceID)
{
    if (!bIsQTEActive || !ActiveQTEInfo.IsValid() || ActiveQTEInfo.InstanceID != InstanceID)
    {
        return; 
    }

    UE_LOG(LogNarrativeEngine, Verbose, TEXT("HandleQTEInputTriggered for QTE: %s (Asset: %s)"), *InstanceID.ToString(), *ActiveQTEInfo.QTEData->GetName());

    if (ActiveQTEInfo.GetType() == EQTEType::TimedPress)
    {
        UE_LOG(LogNarrativeEngine, Log, TEXT("QTE '%s' succeeded (Timed Press)."), *InstanceID.ToString());
        EndQTE(true);
        return;
    }

    if (ActiveQTEInfo.GetType() == EQTEType::Mash)
    {
        MashCount++;
        float Progress = FMath::Clamp(static_cast<float>(MashCount) / ActiveQTEInfo.GetSuccessThreshold(), 0.0f, 1.0f);
        UE_LOG(LogNarrativeEngine, Verbose, TEXT("QTE '%s' Mash Count: %d (Progress: %.2f)"), *InstanceID.ToString(), MashCount, Progress);
        OnQTEProgress.Broadcast(InstanceID, Progress);

        if (MashCount >= static_cast<int32>(ActiveQTEInfo.GetSuccessThreshold()))
        {
            UE_LOG(LogNarrativeEngine, Log, TEXT("QTE '%s' succeeded (Mash Threshold Reached)."), *InstanceID.ToString());
            EndQTE(true);
            return;
        }
    }
}

void UQTESubsystem::HandleQTEInputStarted(const FInputActionInstance& Instance, FName InstanceID)
{
    if (!bIsQTEActive || !ActiveQTEInfo.IsValid() || ActiveQTEInfo.InstanceID != InstanceID)
    {
        return;
    }
    
    if (ActiveQTEInfo.GetType() == EQTEType::Hold)
    {
        UE_LOG(LogNarrativeEngine, Verbose, TEXT("HandleQTEInputStarted for Hold QTE: %s (Asset: %s)"), *InstanceID.ToString(), *ActiveQTEInfo.QTEData->GetName());
        if(UWorld* World = GetWorld())
        {
            HoldStartTime = World->GetTimeSeconds();
        }
        else
        {
            HoldStartTime = 0.0f; 
        }
        OnQTEProgress.Broadcast(InstanceID, 0.0f); 
    }
}

void UQTESubsystem::HandleQTEInputCompleted(const FInputActionInstance& Instance, FName InstanceID)
{
    if (!bIsQTEActive || !ActiveQTEInfo.IsValid() || ActiveQTEInfo.InstanceID != InstanceID)
    {
        return;
    }
    
    if (ActiveQTEInfo.GetType() == EQTEType::Hold)
    {
        UE_LOG(LogNarrativeEngine, Verbose, TEXT("HandleQTEInputCompleted for Hold QTE: %s (Asset: %s)"), *InstanceID.ToString(), *ActiveQTEInfo.QTEData->GetName());
        UE_LOG(LogNarrativeEngine, Log, TEXT("QTE '%s' succeeded (Hold Completed)."), *InstanceID.ToString());
        OnQTEProgress.Broadcast(InstanceID, 1.0f); 
        EndQTE(true); 
    }
}

void UQTESubsystem::HandleQTEInputCanceled(const FInputActionInstance& Instance, FName InstanceID)
{
    if (!bIsQTEActive || !ActiveQTEInfo.IsValid() || ActiveQTEInfo.InstanceID != InstanceID)
    {
        return;
    }
    
    if (ActiveQTEInfo.GetType() == EQTEType::Hold)
    {
        UE_LOG(LogNarrativeEngine, Log, TEXT("QTE '%s' failed (Hold Canceled/Released Early)."), *InstanceID.ToString());
        float HeldDuration = 0.0f;
        if (HoldStartTime > 0.0f && GetWorld())
        {
            HeldDuration = GetWorld()->GetTimeSeconds() - HoldStartTime;
            float RequiredHoldTime = ActiveQTEInfo.GetSuccessThreshold(); 
            float Progress = FMath::Clamp(HeldDuration / RequiredHoldTime, 0.0f, 1.0f);
            UE_LOG(LogNarrativeEngine, Verbose, TEXT("Hold Cancelled after %.2f seconds (Progress: %.2f)"), HeldDuration, Progress);
            OnQTEProgress.Broadcast(InstanceID, Progress); 
        }
        EndQTE(false); 
    }
}

void UQTESubsystem::HandleQTEFailInputTriggered(const FInputActionInstance& Instance, FName InstanceID)
{
    if (!bIsQTEActive || !ActiveQTEInfo.IsValid() || ActiveQTEInfo.InstanceID != InstanceID)
    {
        return;
    }
    UE_LOG(LogNarrativeEngine, Log, TEXT("Fail Action Triggered for QTE: %s. Failing QTE."), *InstanceID.ToString());
    EndQTE(false); 
}
