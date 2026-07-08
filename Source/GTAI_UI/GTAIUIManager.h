// GTAIUIManager.h
// Central UI subsystem. Owns the root widget layers, the shared ViewModels,
// DPI/application-scale control, the input-mode switch (gameplay <-> UI),
// and the notification queue. Spawned once per game instance.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_UI.h"
#include "GTAIUIManager.generated.h"

class UGTAIUserWidget;
class UGTAIUWHud;
class UGTAIUWPhone;
class UGTAIUWNotificationLayer;
class UGTAIUWPauseMenu;
class UGTAIViewModel_Player;
class UGTAIViewModel_World;
class UGTAIViewModel_Map;
class APlayerController;

UENUM(BlueprintType)
enum class EGTAIInputMode : uint8
{
    Gameplay,   // gameplay input live, HUD visible
    UI,         // modal UI (menus/phone), gameplay suppressed
    Cinematic   // UI hidden/locked
};

UCLASS()
class GTAIUI_API UGTAIUIManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // USubsystem interface.
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Build the persistent HUD + notification layers and register ViewModels. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    void InitializeUI(APlayerController* PC);

    /** Returns the singleton manager for the given world. */
    static UGTAIUIManager* Get(const UObject* WorldContext);

    // ---- Layers ----
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    UGTAIUWHud* GetHUD() const { return HUD; }

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    UGTAIUWNotificationLayer* GetNotificationLayer() const { return NotificationLayer; }

    /** Push a full-screen UI layer (menu/phone) and switch input mode. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    void PushUI(UGTAIUserWidget* Widget, EGTAIInputMode Mode = EGTAIInputMode::UI);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    void PopUI(UGTAIUserWidget* Widget);

    /** Opens the phone (idempotent). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    void OpenPhone();

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    void ClosePhone();

    /** Opens the pause menu. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    void OpenPauseMenu();

    // ---- ViewModels (shared, bound by widgets) ----
    UGTAIViewModel_Player* GetPlayerVM() const { return PlayerVM; }
    UGTAIViewModel_World*  GetWorldVM()  const { return WorldVM; }
    UGTAIViewModel_Map*    GetMapVM()    const { return MapVM; }

    // ---- Input mode ----
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    void SetInputMode(EGTAIInputMode Mode);

    EGTAIInputMode GetInputMode() const { return CurrentMode; }

    /** Convenience: push a toast through the notification layer. */
    void Notify(class UGTAI_NotificationData* Data);

protected:
    UPROPERTY()
    TObjectPtr<UGTAIUWHud> HUD;

    UPROPERTY()
    TObjectPtr<UGTAIUWPhone> Phone;

    UPROPERTY()
    TObjectPtr<UGTAIUWNotificationLayer> NotificationLayer;

    UPROPERTY()
    TObjectPtr<UGTAIUWPauseMenu> PauseMenu;

    UPROPERTY()
    TObjectPtr<UGTAIViewModel_Player> PlayerVM;

    UPROPERTY()
    TObjectPtr<UGTAIViewModel_World> WorldVM;

    UPROPERTY()
    TObjectPtr<UGTAIViewModel_Map> MapVM;

    UPROPERTY()
    TObjectPtr<APlayerController> OwningPC;

    EGTAIInputMode CurrentMode = EGTAIInputMode::Gameplay;
};
