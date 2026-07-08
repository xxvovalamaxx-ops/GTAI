// Copyright GTAI. All Rights Reserved.
// VISTA — Menu & Settings stubs

#include "GTAIUWMainMenu.h"
void UGTAIUWMainMenu::Show() { SetVisibility(ESlateVisibility::Visible); }

#include "GTAIUWPauseMenu.h"
void UGTAIUWPauseMenu::Show() { SetVisibility(ESlateVisibility::Visible); }

#include "GTAIUWSettings.h"
void UGTAIUWSettings::Show() { SetVisibility(ESlateVisibility::Visible); }

#include "GTAIUWNotificationLayer.h"
void UGTAIUWNotificationLayer::ShowNotification(const FString& Msg) { /* TODO */ }
