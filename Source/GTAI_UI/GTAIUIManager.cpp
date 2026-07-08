// Copyright GTAI. All Rights Reserved.
// VISTA — Central UI subsystem.
// Owns the persistent HUD + notification layers, the shared ViewModels, the
// UI/input-mode stack (PushUI/PopUI), and the notification passthrough.
// Under namespace GTAI::UI. See AGENTS.md rule 5 and the ui-system-design skill.

#include "GTAIUIManager.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "GTAIUWHud.h"
#include "GTAIUWPhone.h"
#include "GTAIUWNotificationLayer.h"
#include "GTAIUWPauseMenu.h"
#include "GTAI_NotificationTypes.h"
#include "GTAIViewModel_Player.h"
#include "GTAIViewModel_World.h"
#include "GTAIViewModel_Map.h"

namespace GTAI::UI
{
    static const FName NAME_UIStack(TEXT("UIStack"));
}

void UGTAIUIManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Allocate the shared ViewModels once per game instance so every widget
    // binds to the same instances (event-driven FieldNotify updates).
    PlayerVM = NewObject<UGTAIViewModel_Player>(this, TEXT("VM_Player"));
    WorldVM  = NewObject<UGTAIViewModel_World>(this, TEXT("VM_World"));
    MapVM    = NewObject<UGTAIViewModel_Map>(this, TEXT("VM_Map"));

    CurrentMode = EGTAIInputMode::Gameplay;
    UIStack.Empty();
}

void UGTAIUIManager::Deinitialize()
{
    if (HUD)        { HUD->RemoveFromParent();        HUD = nullptr; }
    if (Phone)      { Phone->RemoveFromParent();      Phone = nullptr; }
    if (PauseMenu)  { PauseMenu->RemoveFromParent();  PauseMenu = nullptr; }
    if (NotificationLayer)
    {
        NotificationLayer->RemoveFromParent();
        NotificationLayer = nullptr;
    }

    HUD = nullptr;
    Phone = nullptr;
    PauseMenu = nullptr;
    NotificationLayer = nullptr;
    PlayerVM = nullptr;
    WorldVM = nullptr;
    MapVM = nullptr;
    OwningPC = nullptr;
    UIStack.Empty();

    Super::Deinitialize();
}

UGTAIUIManager* UGTAIUIManager::Get(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        return nullptr;
    }

    const UGameInstance* GI = WorldContext->GetWorld()
        ? WorldContext->GetWorld()->GetGameInstance()
        : Cast<UGameInstance>(WorldContext->GetOuter());

    return GI ? GI->GetSubsystem<UGTAIUIManager>() : nullptr;
}

void UGTAIUIManager::InitializeUI(APlayerController* PC)
{
    if (!PC)
    {
        return;
    }

    OwningPC = PC;

    // --- HUD (persistent, beneath menus) ---
    if (!HUD && HUDClass)
    {
        HUD = CreateWidget<UGTAIUWHud>(PC, HUDClass);
        if (HUD)
        {
            HUD->AddToViewport(0);
            HUD->BindViewModels(PlayerVM, WorldVM, MapVM);
        }
    }

    // --- Notification layer (top-most, non-modal) ---
    if (!NotificationLayer && NotificationLayerClass)
    {
        NotificationLayer = CreateWidget<UGTAIUWNotificationLayer>(PC, NotificationLayerClass);
        if (NotificationLayer)
        {
            NotificationLayer->AddToViewport(100);
        }
    }

    // --- Phone (owned but hidden until opened) ---
    if (!Phone && PhoneClass)
    {
        Phone = CreateWidget<UGTAIUWPhone>(PC, PhoneClass);
    }

    // --- Pause menu (owned but hidden until opened) ---
    if (!PauseMenu && PauseMenuClass)
    {
        PauseMenu = CreateWidget<UGTAIUWPauseMenu>(PC, PauseMenuClass);
    }

    SetInputMode(EGTAIInputMode::Gameplay);
}

void UGTAIUIManager::PushUI(UGTAIUserWidget* Widget, EGTAIInputMode Mode)
{
    if (!Widget || !OwningPC)
    {
        return;
    }

    // Idempotent: do not push the same widget twice.
    if (UIStack.Contains(Widget))
    {
        SetInputMode(Mode);
        return;
    }

    // Full-screen modal widgets go above the HUD (z-order 50); the phone is a
    // UCommonActivatableWidget and self-manages its own viewport slot.
    if (!Widget->IsInViewport())
    {
        Widget->AddToViewport(50);
    }

    UIStack.Add(Widget);
    SetInputMode(Mode);
}

void UGTAIUIManager::PopUI(UGTAIUserWidget* Widget)
{
    if (!Widget)
    {
        return;
    }

    const int32 Idx = UIStack.IndexOfByKey(Widget);
    if (Idx == INDEX_NONE)
    {
        return;
    }

    UIStack.RemoveAt(Idx);
    Widget->RemoveFromParent();

    // Restore the input mode of whatever is now on top (or Gameplay at root).
    const EGTAIInputMode Restored = (UIStack.Num() > 0)
        ? EGTAIInputMode::UI
        : EGTAIInputMode::Gameplay;
    SetInputMode(Restored);
}

void UGTAIUIManager::OpenPhone()
{
    if (!Phone || !OwningPC)
    {
        return;
    }

    if (Phone->IsInViewport())
    {
        return; // idempotent
    }

    Phone->AddToViewport(60);
    PushUI(Phone, EGTAIInputMode::UI);
}

void UGTAIUIManager::ClosePhone()
{
    if (!Phone)
    {
        return;
    }

    PopUI(Phone);
}

void UGTAIUIManager::OpenPauseMenu()
{
    if (!PauseMenu || !OwningPC)
    {
        return;
    }

    if (PauseMenu->IsInViewport())
    {
        return;
    }

    PauseMenu->AddToViewport(70);
    PushUI(PauseMenu, EGTAIInputMode::UI);
}

void UGTAIUIManager::SetInputMode(EGTAIInputMode Mode)
{
    if (!OwningPC || Mode == CurrentMode)
    {
        return;
    }

    CurrentMode = Mode;

    const FInputModeGameAndUI GameAndUI = []()
    {
        FInputModeGameAndUI IM;
        IM.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        IM.SetHideCursorDuringCapture(false);
        return IM;
    }();

    switch (Mode)
    {
    case EGTAIInputMode::Gameplay:
        // Gameplay live, cursor hidden; HUD visible.
        OwningPC->SetInputMode(FInputModeGameOnly());
        OwningPC->bShowMouseCursor = false;
        break;

    case EGTAIInputMode::UI:
        // Modal UI: game still receives look but cursor is free for menus.
        OwningPC->SetInputMode(GameAndUI);
        OwningPC->bShowMouseCursor = true;
        break;

    case EGTAIInputMode::Cinematic:
        // UI locked/hidden; suppress all input.
        OwningPC->SetInputMode(FInputModeUIOnly());
        OwningPC->bShowMouseCursor = false;
        break;

    default:
        OwningPC->SetInputMode(FInputModeGameOnly());
        break;
    }
}

void UGTAIUIManager::Notify(UGTAI_NotificationData* Data)
{
    if (NotificationLayer && Data)
    {
        NotificationLayer->Push(Data);
    }
}
