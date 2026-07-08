// GTAIUWNotificationLayer.h
// Top-most, non-modal layer owning a priority queue + toast pool. Always above
// the HUD, below modals. Respects Do-Not-Disturb (silent types routed to a
// missed-badge instead of a toast). See design doc section 9. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWNotificationLayer.generated.h"

class UGTAI_NotificationData;
class UGTAIUWToast;
class UVerticalBox;

UCLASS()
class GTAIUI_API UGTAIUWNotificationLayer : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Enqueue a notification (auto-routed per DND + priority). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Notify")
    void Push(UGTAI_NotificationData* Data);

    /** Pre-allocate toast pool. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Notify")
    int32 MaxConcurrentToasts = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Notify")
    int32 PoolSize = 6;

    /** When true, silent/low notifications are suppressed to a badge. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Notify")
    bool bDoNotDisturb = false;

protected:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> ToastContainer;

    /** FIFO overflow queue. */
    UPROPERTY()
    TArray<TObjectPtr<UGTAI_NotificationData>> PendingQueue;

    UPROPERTY()
    TArray<TObjectPtr<UGTAIUWToast>> ToastPool;

    UGTAIUWToast* AcquireToast();
    void ReleaseToast(UGTAIUWToast* Toast);

    int32 ActiveCount = 0;
};
