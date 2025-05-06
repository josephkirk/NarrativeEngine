#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "QTEDataAsset.h"
#include "Blueprint/UserWidget.h" // <<< Added include
#include "QTESubsystem.generated.h"

// Forward declaration
struct FInputActionInstance;
class UQTEDataAsset;
class UUserWidget; // <<< Added forward declaration

UENUM(BlueprintType)
enum class EQTEType : uint8
{
    TimedPress UMETA(DisplayName = "Timed Press"),
    Mash       UMETA(DisplayName = "Mash"),
    Hold       UMETA(DisplayName = "Hold")
};

USTRUCT(BlueprintType)
struct NARRATIVEENGINE_API FActiveQTEInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    TObjectPtr<const UQTEDataAsset> QTEData = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    FName InstanceID = NAME_None;

    FActiveQTEInfo() = default;

    FActiveQTEInfo(const UQTEDataAsset* InData, FName InID)
        : QTEData(InData), InstanceID(InID) {}

    bool IsValid() const { return QTEData != nullptr && InstanceID != NAME_None; }

    EQTEType GetType() const { return QTEData ? QTEData->Type : EQTEType::TimedPress; }
    float GetDuration() const { return QTEData ? QTEData->Duration : 0.0f; }
    const UInputAction* GetRequiredAction() const { return QTEData ? QTEData->RequiredAction : nullptr; }
    const UInputAction* GetFailAction() const { return QTEData ? QTEData->FailAction : nullptr; }
    float GetSuccessThreshold() const { return QTEData ? QTEData->SuccessThreshold : 0.0f; }
    FText GetPromptText() const { return QTEData ? QTEData->PromptText : FText::GetEmpty(); }
};

UCLASS()
class NARRATIVEENGINE_API UQTESubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQTEStarted, FName, InstanceID, const UQTEDataAsset*, QTEData);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQTESucceeded, FName, InstanceID);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQTEFailed, FName, InstanceID);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQTEProgress, FName, InstanceID, float, Progress);

    UPROPERTY(BlueprintAssignable, Category = "QTE|Delegates")
    FOnQTEStarted OnQTEStarted;

    UPROPERTY(BlueprintAssignable, Category = "QTE|Delegates")
    FOnQTESucceeded OnQTESucceeded;

    UPROPERTY(BlueprintAssignable, Category = "QTE|Delegates")
    FOnQTEFailed OnQTEFailed;

    UPROPERTY(BlueprintAssignable, Category = "QTE|Delegates")
    FOnQTEProgress OnQTEProgress;

    UFUNCTION(BlueprintCallable, Category = "QTE", meta = (DisplayName = "Start QTE (Data Asset)"))
    FName StartQTE(const UQTEDataAsset* QTEDataAsset, FName OptionalInstanceID = NAME_None);

    UFUNCTION(BlueprintCallable, Category = "QTE")
    bool CancelActiveQTE();

protected:

private:

    void HandleQTEInputTriggered(const FInputActionInstance& Instance, FName InstanceID);
    void HandleQTEInputStarted(const FInputActionInstance& Instance, FName InstanceID);
    void HandleQTEInputCompleted(const FInputActionInstance& Instance, FName InstanceID);
    void HandleQTEInputCanceled(const FInputActionInstance& Instance, FName InstanceID);
    void HandleQTEFailInputTriggered(const FInputActionInstance& Instance, FName InstanceID);

    void EndQTE(bool bSuccess);

    void TimeoutQTE();

    FTimerHandle QTEDurationTimerHandle;

    FActiveQTEInfo ActiveQTEInfo;

    bool bIsQTEActive = false;

    int32 MashCount = 0;

    float HoldStartTime = 0.0f;

    TArray<uint32> InputBindingHandles;

    UPROPERTY(EditDefaultsOnly, Category = "QTE|Input")
    TObjectPtr<UInputMappingContext> QTE_InputMappingContext;

    UPROPERTY()
    TObjectPtr<class UEnhancedInputLocalPlayerSubsystem> InputSubsystem = nullptr;

    uint64 UniqueIDCounter = 0;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UUserWidget>> ActiveQTEWidgets;
};
