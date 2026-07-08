// GTAI_NotificationTypes.h
// Data + enums for the notification system. See design doc section 9.
// Under namespace GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAI_UI.h"
#include "GTAI_NotificationTypes.generated.h"

UENUM(BlueprintType)
enum class EGTAINotificationType : uint8
{
    IncomingCall UMETA(DisplayName = "Incoming Call"),
    TextMessage  UMETA(DisplayName = "Text Message"),
    Alert        UMETA(DisplayName = "Alert"),
    MissionUpdate UMETA(DisplayName = "Mission Update"),
    Reward       UMETA(DisplayName = "Reward"),
    System       UMETA(DisplayName = "System"),
    Achievement  UMETA(DisplayName = "Achievement")
};

UENUM(BlueprintType)
enum class EGTAINotificationPriority : uint8
{
    Low, Normal, High, Critical
};

UCLASS(BlueprintType)
class GTAIUI_API UGTAI_NotificationData : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GTAI|UI|Notify")
    EGTAINotificationType Type = EGTAINotificationType::Alert;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GTAI|UI|Notify")
    EGTAINotificationPriority Priority = EGTAINotificationPriority::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GTAI|UI|Notify")
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GTAI|UI|Notify")
    FText Body;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GTAI|UI|Notify")
    TSoftObjectPtr<UTexture2D> Icon;

    /** Seconds before auto-dismiss (0 = until dismissed / interactive). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GTAI|UI|Notify")
    float Duration = 4.f;

    /** Interactive toasts (e.g. calls) linger and raise OnActivated. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GTAI|UI|Notify")
    bool bInteractive = false;

    /** Optional payload (contact id, thread id, mission id). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GTAI|UI|Notify")
    FString Payload;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAIOnNotificationActivated, UGTAI_NotificationData*, Data);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|UI|Notify")
    FGTAIOnNotificationActivated OnActivated;
};
