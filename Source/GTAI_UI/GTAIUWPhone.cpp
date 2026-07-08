// Copyright GTAI. All Rights Reserved.
// VISTA — In-game smartphone (iOS/Android hybrid).
// A UCommonActivatableWidget pushed onto the UI stack. Owns a status bar, a
// home screen (UGTAIUWPhoneHome), and an activatable back-stack of app screens
// (Contacts, Messages, Camera, Map, ...). Navigation: OpenApp pushes an app
// screen and activates it; GoHome/back returns to the home screen. Under
// namespace GTAI::UI. See design doc section 3.

#include "GTAIUWPhone.h"

#include "Engine/World.h"
#include "TimerManager.h"

#include "GTAIUWAppBase.h"
#include "GTAIUWPhoneHome.h"

// ───────────────────────── Lifecycle ─────────────────────────
void UGTAIUWPhone::NativeConstruct()
{
    Super::NativeConstruct();

    // Seed the home screen into the switcher if it exists already in the WBP.
    if (HomeScreen && AppSwitcher)
    {
        const int32 Idx = AppSwitcher->FindChildIndex(HomeScreen);
        if (Idx != INDEX_NONE)
        {
            AppSwitcher->SetActiveWidgetIndex(Idx);
        }
    }
}

void UGTAIUWPhone::NativeOnActivated()
{
    Super::NativeOnActivated();

    // Refresh the status-bar clock immediately and start a 1 Hz ticker.
    auto UpdateClock = [this]()
    {
        if (ClockLabel)
        {
            const FString Time = FPlatformTime::StrTimestamp().RightChop(11).Left(8);
            ClockLabel->SetText(FText::FromString(Time));
        }
    };

    UpdateClock();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            ClockTimerHandle,
            MoveTemp(UpdateClock),
            1.f, true);
    }
}

void UGTAIUWPhone::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }
}

// ───────────────────────── App navigation ─────────────────────────
void UGTAIUWPhone::OpenApp(TSubclassOf<UGTAIUWAppBase> AppClass)
{
    if (!AppClass || !AppSwitcher)
    {
        return;
    }

    // Reuse an existing instance of this app if it is still on the stack so
    // back-navigation and state are preserved.
    UGTAIUWAppBase* Existing = nullptr;
    for (UGTAIUWAppBase* App : AppStack)
    {
        if (App && App->GetClass() == AppClass)
        {
            Existing = App;
            break;
        }
    }

    UGTAIUWAppBase* App = Existing;
    if (!App)
    {
        App = CreateWidget<UGTAIUWAppBase>(this, AppClass);
        if (!App)
        {
            return;
        }
        AppSwitcher->AddChild(App);
        AppStack.Add(App);
    }

    AppSwitcher->SetActiveWidget(App);
    App->ActivateWidget();

    // Clear the badge for the opened app.
    if (!App->AppId.IsNone())
    {
        SetAppBadge(App->AppId, 0);
    }
}

void UGTAIUWPhone::GoHome()
{
    if (!AppSwitcher)
    {
        return;
    }

    // Pop every app off the stack back to the home screen.
    while (AppStack.Num() > 0)
    {
        UGTAIUWAppBase* App = AppStack.Pop();
        if (App)
        {
            App->DeactivateWidget();
        }
    }

    if (HomeScreen)
    {
        AppSwitcher->SetActiveWidget(HomeScreen);
    }
}

void UGTAIUWPhone::ClosePhone()
{
    // Deactivate us; the UI manager owns viewport removal / input-mode restore.
    DeactivateWidget();
}

void UGTAIUWPhone::SetAppBadge(FName AppId, int32 Count)
{
    if (AppId.IsNone())
    {
        return;
    }

    if (Count <= 0)
    {
        AppBadges.Remove(AppId);
    }
    else
    {
        AppBadges.FindOrAdd(AppId) = Count;
    }

    // Forward to the home screen so the dock/grid icon can show the badge.
    if (HomeScreen)
    {
        HomeScreen->SetAppBadge(AppId, Count);
    }
}
