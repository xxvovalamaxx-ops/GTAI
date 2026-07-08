// GTAIUWAmmoStatus.h
// Weapon name + ammo "current/max" + reload state. Hidden when unarmed.
// See design doc 2.8. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWAmmoStatus.generated.h"

UCLASS()
class GTAIUI_API UGTAIUWAmmoStatus : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> WeaponNameLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> AmmoLabel;

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetWeapon(const FText& Name, int32 Current, int32 Max);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetReloading(bool bReloading);
};
