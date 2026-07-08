// GTAIUWHud.h
// Root HUD widget. Composes the persistent HUD sub-components and binds them to
// the shared Player/World/Maps ViewModels. Non-modal; lives beneath menus and
// the notification layer. See design doc section 2. Under namespace GTAI::UI.
#pragma once

#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWHud.generated.h"

class UGTAIUWHealthBar;
class UGTAIUWArmorBar;
class UGTAIUWWantedStars;
class UGTAIUWMoneyCounter;
class UGTAIUWSpeedometer;
class UGTAIUWAmmoStatus;
class UGTAIUWWorldStatus;
class UGTAIUWRadar;
class UGTAIUWWeaponWheel;
class UGTAIViewModel_Player;
class UGTAIViewModel_World;
class UGTAIViewModel_Map;

UCLASS()
class GTAIUI_API UGTAIUWHud : public UGTAIUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    /** Wire sub-widgets to the manager's shared ViewModels. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void BindViewModels(UGTAIViewModel_Player* PlayerVM, UGTAIViewModel_World* WorldVM, UGTAIViewModel_Map* MapVM);

    /** Toggle the weapon wheel overlay (gamepad stick / hotkey). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void ToggleWeaponWheel(bool bOpen);

protected:
    // BindWidget sub-widgets (authored in WBP_HUD).
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWHealthBar> HealthBar;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWArmorBar> ArmorBar;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWWantedStars> WantedStars;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWMoneyCounter> MoneyCounter;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWSpeedometer> Speedometer;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWAmmoStatus> AmmoStatus;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWWorldStatus> WorldStatus;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWRadar> Radar;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UGTAIUWWeaponWheel> WeaponWheel;
};
