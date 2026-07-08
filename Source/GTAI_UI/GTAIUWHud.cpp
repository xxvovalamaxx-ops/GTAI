// Copyright GTAI. All Rights Reserved.
// VISTA — Root HUD widget.
// Composes the persistent HUD sub-components and drives the ones that are not
// themselves MVVM-bound in Blueprint via a throttled (12 Hz) C++ bridge from
// the manager's shared ViewModels. Under namespace GTAI::UI.

#include "GTAIUWHud.h"

#include "GTAIUWHealthBar.h"
#include "GTAIUWArmorBar.h"
#include "GTAIUWWantedStars.h"
#include "GTAIUWMoneyCounter.h"
#include "GTAIUWSpeedometer.h"
#include "GTAIUWAmmoStatus.h"
#include "GTAIUWWorldStatus.h"
#include "GTAIUWRadar.h"
#include "GTAIUWWeaponWheel.h"
#include "GTAIViewModel_Player.h"
#include "GTAIViewModel_World.h"
#include "GTAIViewModel_Map.h"

void UGTAIUWHud::NativeConstruct()
{
    Super::NativeConstruct();

    // Bind sub-widgets to the shared ViewModels if a blueprint already created
    // them via the editor. C++-authored sub-widgets can also be created here if
    // the WBP_HUD omits them, but typically the designer owns the subtree.
    if (CachedPlayerVM && CachedWorldVM && CachedMapVM)
    {
        if (Radar)
        {
            Radar->BindViewModels(CachedWorldVM, CachedMapVM);
        }
    }
}

void UGTAIUWHud::BindViewModels(UGTAIViewModel_Player* PlayerVM, UGTAIViewModel_World* WorldVM, UGTAIViewModel_Map* MapVM)
{
    CachedPlayerVM = PlayerVM;
    CachedWorldVM  = WorldVM;
    CachedMapVM    = MapVM;

    if (Radar && CachedWorldVM && CachedMapVM)
    {
        Radar->BindViewModels(CachedWorldVM, CachedMapVM);
    }

    // Push initial values into the non-MVVM sub-widgets immediately so the HUD
    // is correct on the first frame before the throttled tick fires.
    if (CachedPlayerVM)
    {
        if (HealthBar) HealthBar->SetHealthPercent(CachedPlayerVM->GetHealthPercent());
        if (ArmorBar)
        {
            ArmorBar->SetArmorPercent(CachedPlayerVM->GetArmorPercent());
            ArmorBar->SetHasArmor(CachedPlayerVM->GetArmor() > KINDA_SMALL_NUMBER);
        }
        if (MoneyCounter) MoneyCounter->SetCash(CachedPlayerVM->GetCash());
    }

    if (CachedWorldVM)
    {
        if (WantedStars) WantedStars->SetWantedLevel(CachedWorldVM->GetWantedLevel());
    }
}

void UGTAIUWHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    HudUpdateAccumulator += InDeltaTime;
    if (HudUpdateAccumulator < HudUpdateInterval)
    {
        return;
    }
    HudUpdateAccumulator = 0.f;

    // Throttled C++ bridge for the imperative sub-widgets. Leaf widgets that
    // are MVVM-bound in the Blueprint (e.g. via View Bindings) update on
    // FieldNotify and are intentionally NOT touched here.
    if (CachedPlayerVM)
    {
        if (HealthBar) HealthBar->SetHealthPercent(CachedPlayerVM->GetHealthPercent());
        if (ArmorBar)
        {
            ArmorBar->SetArmorPercent(CachedPlayerVM->GetArmorPercent());
            ArmorBar->SetHasArmor(CachedPlayerVM->GetArmor() > KINDA_SMALL_NUMBER);
        }
        if (MoneyCounter) MoneyCounter->SetCash(CachedPlayerVM->GetCash());
    }

    if (CachedWorldVM)
    {
        if (WantedStars) WantedStars->SetWantedLevel(CachedWorldVM->GetWantedLevel());
    }
}

void UGTAIUWHud::ToggleWeaponWheel(bool bOpen)
{
    if (!WeaponWheel)
    {
        return;
    }

    if (bOpen)
    {
        WeaponWheel->Open();
    }
    else
    {
        WeaponWheel->Close();
    }
}
