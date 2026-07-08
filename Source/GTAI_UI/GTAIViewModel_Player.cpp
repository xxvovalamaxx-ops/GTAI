// Copyright GTAI. All Rights Reserved.
// VISTA — Player ViewModel (MVVM).
// FieldNotify-backed player vitals/inventory model. Widgets bind to these
// getters and receive change events ONLY when a value actually changes, so
// the HUD never polls per-frame. Setters clamp/stage values and broadcast the
// minimal set of affected fields. Under namespace GTAI::UI.
// See design doc sections 1.3, 2.2-2.5, 2.8.

#include "GTAIViewModel_Player.h"

// ───────────────────────── Health ─────────────────────────
void UGTAIViewModel_Player::SetHealth(float InValue)
{
    const float Clamped = FMath::Clamp(InValue, 0.f, MaxHealth);
    if (FMath::IsNearlyEqual(Health, Clamped))
    {
        return;
    }

    Health = Clamped;
    BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, Health));
    BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, GetHealthPercent));
    BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, IsLowHealth));
}

// ───────────────────────── Armor ─────────────────────────
void UGTAIViewModel_Player::SetArmor(float InValue)
{
    const float Clamped = FMath::Clamp(InValue, 0.f, MaxArmor);
    if (FMath::IsNearlyEqual(Armor, Clamped))
    {
        return;
    }

    Armor = Clamped;
    BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, Armor));
    BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, GetArmorPercent));
}

// ───────────────────────── Money ─────────────────────────
void UGTAIViewModel_Player::SetCash(int32 InValue)
{
    if (Cash == InValue)
    {
        return;
    }

    Cash = InValue;
    BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, Cash));
    BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, GetCashString));
}

FText UGTAIViewModel_Player::GetCashString() const
{
    // "$12,450" style formatting with locale grouping separators.
    return FText::FromString(FString::Printf(TEXT("$%s"), *FString::FormatAsNumber(Cash)));
}

// ───────────────────────── Weapon ─────────────────────────
void UGTAIViewModel_Player::SetWeaponName(const FText& InName)
{
    if (WeaponName.EqualTo(InName))
    {
        return;
    }

    WeaponName = InName;
    BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, WeaponName));
}

void UGTAIViewModel_Player::SetAmmo(int32 Current, int32 Max)
{
    const FText NewString = FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::Max(0, Current), FMath::Max(0, Max)));
    if (AmmoString.EqualTo(NewString))
    {
        return;
    }

    AmmoString = NewString;
    BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, GetAmmoString));
}
