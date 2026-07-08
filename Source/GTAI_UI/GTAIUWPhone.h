// GTAIUWPhone.h
// In-game smartphone (iOS/Android hybrid). A UCommonActivatableWidget pushed
// onto the UI stack. Owns a status bar, app grid/dock, and an activatable
// back-stack of app screens. See design doc section 3. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWPhone.generated.h"

class UGTAIUWPhoneHome;
class UGTAIUWAppBase;
class UGTAIUWNotificationLayer;

UCLASS()
class GTAIUI_API UGTAIUWPhone : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;

    /** Launch a specific app by class (Contacts, Messages, Map, ...). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Phone")
    void OpenApp(TSubclassOf<UGTAIUWAppBase> AppClass);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Phone")
    void GoHome();

    /** Close the phone entirely. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Phone")
    void ClosePhone();

    /** Set the unread badge count on the Messages/Phone dock icons. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Phone")
    void SetAppBadge(FName AppId, int32 Count);

protected:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWPhoneHome> HomeScreen;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UWidgetSwitcher> AppSwitcher;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> ClockLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UImage> BatteryIcon;

    /** Stack of active app widgets for back-navigation. */
    UPROPERTY()
    TArray<TObjectPtr<UGTAIUWAppBase>> AppStack;

    /** Unread badge counts keyed by AppId (e.g. "Messages" -> 3). */
    UPROPERTY()
    TMap<FName, int32> AppBadges;
};
