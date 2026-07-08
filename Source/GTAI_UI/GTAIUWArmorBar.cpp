// Copyright GTAI. All Rights Reserved.
// VISTA — ArmorBar Widget

#include "GTAIUWArmorBar.h"
#include "GTAIViewModel_Player.h"
#include "Components/ProgressBar.h"

void UGTAIUWArmorBar::NativeConstruct() { Super::NativeConstruct(); }
void UGTAIUWArmorBar::BindViewModel(UGTAIViewModel_Player* VM)
{
    VM->OnArmorChanged.AddDynamic(this, &UGTAIUWArmorBar::OnArmorChanged);
}
void UGTAIUWArmorBar::OnArmorChanged(float Current, float Max)
{
    if (ArmorBar) ArmorBar->SetPercent(Max > 0 ? Current / Max : 0.f);
}
