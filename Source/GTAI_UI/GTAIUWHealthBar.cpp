// Copyright GTAI. All Rights Reserved.
// VISTA — UI Widget Batch: HealthBar, ArmorBar, AmmoStatus, MoneyCounter, WantedStars, Speedometer, WorldStatus, Toast

#include "GTAIUWHealthBar.h"
#include "GTAIViewModel_Player.h"
#include "Components/ProgressBar.h"

void UGTAIUWHealthBar::NativeConstruct() { Super::NativeConstruct(); }
void UGTAIUWHealthBar::BindViewModel(UGTAIViewModel_Player* VM)
{
    VM->OnHealthChanged.AddDynamic(this, &UGTAIUWHealthBar::OnHealthChanged);
}
void UGTAIUWHealthBar::OnHealthChanged(float Current, float Max)
{
    if (HealthBar) HealthBar->SetPercent(Max > 0 ? Current / Max : 0.f);
}
